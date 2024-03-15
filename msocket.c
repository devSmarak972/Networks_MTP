#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include "msocket.h"
#define MAX_SOCKETS 10
#define MAX_MESSAGE_SIZE 1024
#define NUM_SOCKETS 25
typedef struct {
    int sockfd;
    struct sockaddr_in src_addr;
    struct sockaddr_in dest_addr;
    int bound;
    char message_buffer[MAX_MESSAGE_SIZE];
    int message_length;
} SocketEntry;
sem_t *semaphore; // Semaphore to synchronize access to shared memory

SocketEntry socket_table[MAX_SOCKETS];
// Function to initialize shared memory buffer when m_socket call is made
int initialize_shared_memory(MTPSocketInfo* shared_memory) {
    // Find the next free index in the shared memory array
    int free_index = -1;
    for (int i = 0; i < NUM_SOCKETS; i++) {
        if (shared_memory[i].is_free) {
            free_index = i;
            break;
        }
    }
    printf("ckpt 4\n");

    // Initialize the source and destination addresses at the free index
    if (free_index != -1) {
        printf("index: %d\n",free_index);
        shared_memory[free_index].is_free = 0; // Mark the entry as used

        return free_index;
        // You may need to initialize other fields of MTPSocketInfo here
    } else {
        printf('No free entry available');
        return -1;
        // No free entry available in shared memory
        // Handle error or return an appropriate value
    }
}

int m_socket(int domain, int type, int protocol) {

    if (type != SOCK_MTP) {
        errno = EINVAL;  // Unsupported socket type
        return -1;
    }
    
    key_t key = ftok("makefile", 'S'); // Generate a key for the shared memory segment

//     // Create or access the shared memory segment
    int shmid = shmget(key, NUM_SOCKETS * sizeof(MTPSocketInfo), 0666);

    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

//     // Attach the shared memory segment to the process address space
    MTPSocketInfo *shared_memory = shmat(shmid, NULL, 0);
     printf("ckpt 1\n");

    if (shared_memory == (MTPSocketInfo *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
            // Initialize semaphore
    semaphore = sem_open("sem", 0);
    if (semaphore == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
     printf("ckpt 2\n");


 // Lock the semaphore before accessing/modifying shared memory
    if (sem_wait(semaphore) == -1) {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
    printf("sem waiting done\n");
    int index=initialize_shared_memory(shared_memory);

    if(index==-1)
    {
        errno = ENOBUFS; // Set global error variable

        //throw error;
        return -1;
    }
         printf("ckpt 3\n");

    // Create the UDP socket
    int sockfd = socket(domain, SOCK_DGRAM, protocol);
    if (sockfd < 0) {
        return -1;
    }
     printf("sem wait\n");

    // Initialize the socket entry
    shared_memory[index].send_socket_id = sockfd;
    socket_table[index].bound = 0;
       // Detach the shared memory segment from the process address space
    // Unlock the semaphore after finishing accessing/modifying shared memory
    if (sem_post(semaphore) == -1) {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
    if (shmdt(shared_memory) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

int m_bind(int sockfd, struct sockaddr_in source_addr, socklen_t addrlen,struct sockaddr_in dest_addr) {
    // Find the socket entry
    int i;
    key_t key = ftok("makefile", 'S'); // Generate a key for the shared memory segment

    // Create or access the shared memory segment
    int shmid = shmget(key, NUM_SOCKETS * sizeof(MTPSocketInfo),  0666);
    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Attach the shared memory segment to the process address space
    MTPSocketInfo *shared_memory = shmat(shmid, NULL, 0);
    if (shared_memory == (MTPSocketInfo *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

  

    if (i == MAX_SOCKETS) {
        errno = EBADF;  // Bad file descriptor
        return -1;
    }
   semaphore = sem_open("sem", 0);
    if (semaphore == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

 
 // Lock the semaphore before accessing/modifying shared memory
    if (sem_wait(semaphore) == -1) {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
    // Bind the UDP socket
    int ret = bind(sockfd, (struct sockaddr *)&source_addr, addrlen);
    if (ret == 0) {
            shared_memory->source_addr = source_addr;
        shared_memory->dest_addr = dest_addr;
         shared_memory->bound = 1;
    }
        if (sem_post(semaphore) == -1) {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
   if (shmdt(shared_memory) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
    return ret;
}

ssize_t m_sendto(int sockfd, const void *buf, size_t len, int flags,
                 const struct sockaddr *dest_addr, socklen_t addrlen) {
    // Find the socket entry
    int i;
    for (i = 0; i < MAX_SOCKETS; i++) {
        if (socket_table[i].sockfd == sockfd) {
            break;
        }
    }

    if (i == MAX_SOCKETS) {
        errno = EBADF;  // Bad file descriptor
        return -1;
    }

    // Check if the socket is bound
    if (!socket_table[i].bound) {
        errno = ENOTCONN;
        return -1;
    }

    // Check if destination IP/Port matches with bound IP/Port
    if (memcmp(dest_addr, &socket_table[i].dest_addr, addrlen) != 0) {
        errno = ENOTCONN;
        return -1;
    }

    // Check if there is space in the send buffer
    if (socket_table[i].message_length + len > MAX_MESSAGE_SIZE) {
        errno = ENOBUFS;
        return -1;
    }

    // Copy the message to the send buffer
    memcpy(socket_table[i].message_buffer + socket_table[i].message_length, buf, len);
    socket_table[i].message_length += len;

    return len;
}

ssize_t m_recvfrom(int sockfd, void *buf, size_t len, int flags,
                   struct sockaddr *src_addr, socklen_t *addrlen) {
    // Find the socket entry
    int i;
    for (i = 0; i < MAX_SOCKETS; i++) {
        if (socket_table[i].sockfd == sockfd) {
            break;
        }
    }

    if (i == MAX_SOCKETS) {
        errno = EBADF;  // Bad file descriptor
        return -1;
    }

    // Check if there is any message in the receive buffer
    if (socket_table[i].message_length == 0) {
        errno = ENOMSG;
        return -1;
    }

    // Copy the message from the receive buffer
    memcpy(buf, socket_table[i].message_buffer, len);
    *addrlen = sizeof(socket_table[i].src_addr);
    memcpy(src_addr, &socket_table[i].src_addr, *addrlen);

    // Reset the receive buffer
    socket_table[i].message_length = 0;

    return len;
}

int m_close(int sockfd) {
    // Find the socket entry
    int i;
    for (i = 0; i < MAX_SOCKETS; i++) {
        if (socket_table[i].sockfd == sockfd) {
            break;
        }
    }

    if (i == MAX_SOCKETS) {
        errno = EBADF;  // Bad file descriptor
        return -1;
    }

    // Close the UDP socket
    int ret = close(sockfd);
    if (ret == 0) {
        memset(&socket_table[i], 0, sizeof(SocketEntry));
        socket_table[i].sockfd = -1;
    }

    return ret;
}
