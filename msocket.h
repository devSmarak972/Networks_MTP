#ifndef MTP_SOCKET_H
#define MTP_SOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SOCK_MTP 2

int m_socket(int domain, int type, int protocol);
int m_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t m_sendto(int sockfd, const void *buf, size_t len, int flags,
                 const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t m_recvfrom(int sockfd, void *buf, size_t len, int flags,
                   struct sockaddr *src_addr, socklen_t *addrlen);
int m_close(int sockfd);

#endif /* MTP_SOCKET_H */
