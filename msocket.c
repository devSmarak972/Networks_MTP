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
sem_t *sem_c; // Semaphore to synchronize access to shared memory
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
    // printf(" 4\n");

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
    printf("key %d\n",key);
//     // Create or access the shared memory segment
    int shmid = shmget(key, NUM_SOCKETS * sizeof(MTPSocketInfo), 0666);

    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

//     // Attach the shared memory segment to the process address space
    MTPSocketInfo *shared_memory = shmat(shmid, NULL, 0);
    //  printf("ckpt 1\n");

    if (shared_memory == (MTPSocketInfo *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
            // Initialize semaphore
    sem_c = sem_open("semc", 0);
    if (sem_c == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    //  printf("ckpt 2\n");


 // Lock the semaphore before accessing/modifying shared memory
    if (sem_wait(sem_c) == -1) {
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
        //  printf("ckpt 3\n");

    // Create the UDP socket
    shared_memory[index].bound=1;
    int ret=0;
    if (sem_post(sem_c) == -1) {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
    while(shared_memory[index].bound>0);
    // if (sem_wait(semaphore) == -1) {
    //     perror("sem_wait");
    //     exit(EXIT_FAILURE);
    // }
    printf("bound %d\n",shared_memory[index].bound);
    if(shared_memory[index].bound==0)
    {
        ret=0;
        shared_memory[index].bound=3;
    }
    else
    {
        shared_memory[index].bound=3;
        ret=shared_memory[index].bound;
        // shared_memory->source_addr 
        // shared_memory->dest_addr ;

    }

       // Detach the shared memory segment from the process address space
    // Unlock the semaphore after finishing accessing/modifying shared memory
    
    if (shmdt(shared_memory) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
    
    return index;
}

int m_bind(int index, struct sockaddr_in source_addr, socklen_t addrlen,struct sockaddr_in dest_addr) {
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
   sem_c = sem_open("semc", 0);
    if (sem_c == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

 
 // Lock the semaphore before accessing/modifying shared memory
    if (sem_wait(sem_c) == -1) {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
    // Bind the UDP socket

    int ret;
            shared_memory[index].source_addr = source_addr;
        shared_memory[index].dest_addr = dest_addr;
         shared_memory[index].bound = 2;
         printf("binding\n");
        if (sem_post(sem_c) == -1) {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
    while(shared_memory[index].bound>0);
    if(shared_memory[index].bound==0)
    {
        ret=0;
    shared_memory[index].bound=3;


    }
    else
    {
        shared_memory[index].bound=3;
        ret=shared_memory[index].bound;
        // shared_memory->source_addr 
        // shared_memory->dest_addr ;

    }


   if (shmdt(shared_memory) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
    return ret;
}

ssize_t m_sendto(int index, const void *buf, size_t len, int flags,
                 const struct sockaddr *dest_addr, socklen_t addrlen) {
    // Find the socket entry
    int i=index;
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

    // Check if the socket is bound
    if (shared_memory[i].bound!=3) {
        errno = ENOTCONN;
        return -1;
    }

    // Check if destination IP/Port matches with bound IP/Port
    if (memcmp(dest_addr, &shared_memory[i].dest_addr, addrlen) != 0) {
        errno = ENOTCONN;
        return -1;
    }

    // Check if there is space in the send buffer
    if (shared_memory[i].sender_window.swnd_start + len > SENDER_BUFFER_SIZE) {
        errno = ENOBUFS;
        return -1;
    }

    // Copy the message to the send buffer
    memcpy(shared_memory[i].sender_window.sender_buffer + shared_memory[i].sender_window.swnd_end, buf, len);
    shared_memory[i].sender_window.swnd_end+=len;

      if (shmdt(shared_memory) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
    return len;
}

ssize_t m_recvfrom(int index, void *buf, size_t len, int flags,
                   struct sockaddr *src_addr, socklen_t *addrlen) {
    // Find the socket entry
    // Find the socket entry
    int i=index;
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

    // Check if the socket is bound
    if (shared_memory[i].bound!=3) {
        errno = ENOTCONN;
        return -1;
    }

 

    // Copy the message from the receive buffer
    memcpy(buf, shared_memory[i].receiver_window.receiver_buffer, len);
    *addrlen = sizeof(shared_memory[i].dest_addr);
    memcpy(src_addr, &shared_memory[i].dest_addr, *addrlen);

    // Reset the receive buffer
    bzero(shared_memory[i].receiver_window.receiver_buffer,shared_memory[i].receiver_window.rwnd_size);
    shared_memory[i].receiver_window.rwnd_size = 0;

    return len;
}

int m_close(int index) {
    // Find the socket entry
    int i=index;
   

    if (i == MAX_SOCKETS) {
        errno = EBADF;  // Bad file descriptor
        return -1;
    }

    // Close the UDP socket
    // int ret = close(sockfd);
    int ret=0;
    // if (ret == 0) {
    //     memset(&socket_table[i], 0, sizeof(SocketEntry));
    //     socket_table[i].sockfd = -1;
    // }

    return ret;
}
