#ifndef MTP_SOCKET_H
#define MTP_SOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <sys/select.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define MAX_SOCKETS 25
#define RECEIVER_BUFFER_SIZE 5
#define DROP_PROBABILITY 0.3
#define MAX_SEQNUM 16

#define SENDER_BUFFER_SIZE 10
#define TIMEOUT_SECONDS 5
#define NUM_SOCKETS 25
#define SHARED_MEMORY_NAME "mtp_shared_memory"
#define SOCK_MTP 2
typedef struct {
    int sequence_number;
    int frame_no;
    int ack_received;
    int timestamp;
    int is_ack;
    int rwnd_size;
    char message[1024]; // Assuming message size is 1024 bytes
} Message;
// Function to serialize Msg data

typedef struct {
    int sequence_number;
    int rwnd_size;
} Acknowledgement;


typedef struct {
    Message receiver_buffer[RECEIVER_BUFFER_SIZE];
    int rwnd_start;
    int rwnd_end;
    int rwnd_size;
    int nospace;
} ReceiverWindow;
typedef struct {
    Message sender_buffer[SENDER_BUFFER_SIZE];
    int swnd_start;
    int swnd_end;
    int swnd_size;
    int swnd_written;
    sem_t* sem_send;
    int nospace;
    int flag;

} SenderWindow;

typedef struct {
    int is_free;
    int bound;
    int recv_socket_id;
    char other_end_IP[20];
    int other_end_port;
    int closed;
    int send_socket_id;
    struct sockaddr_in source_addr;
    struct sockaddr_in dest_addr;
    ReceiverWindow receiver_window;
    SenderWindow sender_window;
} MTPSocketInfo;
int m_socket(int domain, int type, int protocol);
int m_bind(int sockfd, struct sockaddr_in source_addr, socklen_t addrlen,struct sockaddr_in dest_addr);
ssize_t m_sendto(int sockfd, const void *buf, size_t len, int flags,
                 const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t m_recvfrom(int sockfd, void *buf, size_t len, int flags,
                   struct sockaddr *src_addr, socklen_t *addrlen);
int m_close(int sockfd);
void serializeMsg(  Message *data, uint8_t *buffer);

void deserializeMsg(const uint8_t *buffer, Message *data); 
int dropMessage(double p);
void resetSM(MTPSocketInfo* mtp_socket);

#endif /* MTP_SOCKET_H */
