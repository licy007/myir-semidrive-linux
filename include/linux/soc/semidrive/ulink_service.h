#pragma once

#include <linux/io.h>
#include <linux/net.h>
#include <net/net_namespace.h>
#include <uapi/linux/in.h>

#define COPY_DIR_SSA_TO_SSB 0
#define COPY_DIR_SSB_TO_SSA 1

// TODO
#define SERVICE_ADDR 0xac140226
#define SERVICE_PORT 21843

static inline int ulink_pcie_dma_copy(int dir, uint64_t dst, uint64_t src,
				      uint32_t size)
{
	struct socket *sock;
	struct sockaddr_in addr;
	struct msghdr msg;
	struct kvec iov;
	struct {
		int direction;
		uint32_t size;
		uint64_t src_addr;
		uint64_t dst_addr;
	} req;
	struct {
		int retval;
	} rst;
	int ret;

	ret = sock_create_kern(&init_net, PF_INET, SOCK_DGRAM, IPPROTO_UDP,
			       &sock);
	if (ret < 0)
		return ret;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = PF_INET;
	addr.sin_port = htons(SERVICE_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	ret = kernel_bind(sock, (const struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0)
		goto release;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = PF_INET;
	addr.sin_port = htons(SERVICE_PORT);
	addr.sin_addr.s_addr = htonl(SERVICE_ADDR);
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &addr;
	msg.msg_namelen = sizeof(addr);
	iov.iov_base = &req;
	iov.iov_len = sizeof(req);
	ret = kernel_sendmsg(sock, &msg, &iov, 1, iov.iov_len);
	if (ret < 0)
		goto release;

	memset(&msg, 0, sizeof(msg));
	memset(&rst, 0, sizeof(rst));
	iov.iov_base = &rst;
	iov.iov_len = sizeof(rst);
	ret = kernel_recvmsg(sock, &msg, &iov, 1, iov.iov_len, 0);
	if (ret == 0)
		ret = -EINVAL;
	if (ret < 0)
		goto release;

	ret = rst.retval;

release:
	sock_release(sock);

	return ret;
}
