#ifndef __RPCALLS_FRONT_H__
#define __RPCALLS_FRONT_H__

#include <linux/net.h>

int rpmsg_front_socket(struct socket *sock);
int rpmsg_front_connect(struct socket *sock, struct sockaddr *addr,
			  int addr_len, int flags);
int rpmsg_front_bind(struct socket *sock,
		       struct sockaddr *addr,
		       int addr_len);
int rpmsg_front_listen(struct socket *sock, int backlog);
int rpmsg_front_accept(struct socket *sock,
			 struct socket *newsock,
			 int flags);
int rpmsg_front_sendmsg(struct socket *sock,
			  struct msghdr *msg,
			  size_t len);
int rpmsg_front_recvmsg(struct socket *sock,
			  struct msghdr *msg,
			  size_t len,
			  int flags);
unsigned int rpmsg_front_poll(struct file *file,
				struct socket *sock,
				poll_table *wait);
int rpmsg_front_release(struct socket *sock);

#endif