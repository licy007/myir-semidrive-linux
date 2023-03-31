/*
 * (c) 2017 Stefano Stabellini <stefano@aporeto.com>
 * (c) 2020 Semidriver Semiconductor Inc.
 * License terms:  GNU General Public License (GPL), version 2
 */


#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <xen/events.h>
#include <xen/grant_table.h>
#include <xen/xen.h>
#include <xen/xenbus.h>
#include <xen/interface/io/pvcalls.h>
#include <linux/soc/semidrive/mb_msg.h>
#include <linux/soc/semidrive/ipcc.h>
#include <linux/soc/semidrive/xenipc.h>
#include <linux/soc/semidrive/rpmsg_front.h>

#define RPCALLS_INVALID_ID UINT_MAX
#define RPCALLS_RING_ORDER XENBUS_MAX_RING_GRANT_ORDER
#define RPCALLS_NR_RSP_PER_RING __CONST_RING_SIZE(xen_pvcalls, XEN_PAGE_SIZE)
#define RPCALLS_FRONT_MAX_SPIN 5000

struct rpcalls_bedata {
	struct xen_pvcalls_front_ring ring;
	grant_ref_t ref;
	int irq;

	struct list_head socket_mappings;
	spinlock_t socket_lock;

	wait_queue_head_t inflight_req;
	struct xen_pvcalls_response rsp[RPCALLS_NR_RSP_PER_RING];
};
/* Only one front/back connection supported. */
static struct xenbus_device *rpmsg_front_dev;
static atomic_t rpcalls_refcount;

/* first increment refcount, then proceed */
#define rpcalls_enter() {               \
	atomic_inc(&rpcalls_refcount);      \
}

/* first complete other operations, then decrement refcount */
#define rpcalls_exit() {                \
	atomic_dec(&rpcalls_refcount);      \
}

struct sock_mapping {
	bool active_socket;
	struct list_head list;
	struct socket *sock;
	atomic_t refcount;
	union {
		struct {
			int irq;
			grant_ref_t ref;
			struct pvcalls_data_intf *ring;
			struct pvcalls_data data;
			struct mutex in_mutex;
			struct mutex out_mutex;

			wait_queue_head_t inflight_conn_req;
		} active;
		struct {
		/*
		 * Socket status, needs to be 64-bit aligned due to the
		 * test_and_* functions which have this requirement on arm64.
		 */
#define RPCALLS_STATUS_UNINITALIZED  0
#define RPCALLS_STATUS_BIND          1
#define RPCALLS_STATUS_LISTEN        2
			uint8_t status __attribute__((aligned(8)));
		/*
		 * Internal state-machine flags.
		 * Only one accept operation can be inflight for a socket.
		 * Only one poll operation can be inflight for a given socket.
		 * flags needs to be 64-bit aligned due to the test_and_*
		 * functions which have this requirement on arm64.
		 */
#define RPCALLS_FLAG_ACCEPT_INFLIGHT 0
#define RPCALLS_FLAG_POLL_INFLIGHT   1
#define RPCALLS_FLAG_POLL_RET        2
			uint8_t flags __attribute__((aligned(8)));
			uint32_t inflight_req_id;
			struct sock_mapping *accept_map;
			wait_queue_head_t inflight_accept_req;
		} passive;
	};
};

static inline struct sock_mapping *rpcalls_enter_sock(struct socket *sock)
{
	struct sock_mapping *map;

	if (!rpmsg_front_dev ||
		dev_get_drvdata(&rpmsg_front_dev->dev) == NULL)
		return ERR_PTR(-ENOTCONN);

	map = (struct sock_mapping *)sock->sk->sk_send_head;
	if (map == NULL)
		return ERR_PTR(-ENOTSOCK);

	rpcalls_enter();
	atomic_inc(&map->refcount);
	return map;
}

static inline void rpcalls_exit_sock(struct socket *sock)
{
	struct sock_mapping *map;

	map = (struct sock_mapping *)sock->sk->sk_send_head;
	atomic_dec(&map->refcount);
	rpcalls_exit();
}

static inline int get_request(struct rpcalls_bedata *bedata, int *req_id)
{
	*req_id = bedata->ring.req_prod_pvt & (RING_SIZE(&bedata->ring) - 1);
	if (RING_FULL(&bedata->ring) ||
	    bedata->rsp[*req_id].req_id != RPCALLS_INVALID_ID)
		return -EAGAIN;
	return 0;
}

static bool rpmsg_front_write_todo(struct sock_mapping *map)
{
	struct pvcalls_data_intf *intf = map->active.ring;
	RING_IDX cons, prod, size = XEN_FLEX_RING_SIZE(RPCALLS_RING_ORDER);
	int32_t error;

	error = intf->out_error;
	if (error == -ENOTCONN)
		return false;
	if (error != 0)
		return true;

	cons = intf->out_cons;
	prod = intf->out_prod;
	return !!(size - pvcalls_queued(prod, cons, size));
}

static bool rpmsg_front_read_todo(struct sock_mapping *map)
{
	struct pvcalls_data_intf *intf = map->active.ring;
	RING_IDX cons, prod;
	int32_t error;

	cons = intf->in_cons;
	prod = intf->in_prod;
	error = intf->in_error;
	return (error != 0 ||
		pvcalls_queued(prod, cons,
			       XEN_FLEX_RING_SIZE(RPCALLS_RING_ORDER)) != 0);
}

static irqreturn_t rpmsg_front_event_handler(int irq, void *dev_id)
{
	struct xenbus_device *dev = dev_id;
	struct rpcalls_bedata *bedata;
	struct xen_pvcalls_response *rsp;
	uint8_t *src, *dst;
	int req_id = 0, more = 0, done = 0;

	if (dev == NULL)
		return IRQ_HANDLED;

	rpcalls_enter();
	bedata = dev_get_drvdata(&dev->dev);
	if (bedata == NULL) {
		rpcalls_exit();
		return IRQ_HANDLED;
	}

again:
	while (RING_HAS_UNCONSUMED_RESPONSES(&bedata->ring)) {
		rsp = RING_GET_RESPONSE(&bedata->ring, bedata->ring.rsp_cons);

		req_id = rsp->req_id;
		if (rsp->cmd == PVCALLS_POLL) {
			struct sock_mapping *map = (struct sock_mapping *)(uintptr_t)
						   rsp->u.poll.id;

			clear_bit(RPCALLS_FLAG_POLL_INFLIGHT,
				  (void *)&map->passive.flags);
			/*
			 * clear INFLIGHT, then set RET. It pairs with
			 * the checks at the beginning of
			 * rpmsg_front_poll_passive.
			 */
			smp_wmb();
			set_bit(RPCALLS_FLAG_POLL_RET,
				(void *)&map->passive.flags);
		} else {
			dst = (uint8_t *)&bedata->rsp[req_id] +
			      sizeof(rsp->req_id);
			src = (uint8_t *)rsp + sizeof(rsp->req_id);
			memcpy(dst, src, sizeof(*rsp) - sizeof(rsp->req_id));
			/*
			 * First copy the rest of the data, then req_id. It is
			 * paired with the barrier when accessing bedata->rsp.
			 */
			smp_wmb();
			bedata->rsp[req_id].req_id = req_id;
		}

		done = 1;
		bedata->ring.rsp_cons++;
	}

	RING_FINAL_CHECK_FOR_RESPONSES(&bedata->ring, more);
	if (more)
		goto again;
	if (done)
		wake_up(&bedata->inflight_req);
	rpcalls_exit();
	return IRQ_HANDLED;
}

static void rpmsg_front_free_map(struct rpcalls_bedata *bedata,
				   struct sock_mapping *map)
{
	int i;

	unbind_from_irqhandler(map->active.irq, map);

	spin_lock(&bedata->socket_lock);
	if (!list_empty(&map->list))
		list_del_init(&map->list);
	spin_unlock(&bedata->socket_lock);

	for (i = 0; i < (1 << RPCALLS_RING_ORDER); i++)
		gnttab_end_foreign_access(map->active.ring->ref[i], 0, 0);
	gnttab_end_foreign_access(map->active.ref, 0, 0);
	free_page((unsigned long)map->active.ring);

	kfree(map);
}

static irqreturn_t rpmsg_front_conn_handler(int irq, void *sock_map)
{
	struct sock_mapping *map = sock_map;

	if (map == NULL)
		return IRQ_HANDLED;

	wake_up_interruptible(&map->active.inflight_conn_req);

	return IRQ_HANDLED;
}

int rpmsg_front_socket(struct socket *sock)
{
	struct rpcalls_bedata *bedata;
	struct sock_mapping *map = NULL;
	struct xen_pvcalls_request *req;
	int notify, req_id, ret;

	/*
	 * only supports domain AF_RPMSG,
	 * type SOCK_SEQPACKET and protocol 0 sockets for now.
	 *
	 * Check socket type here, AF_RPMSG and protocol checks are done
	 * by the caller.
	 */
	if (sock->type != SOCK_SEQPACKET)
		return -EOPNOTSUPP;

	rpcalls_enter();
	if (!rpmsg_front_dev) {
		rpcalls_exit();
		return -EACCES;
	}
	bedata = dev_get_drvdata(&rpmsg_front_dev->dev);

	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (map == NULL) {
		rpcalls_exit();
		return -ENOMEM;
	}

	spin_lock(&bedata->socket_lock);

	ret = get_request(bedata, &req_id);
	if (ret < 0) {
		kfree(map);
		spin_unlock(&bedata->socket_lock);
		rpcalls_exit();
		return ret;
	}

	/*
	 * sock->sk->sk_send_head is not used for ip sockets: reuse the
	 * field to store a pointer to the struct sock_mapping
	 * corresponding to the socket. This way, we can easily get the
	 * struct sock_mapping from the struct socket.
	 */
	sock->sk->sk_send_head = (void *)map;
	list_add_tail(&map->list, &bedata->socket_mappings);

	req = RING_GET_REQUEST(&bedata->ring, req_id);
	req->req_id = req_id;
	req->cmd = PVCALLS_SOCKET;
	req->u.socket.id = (uintptr_t) map;
	req->u.socket.domain = AF_RPMSG;
	req->u.socket.type = SOCK_SEQPACKET;
	req->u.socket.protocol = 0;

	bedata->ring.req_prod_pvt++;
	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&bedata->ring, notify);
	spin_unlock(&bedata->socket_lock);
	if (notify)
		notify_remote_via_irq(bedata->irq);

	wait_event(bedata->inflight_req,
		   READ_ONCE(bedata->rsp[req_id].req_id) == req_id);

	/* read req_id, then the content */
	smp_rmb();
	ret = bedata->rsp[req_id].ret;
	bedata->rsp[req_id].req_id = RPCALLS_INVALID_ID;

	rpcalls_exit();
	return ret;
}

static void free_active_ring(struct sock_mapping *map)
{
	if (!map->active.ring)
		return;

	free_pages((unsigned long)map->active.data.in,
			map->active.ring->ring_order);
	free_page((unsigned long)map->active.ring);
}

static int alloc_active_ring(struct sock_mapping *map)
{
	void *bytes;

	map->active.ring = (struct pvcalls_data_intf *)
		get_zeroed_page(GFP_KERNEL);
	if (!map->active.ring)
		goto out;

	map->active.ring->ring_order = RPCALLS_RING_ORDER;
	bytes = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO,
					RPCALLS_RING_ORDER);
	if (!bytes)
		goto out;

	map->active.data.in = bytes;
	map->active.data.out = bytes +
		XEN_FLEX_RING_SIZE(RPCALLS_RING_ORDER);

	return 0;

out:
	free_active_ring(map);
	return -ENOMEM;
}

static int create_active(struct sock_mapping *map, int *evtchn)
{
	void *bytes;
	int ret = -ENOMEM, irq = -1, i;

	*evtchn = -1;
	init_waitqueue_head(&map->active.inflight_conn_req);

	bytes = map->active.data.in;
	for (i = 0; i < (1 << RPCALLS_RING_ORDER); i++)
		map->active.ring->ref[i] = gnttab_grant_foreign_access(
			rpmsg_front_dev->otherend_id,
			pfn_to_gfn(virt_to_pfn(bytes) + i), 0);

	map->active.ref = gnttab_grant_foreign_access(
		rpmsg_front_dev->otherend_id,
		pfn_to_gfn(virt_to_pfn((void *)map->active.ring)), 0);

	ret = xenbus_alloc_evtchn(rpmsg_front_dev, evtchn);
	if (ret)
		goto out_error;
	irq = bind_evtchn_to_irqhandler(*evtchn, rpmsg_front_conn_handler,
					0, "rpcalls-frontend", map);
	if (irq < 0) {
		ret = irq;
		goto out_error;
	}

	map->active.irq = irq;
	map->active_socket = true;
	mutex_init(&map->active.in_mutex);
	mutex_init(&map->active.out_mutex);

	return 0;

out_error:
	if (*evtchn >= 0)
		xenbus_free_evtchn(rpmsg_front_dev, *evtchn);
	return ret;
}

int rpmsg_front_connect(struct socket *sock, struct sockaddr *addr,
				int addr_len, int flags)
{
	struct rpcalls_bedata *bedata;
	struct sock_mapping *map = NULL;
	struct xen_pvcalls_request *req;
	int notify, req_id, ret, evtchn;

	if (addr->sa_family != AF_RPMSG || sock->type != SOCK_SEQPACKET)
		return -EOPNOTSUPP;

	map = rpcalls_enter_sock(sock);
	if (IS_ERR(map))
		return PTR_ERR(map);

	bedata = dev_get_drvdata(&rpmsg_front_dev->dev);

	ret = alloc_active_ring(map);
	if (ret < 0) {
		rpcalls_exit_sock(sock);
		return ret;
	}

	spin_lock(&bedata->socket_lock);
	ret = get_request(bedata, &req_id);
	if (ret < 0) {
		spin_unlock(&bedata->socket_lock);
		free_active_ring(map);
		rpcalls_exit_sock(sock);
		return ret;
	}
	ret = create_active(map, &evtchn);
	if (ret < 0) {
		spin_unlock(&bedata->socket_lock);
		free_active_ring(map);
		rpcalls_exit_sock(sock);
		return ret;
	}

	req = RING_GET_REQUEST(&bedata->ring, req_id);
	req->req_id = req_id;
	req->cmd = PVCALLS_CONNECT;
	req->u.connect.id = (uintptr_t)map;
	req->u.connect.len = addr_len;
	req->u.connect.flags = flags;
	req->u.connect.ref = map->active.ref;
	req->u.connect.evtchn = evtchn;
	memcpy(req->u.connect.addr, addr, sizeof(*addr));

	map->sock = sock;

	bedata->ring.req_prod_pvt++;
	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&bedata->ring, notify);
	spin_unlock(&bedata->socket_lock);

	if (notify)
		notify_remote_via_irq(bedata->irq);

	wait_event(bedata->inflight_req,
		   READ_ONCE(bedata->rsp[req_id].req_id) == req_id);

	/* read req_id, then the content */
	smp_rmb();
	ret = bedata->rsp[req_id].ret;
	bedata->rsp[req_id].req_id = RPCALLS_INVALID_ID;
	rpcalls_exit_sock(sock);
	return ret;
}

static bool __write_complete(struct sock_mapping *map)
{
	struct pvcalls_data_intf *intf = map->active.ring;
	RING_IDX cons, prod, size = XEN_FLEX_RING_SIZE(RPCALLS_RING_ORDER);
	int32_t error;

	error = intf->out_error;
	if (error == -ENOTCONN)
		return false;
	if (error != 0)
		return true;

	cons = intf->out_cons;
	prod = intf->out_prod;
	return pvcalls_queued(prod, cons, size) == 0;
}

static int __write_ring(struct pvcalls_data_intf *intf,
			struct pvcalls_data *data,
			struct iov_iter *msg_iter,
			int len)
{
	RING_IDX cons, prod, size, masked_prod, masked_cons;
	RING_IDX array_size = XEN_FLEX_RING_SIZE(RPCALLS_RING_ORDER);
	int32_t error;

	error = intf->out_error;
	if (error < 0)
		return error;
	cons = intf->out_cons;
	prod = intf->out_prod;
	/* read indexes before continuing */
	virt_mb();

	size = pvcalls_queued(prod, cons, array_size);
	if (size >= array_size)
		return -EINVAL;
	if (size == array_size)
		return 0;
	if (len > array_size - size)
		len = array_size - size;

	masked_prod = pvcalls_mask(prod, array_size);
	masked_cons = pvcalls_mask(cons, array_size);

	if (masked_prod < masked_cons) {
		len = copy_from_iter(data->out + masked_prod, len, msg_iter);
	} else {
		if (len > array_size - masked_prod) {
			int ret = copy_from_iter(data->out + masked_prod,
				       array_size - masked_prod, msg_iter);
			if (ret != array_size - masked_prod) {
				len = ret;
				goto out;
			}
			len = ret + copy_from_iter(data->out, len - ret, msg_iter);
		} else {
			len = copy_from_iter(data->out + masked_prod, len, msg_iter);
		}
	}
out:
	/* write to ring before updating pointer */
	virt_wmb();
	intf->out_prod += len;

	return len;
}

int rpmsg_front_sendmsg(struct socket *sock, struct msghdr *msg,
			  size_t len)
{
	struct sock_mapping *map;
	int sent, tot_sent = 0;
	int count = 0;

	map = rpcalls_enter_sock(sock);
	if (IS_ERR(map))
		return PTR_ERR(map);

	mutex_lock(&map->active.out_mutex);
	if ((msg->msg_flags & MSG_DONTWAIT) && !rpmsg_front_write_todo(map)) {
		mutex_unlock(&map->active.out_mutex);
		rpcalls_exit();
		return -EAGAIN;
	}
	if (len > INT_MAX)
		len = INT_MAX;

again:
	count++;
	sent = __write_ring(map->active.ring,
			    &map->active.data, &msg->msg_iter,
			    len);
	if (sent > 0) {
		len -= sent;
		tot_sent += sent;
		notify_remote_via_irq(map->active.irq);
	}
	if (sent >= 0 && len > 0 && count < RPCALLS_FRONT_MAX_SPIN)
		goto again;
	if (sent < 0)
		tot_sent = sent;

	/* blocking sendmsg, wait for reply */
	if (!(msg->msg_flags & MSG_DONTWAIT)) {
		wait_event_interruptible(map->active.inflight_conn_req,
					 __write_complete(map));
	}

	mutex_unlock(&map->active.out_mutex);
	rpcalls_exit();
	return tot_sent;
}

static int __read_ring(struct pvcalls_data_intf *intf,
		       struct pvcalls_data *data,
		       struct iov_iter *msg_iter,
		       size_t len, int flags)
{
	RING_IDX cons, prod, size, masked_prod, masked_cons;
	RING_IDX array_size = XEN_FLEX_RING_SIZE(RPCALLS_RING_ORDER);
	int32_t error;

	cons = intf->in_cons;
	prod = intf->in_prod;
	error = intf->in_error;
	/* get pointers before reading from the ring */
	virt_rmb();

	size = pvcalls_queued(prod, cons, array_size);
	masked_prod = pvcalls_mask(prod, array_size);
	masked_cons = pvcalls_mask(cons, array_size);

	/* the array size is 4096*8 */
	if (size < (array_size/2) && error == -EOVERFLOW)
		intf->in_error = 0;

	if (size == 0)
		return error ?: size;

	if (len > size)
		len = size;

	if (masked_prod > masked_cons) {
		len = copy_to_iter(data->in + masked_cons, len, msg_iter);
	} else {
		if (len > (array_size - masked_cons)) {
			int ret = copy_to_iter(data->in + masked_cons,
				     array_size - masked_cons, msg_iter);
			if (ret != array_size - masked_cons) {
				len = ret;
				goto out;
			}
			len = ret + copy_to_iter(data->in, len - ret, msg_iter);
		} else {
			len = copy_to_iter(data->in + masked_cons, len, msg_iter);
		}
	}
out:
	/* read data from the ring before increasing the index */
	virt_mb();
	if (!(flags & MSG_PEEK))
		intf->in_cons += len;

	return len;
}

int rpmsg_front_recvmsg(struct socket *sock, struct msghdr *msg, size_t len,
		     int flags)
{
	int ret;
	struct sock_mapping *map;

	if (flags & (MSG_CMSG_CLOEXEC|MSG_ERRQUEUE|MSG_OOB|MSG_TRUNC))
		return -EOPNOTSUPP;

	map = rpcalls_enter_sock(sock);
	if (IS_ERR(map))
		return PTR_ERR(map);

	mutex_lock(&map->active.in_mutex);
	if (len > XEN_FLEX_RING_SIZE(RPCALLS_RING_ORDER))
		len = XEN_FLEX_RING_SIZE(RPCALLS_RING_ORDER);

	while (!(flags & MSG_DONTWAIT) && !rpmsg_front_read_todo(map)) {
		/* Wait until we get data or the endpoint goes away */
		if (wait_event_interruptible(map->active.inflight_conn_req,
						 rpmsg_front_read_todo(map))) {
			ret = -ERESTARTSYS;
			goto read_ring_done;
		}
	}
	ret = __read_ring(map->active.ring, &map->active.data,
			  &msg->msg_iter, len, flags);

	if (ret > 0)
		notify_remote_via_irq(map->active.irq);
	if (ret == 0)
		ret = (flags & MSG_DONTWAIT) ? -EAGAIN : 0;
	if (ret == -ENOTCONN)
		ret = 0;

read_ring_done:
	mutex_unlock(&map->active.in_mutex);
	rpcalls_exit();
	return ret;
}

int rpmsg_front_bind(struct socket *sock, struct sockaddr *addr, int addr_len)
{
	struct rpcalls_bedata *bedata;
	struct sock_mapping *map = NULL;
	struct xen_pvcalls_request *req;
	int notify, req_id, ret;

	if (addr->sa_family != AF_RPMSG || sock->type != SOCK_SEQPACKET)
		return -EOPNOTSUPP;

	map = rpcalls_enter_sock(sock);
	if (IS_ERR(map))
		return PTR_ERR(map);
	bedata = dev_get_drvdata(&rpmsg_front_dev->dev);

	spin_lock(&bedata->socket_lock);
	ret = get_request(bedata, &req_id);
	if (ret < 0) {
		spin_unlock(&bedata->socket_lock);
		rpcalls_exit();
		return ret;
	}
	req = RING_GET_REQUEST(&bedata->ring, req_id);
	req->req_id = req_id;
	map->sock = sock;
	req->cmd = PVCALLS_BIND;
	req->u.bind.id = (uintptr_t)map;
	memcpy(req->u.bind.addr, addr, sizeof(*addr));
	req->u.bind.len = addr_len;

	init_waitqueue_head(&map->passive.inflight_accept_req);

	map->active_socket = false;

	bedata->ring.req_prod_pvt++;
	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&bedata->ring, notify);
	spin_unlock(&bedata->socket_lock);
	if (notify)
		notify_remote_via_irq(bedata->irq);

	wait_event(bedata->inflight_req,
		   READ_ONCE(bedata->rsp[req_id].req_id) == req_id);

	/* read req_id, then the content */
	smp_rmb();
	ret = bedata->rsp[req_id].ret;
	bedata->rsp[req_id].req_id = RPCALLS_INVALID_ID;

	map->passive.status = RPCALLS_STATUS_BIND;
	rpcalls_exit();
	return 0;
}

static unsigned int rpmsg_front_poll_passive(struct file *file,
					       struct rpcalls_bedata *bedata,
					       struct sock_mapping *map,
					       poll_table *wait)
{
	int notify, req_id, ret;
	struct xen_pvcalls_request *req;

	if (test_bit(RPCALLS_FLAG_ACCEPT_INFLIGHT,
		     (void *)&map->passive.flags)) {
		uint32_t req_id = READ_ONCE(map->passive.inflight_req_id);

		if (req_id != RPCALLS_INVALID_ID &&
		    READ_ONCE(bedata->rsp[req_id].req_id) == req_id)
			return POLLIN | POLLRDNORM;

		poll_wait(file, &map->passive.inflight_accept_req, wait);
		return 0;
	}

	if (test_and_clear_bit(RPCALLS_FLAG_POLL_RET,
			       (void *)&map->passive.flags))
		return POLLIN | POLLRDNORM;

	/*
	 * First check RET, then INFLIGHT. No barriers necessary to
	 * ensure execution ordering because of the conditional
	 * instructions creating control dependencies.
	 */

	if (test_and_set_bit(RPCALLS_FLAG_POLL_INFLIGHT,
			     (void *)&map->passive.flags)) {
		poll_wait(file, &bedata->inflight_req, wait);
		return 0;
	}

	spin_lock(&bedata->socket_lock);
	ret = get_request(bedata, &req_id);
	if (ret < 0) {
		spin_unlock(&bedata->socket_lock);
		return ret;
	}
	req = RING_GET_REQUEST(&bedata->ring, req_id);
	req->req_id = req_id;
	req->cmd = PVCALLS_POLL;
	req->u.poll.id = (uintptr_t) map;

	bedata->ring.req_prod_pvt++;
	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&bedata->ring, notify);
	spin_unlock(&bedata->socket_lock);
	if (notify)
		notify_remote_via_irq(bedata->irq);

	poll_wait(file, &bedata->inflight_req, wait);
	return 0;
}

static unsigned int rpmsg_front_poll_active(struct file *file,
					      struct rpcalls_bedata *bedata,
					      struct sock_mapping *map,
					      poll_table *wait)
{
	unsigned int mask = 0;
	int32_t in_error, out_error;
	struct pvcalls_data_intf *intf = map->active.ring;

	out_error = intf->out_error;
	in_error = intf->in_error;

	poll_wait(file, &map->active.inflight_conn_req, wait);
	if (rpmsg_front_write_todo(map))
		mask |= POLLOUT | POLLWRNORM;
	if (rpmsg_front_read_todo(map))
		mask |= POLLIN | POLLRDNORM;
	if (in_error != 0 || out_error != 0)
		mask |= POLLERR;

	return mask;
}

unsigned int rpmsg_front_poll(struct file *file, struct socket *sock,
			       poll_table *wait)
{
	struct rpcalls_bedata *bedata;
	struct sock_mapping *map;
	int ret;

	map = rpcalls_enter_sock(sock);
	if (IS_ERR(map))
		return POLLNVAL;
	bedata = dev_get_drvdata(&rpmsg_front_dev->dev);

	if (map->active_socket)
		ret = rpmsg_front_poll_active(file, bedata, map, wait);
	else
		ret = rpmsg_front_poll_passive(file, bedata, map, wait);
	rpcalls_exit();
	return ret;
}

int rpmsg_front_release(struct socket *sock)
{
	struct rpcalls_bedata *bedata;
	struct sock_mapping *map;
	int req_id, notify, ret;
	struct xen_pvcalls_request *req;

	if (sock->sk == NULL)
		return 0;

	map = rpcalls_enter_sock(sock);
	if (IS_ERR(map)) {
		if (PTR_ERR(map) == -ENOTCONN)
			return -EIO;
		else
			return 0;
	}
	bedata = dev_get_drvdata(&rpmsg_front_dev->dev);

	spin_lock(&bedata->socket_lock);
	ret = get_request(bedata, &req_id);
	if (ret < 0) {
		spin_unlock(&bedata->socket_lock);
		rpcalls_exit();
		return ret;
	}
	sock->sk->sk_send_head = NULL;

	req = RING_GET_REQUEST(&bedata->ring, req_id);
	req->req_id = req_id;
	req->cmd = PVCALLS_RELEASE;
	req->u.release.id = (uintptr_t)map;

	bedata->ring.req_prod_pvt++;
	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&bedata->ring, notify);
	spin_unlock(&bedata->socket_lock);
	if (notify)
		notify_remote_via_irq(bedata->irq);

	wait_event(bedata->inflight_req,
		   READ_ONCE(bedata->rsp[req_id].req_id) == req_id);

	if (map->active_socket) {
		/*
		 * Set in_error and wake up inflight_conn_req to force
		 * recvmsg waiters to exit.
		 */
		map->active.ring->in_error = -EBADF;
		wake_up_interruptible(&map->active.inflight_conn_req);

		/*
		 * We need to make sure that sendmsg/recvmsg on this socket have
		 * not started before we've cleared sk_send_head here. The
		 * easiest (though not optimal) way to guarantee this is to see
		 * that no pvcall (other than us) is in progress.
		 */
		while (atomic_read(&rpcalls_refcount) > 1)
			cpu_relax();

		rpmsg_front_free_map(bedata, map);
	} else {
		wake_up(&bedata->inflight_req);
		wake_up(&map->passive.inflight_accept_req);

		while (atomic_read(&map->refcount) > 1)
			cpu_relax();

		spin_lock(&bedata->socket_lock);
		list_del(&map->list);
		spin_unlock(&bedata->socket_lock);
		if (READ_ONCE(map->passive.inflight_req_id) != RPCALLS_INVALID_ID &&
			READ_ONCE(map->passive.inflight_req_id) != 0) {
			rpmsg_front_free_map(bedata,
					       map->passive.accept_map);
		}
		kfree(map);
	}
	WRITE_ONCE(bedata->rsp[req_id].req_id, RPCALLS_INVALID_ID);

	rpcalls_exit();
	return 0;
}

static const struct xenbus_device_id rpmsg_front_ids[] = {
	{ "rpcall" },
	{ "" }
};

static int rpmsg_front_remove(struct xenbus_device *dev)
{
	struct rpcalls_bedata *bedata;
	struct sock_mapping *map = NULL, *n;

	bedata = dev_get_drvdata(&rpmsg_front_dev->dev);
	dev_set_drvdata(&dev->dev, NULL);
	rpmsg_front_dev = NULL;
	if (bedata->irq >= 0)
		unbind_from_irqhandler(bedata->irq, dev);

	list_for_each_entry_safe(map, n, &bedata->socket_mappings, list) {
		map->sock->sk->sk_send_head = NULL;
		if (map->active_socket) {
			map->active.ring->in_error = -EBADF;
			wake_up_interruptible(&map->active.inflight_conn_req);
		}
	}

	smp_mb();
	while (atomic_read(&rpcalls_refcount) > 0)
		cpu_relax();
	list_for_each_entry_safe(map, n, &bedata->socket_mappings, list) {
		if (map->active_socket) {
			/* No need to lock, refcount is 0 */
			rpmsg_front_free_map(bedata, map);
		} else {
			list_del(&map->list);
			kfree(map);
		}
	}
	if (bedata->ref != -1)
		gnttab_end_foreign_access(bedata->ref, 0, 0);
	kfree(bedata->ring.sring);
	kfree(bedata);
	xenbus_switch_state(dev, XenbusStateClosed);
	return 0;
}

static int rpmsg_front_probe(struct xenbus_device *dev,
			  const struct xenbus_device_id *id)
{
	int ret = -ENOMEM, evtchn, i;
	unsigned int max_page_order, function_calls, len;
	char *versions;
	grant_ref_t gref_head = 0;
	struct xenbus_transaction xbt;
	struct rpcalls_bedata *bedata = NULL;
	struct xen_pvcalls_sring *sring;

	if (rpmsg_front_dev != NULL) {
		dev_err(&dev->dev, "only one PV Calls connection supported\n");
		return -EINVAL;
	}

	versions = xenbus_read(XBT_NIL, dev->otherend, "versions", &len);
	if (IS_ERR(versions))
		return PTR_ERR(versions);
	if (!len)
		return -EINVAL;
	if (strcmp(versions, "1")) {
		kfree(versions);
		return -EINVAL;
	}
	kfree(versions);
	max_page_order = xenbus_read_unsigned(dev->otherend,
					      "max-page-order", 0);
	if (max_page_order < RPCALLS_RING_ORDER)
		return -ENODEV;
	function_calls = xenbus_read_unsigned(dev->otherend,
					      "function-calls", 0);
	/* See XENBUS_FUNCTIONS_CALLS in pvcalls.h */
	if (function_calls != 1)
		return -ENODEV;
	pr_info("%s max-page-order is %u\n", __func__, max_page_order);

	bedata = kzalloc(sizeof(struct rpcalls_bedata), GFP_KERNEL);
	if (!bedata)
		return -ENOMEM;

	dev_set_drvdata(&dev->dev, bedata);
	rpmsg_front_dev = dev;
	init_waitqueue_head(&bedata->inflight_req);
	INIT_LIST_HEAD(&bedata->socket_mappings);
	spin_lock_init(&bedata->socket_lock);
	bedata->irq = -1;
	bedata->ref = -1;

	for (i = 0; i < RPCALLS_NR_RSP_PER_RING; i++)
		bedata->rsp[i].req_id = RPCALLS_INVALID_ID;

	sring = (struct xen_pvcalls_sring *) __get_free_page(GFP_KERNEL |
							     __GFP_ZERO);
	if (!sring)
		goto error;
	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&bedata->ring, sring, XEN_PAGE_SIZE);

	ret = xenbus_alloc_evtchn(dev, &evtchn);
	if (ret)
		goto error;

	bedata->irq = bind_evtchn_to_irqhandler(evtchn,
						rpmsg_front_event_handler,
						0, "pvcalls-frontend", dev);
	if (bedata->irq < 0) {
		ret = bedata->irq;
		goto error;
	}

	ret = gnttab_alloc_grant_references(1, &gref_head);
	if (ret < 0)
		goto error;
	ret = gnttab_claim_grant_reference(&gref_head);
	if (ret < 0)
		goto error;
	bedata->ref = ret;
	gnttab_grant_foreign_access_ref(bedata->ref, dev->otherend_id,
					virt_to_gfn((void *)sring), 0);

 again:
	ret = xenbus_transaction_start(&xbt);
	if (ret) {
		xenbus_dev_fatal(dev, ret, "starting transaction");
		goto error;
	}
	ret = xenbus_printf(xbt, dev->nodename, "version", "%u", 1);
	if (ret)
		goto error_xenbus;
	ret = xenbus_printf(xbt, dev->nodename, "ring-ref", "%d", bedata->ref);
	if (ret)
		goto error_xenbus;
	ret = xenbus_printf(xbt, dev->nodename, "port", "%u",
			    evtchn);
	if (ret)
		goto error_xenbus;
	ret = xenbus_transaction_end(xbt, 0);
	if (ret) {
		if (ret == -EAGAIN)
			goto again;
		xenbus_dev_fatal(dev, ret, "completing transaction");
		goto error;
	}
	xenbus_switch_state(dev, XenbusStateInitialised);

	return 0;

 error_xenbus:
	xenbus_transaction_end(xbt, 1);
	xenbus_dev_fatal(dev, ret, "writing xenstore");
 error:
	rpmsg_front_remove(dev);
	return ret;
}

static void rpmsg_front_changed(struct xenbus_device *dev,
			    enum xenbus_state backend_state)
{
	dev_info(&dev->dev, "%s %p backend(%s) this end(%s)\n",
			 __func__,
			 dev,
			 xenbus_strstate(backend_state),
			 xenbus_strstate(dev->state));

	switch (backend_state) {
	case XenbusStateReconfiguring:
	case XenbusStateReconfigured:
	case XenbusStateInitialising:
	case XenbusStateInitialised:
	case XenbusStateUnknown:
		break;

	case XenbusStateInitWait:
		break;

	case XenbusStateConnected:
		xenbus_switch_state(dev, XenbusStateConnected);
		break;

	case XenbusStateClosed:
		if (dev->state == XenbusStateClosed)
			break;
		/* Missed the backend's CLOSING state */
		/* fall through */
	case XenbusStateClosing:
		xenbus_frontend_closed(dev);
		break;
	}
}

static struct xenbus_driver rpmsg_front_driver = {
	.ids = rpmsg_front_ids,
	.probe = rpmsg_front_probe,
	.remove = rpmsg_front_remove,
	.otherend_changed = rpmsg_front_changed,
};

static int __init rpcalls_front_init(void)
{
	if (!xen_domain() || xen_initial_domain())
		return -ENODEV;

	pr_info("Initialising rpmsg rpcalls frontend driver\n");

	return xenbus_register_frontend(&rpmsg_front_driver);
}

module_init(rpcalls_front_init);

MODULE_ALIAS("rpmsg:rpcall");
MODULE_DESCRIPTION("Semidrive rpcalls kernel module");
MODULE_LICENSE("GPL v2");

