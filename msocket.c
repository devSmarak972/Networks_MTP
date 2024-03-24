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
sem_t *sem_recv; // Semaphore to synchronize access to shared memory

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
    // printf("key %d\n",key);
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
    semaphore = sem_open("sem", 0);
    if (sem_c == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    if (semaphore == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    //  printf("ckpt 2\n");


 // Lock the semaphore before accessing/modifying shared memory
    if (sem_wait(sem_c) == -1) {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
    // printf("sem waiting done\n");
    int index=initialize_shared_memory(shared_memory);
    if(index==-1)
    {
        errno = ENOBUFS; // Set global error variable

    if (sem_post(sem_c) == -1) {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
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
    if (sem_wait(semaphore) == -1) {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
    // printf("bound %d\n",shared_memory[index].bound);
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
    if (sem_post(semaphore) == -1) {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }

       // Detach the shared memory segment from the process address space
    // Unlock the semaphore after finishing accessing/modifying shared memory
    
    if (shmdt(shared_memory) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
    
    return index;
}

int m_bind(int sockfd, struct sockaddr_in source_addr, socklen_t addrlen,struct sockaddr_in dest_addr) {
    // Find the socket entry
    int i=sockfd;
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
            shared_memory[i].source_addr = source_addr;
        shared_memory[i].dest_addr = dest_addr;
         shared_memory[i].bound = 2;
         printf("binding\n");
        if (sem_post(sem_c) == -1) {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }

    while(shared_memory[i].bound>0);
    if(shared_memory[i].bound==0)
    {
        ret=0;
    shared_memory[i].bound=3;

    }
    else
    {
        shared_memory[i].bound=3;
        ret=shared_memory[i].bound;
        // shared_memory->source_addr 
        // shared_memory->dest_addr ;

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
    int i=sockfd;
    key_t key = ftok("makefile", 'S'); // Generate a key for the shared memory segment

    // Create or access the shared memory segment
    int shmid = shmget(key, NUM_SOCKETS * sizeof(MTPSocketInfo),  0666);
    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Attach the shared memory segment to the process address space


    if (i == MAX_SOCKETS) {
        errno = EBADF;  // Bad file descriptor
        return -1;
    }
    MTPSocketInfo *shared_memory = shmat(shmid, NULL, 0);
    if (shared_memory == (MTPSocketInfo *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // Check if the socket is bound
    if (shared_memory[i].bound!=3) {
        errno = ENOTCONN;
        if (shmdt(shared_memory) == -1) {
        
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
        return -1;
    }
     const struct sockaddr_in *sin1 = (const struct sockaddr_in *)dest_addr;
    const struct sockaddr_in *sin2 = (const struct sockaddr_in *)&shared_memory[i].dest_addr;
    // Compare IPv4 addresses
        if (memcmp(&sin1->sin_addr, &sin2->sin_addr, sizeof(sin1->sin_addr)) != 0)
        {
            errno = ENOTCONN;
            if (shmdt(shared_memory) == -1) {
        
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
             return -1;
        }   
        
        // Compare ports
        if (sin1->sin_port != sin2->sin_port)
        {

            errno = ENOTCONN;
            if (shmdt(shared_memory) == -1) {
        
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
             return -1;
        }


    // Check if destination IP/Port matches with bound IP/Port
    // if (memcmp(dest_addr, (struct sockaddr *)&shared_memory[i].dest_addr, addrlen) != 0) {
    //     errno = ENOTCONN;
    //     return -1;
    // }

    // Check if there is space in the send buffer
    // if (shared_memory[i].sender_window.swnd_size==0) {
    //     errno = ENOBUFS;
    //     return -1;
    // }
    // Copy the message to the send buffer
    sem_t* sema=sem_open("sem", 1);
    if (sem_wait(sema) == -1) {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
    int s_end=shared_memory[i].sender_window.swnd_written%SENDER_BUFFER_SIZE;
    // printf("s_end %d %d\n",shared_memory[i].sender_window.swnd_start,shared_memory[i].sender_window.swnd_written);
    if((shared_memory[i].sender_window.swnd_start!=shared_memory[i].sender_window.swnd_written) && (s_end%SENDER_BUFFER_SIZE==(shared_memory[i].sender_window.swnd_start%SENDER_BUFFER_SIZE)) && shared_memory[i].sender_window.flag)
    shared_memory[i].sender_window.nospace=1;
    else
    {
    shared_memory[i].sender_window.nospace=0;
    }
    // printf("is ack sand time %d %d\n",shared_memory[i].sender_window.sender_buffer[s_end].ack_received,shared_memory[i].sender_window.sender_buffer[s_end].timestamp);
    if(shared_memory[i].sender_window.sender_buffer[s_end].ack_received==0 )
    shared_memory[i].sender_window.nospace=1;

    if(!shared_memory[i].sender_window.flag)
        shared_memory[i].sender_window.flag=1; //to not give nobufs for the first time

    if(shared_memory[i].sender_window.nospace)
    {
        errno = ENOBUFS;
        if (shmdt(shared_memory) == -1) {
        
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
    if (sem_post(sema) == -1) {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
        return -1;
    }

    // printf("%d\n",len);
    memcpy(shared_memory[i].sender_window.sender_buffer[s_end].message , buf, len);
    
    shared_memory[i].sender_window.swnd_written+=1;
    shared_memory[i].sender_window.sender_buffer[s_end].timestamp=-1;
    shared_memory[i].sender_window.sender_buffer[s_end].frame_no=(shared_memory[i].sender_window.swnd_written-1);
    // printf("%s %d %d\n",shared_memory[i].sender_window.sender_buffer[s_end].message,shared_memory[i].sender_window.swnd_written,i);
    if (sem_post(sema) == -1) {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
    if (shmdt(shared_memory) == -1) {
        
        perror("shmdt");
        exit(EXIT_FAILURE);

    }
    return len;
}
void serializeMsg(  Message *data, uint8_t *buffer) {
    // printf("serializing %s\n",data->message);
    memcpy(buffer, &(data->sequence_number), sizeof(int));
    memcpy(buffer+sizeof(int), &(data->is_ack), sizeof(int));
    memcpy(buffer+2*sizeof(int), &(data->rwnd_size), sizeof(int));
    memcpy(buffer + 3*sizeof(int), data->message, strlen(data->message));
}

void deserializeMsg(const uint8_t *buffer, Message *data) {
    memcpy(&(data->sequence_number), buffer, sizeof(int));
    memcpy(&(data->is_ack), buffer+sizeof(int), sizeof(int));
    memcpy(&(data->rwnd_size), buffer+2*sizeof(int), sizeof(int));
    memcpy(data->message, buffer + 3*sizeof(int), sizeof(data->message));
    // printf("in desirsalize:%d\n",data->sequence_number);
}

ssize_t m_recvfrom(int sockfd, void *buf, size_t len, int flags,
                   struct sockaddr *src_addr, socklen_t *addrlen) {
    // Find the socket entry
    // Find the socket entry
    int i=sockfd;
    key_t key = ftok("makefile", 'S'); // Generate a key for the shared memory segment

    // Create or access the shared memory segment
    int shmid = shmget(key, NUM_SOCKETS * sizeof(MTPSocketInfo),  0666);
    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Attach the shared memory segment to the process address space


    if (i == MAX_SOCKETS) {
        errno = EBADF;  // Bad file descriptor
        return -1;
    }
    MTPSocketInfo *shared_memory = shmat(shmid, NULL, 0);
    if (shared_memory == (MTPSocketInfo *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // Check if the socket is bound
    if (shared_memory[i].bound!=3) {
        errno = ENOTCONN;

    if (shmdt(shared_memory) == -1) {
        
        perror("shmdt");
        exit(EXIT_FAILURE);

    }
        return -1;
    }
    // char semaphore_name[20];
    
    // snprintf(semaphore_name, sizeof(semaphore_name), "/semrecv_%d", i); // Generate unique semaphore name

    // sem_t* sem_recv=sem_open(semaphore_name, 0);


    // Copy the message from the receive buffer
    // sem_wait(sem_recv);
    // for (int j = shared_memory[i].receiver_window.rwnd_start; j< (shared_memory[i].receiver_window.rwnd_end); j=(j+1)) {
    //     // if(shared_memory[i].receiver_window.receiver_buffer[j].sequence_number==0)
    //     // {
    //     //     break;
    //     // }
    int idx=shared_memory[i].receiver_window.rwnd_start%RECEIVER_BUFFER_SIZE;
    if(shared_memory[i].receiver_window.receiver_buffer[idx].message==NULL)
    {
        errno = ENOMSG;

    if (shmdt(shared_memory) == -1) {
        
        perror("shmdt");
        exit(EXIT_FAILURE);

    }
        return -1;

    }
    if(strlen(shared_memory[i].receiver_window.receiver_buffer[idx].message)==0)
    {
        errno=ENOMSG;

    if (shmdt(shared_memory) == -1) {
        
        perror("shmdt");
        exit(EXIT_FAILURE);

    }
        return -1;
    }
    if(shared_memory[i].receiver_window.rwnd_size==RECEIVER_BUFFER_SIZE)
    {
        return -1;

        errno=ENOMSG;
    if (shmdt(shared_memory) == -1) {
        
        perror("shmdt");
        exit(EXIT_FAILURE);

    }
    }
    if(len<MAX_MESSAGE_SIZE)
    {
        errno=EMSGSIZE;

    if (shmdt(shared_memory) == -1) {
        
        perror("shmdt");
        exit(EXIT_FAILURE);

    }
        return -1;
    }
    sem_t* sema=sem_open("sem", 1);
    if (sem_wait(sema) == -1) {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
    // printf("start :%d end:%d size: %d\n",shared_memory[i].receiver_window.rwnd_start,shared_memory[i].receiver_window.rwnd_end,shared_memory[i].receiver_window.rwnd_size);
    // printf("copying %s %d %d\n",shared_memory[i].receiver_window.receiver_buffer[idx].message,sizeof(buf),sizeof(shared_memory[i].receiver_window.receiver_buffer[idx].message));
    // memcpy(buf, shared_memory[i].receiver_window.receiver_buffer[idx].message, len);
    bzero(buf,len);
    strcpy(buf, shared_memory[i].receiver_window.receiver_buffer[idx].message);
    if(addrlen!=NULL)
    {
    *addrlen = sizeof(shared_memory[i].dest_addr);
    memcpy(src_addr, &shared_memory[i].dest_addr, *addrlen);
    }
    // Reset the receive buffer
    bzero(shared_memory[i].receiver_window.receiver_buffer[idx].message,sizeof(shared_memory[i].receiver_window.receiver_buffer->message));
    shared_memory[i].receiver_window.receiver_buffer[idx].is_ack=0;
    shared_memory[i].receiver_window.receiver_buffer[idx].sequence_number=-1;
    shared_memory[i].receiver_window.rwnd_size++;
    shared_memory[i].receiver_window.rwnd_start++;
    // printf("start updated %d\n",shared_memory[i].receiver_window.rwnd_start);
    // shared_memory[i].receiver_window.rwnd_size = 0;
                
        if (sem_post(sema) == -1) {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }

    if (shmdt(shared_memory) == -1) {
        
        perror("shmdt");
        exit(EXIT_FAILURE);

    }
        // printf("message in recv: %s\n",buf);

    return len;
}

int dropMessage(double p) {
    float random_number = (float)rand() / (float)RAND_MAX;
    return random_number < p;
}
int m_close(int sockfd) {
    // Find the socket entry
    int i=sockfd;
       key_t key = ftok("makefile", 'S'); // Generate a key for the shared memory segment

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

    if(shared_memory[i].is_free)
    {
        errno=ENOTCONN;
        return -1;
    }
    sem_t* sema=sem_open("sem", 1);
    if (sem_wait(sema) == -1) {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
    shared_memory[i].closed=1;
    // resetSM(&shared_memory[i]);
    
    if (sem_post(sema) == -1) {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }

    return 0;

}

void resetSM(MTPSocketInfo* mtp_socket)
{
    memset(mtp_socket, 0, sizeof(MTPSocketInfo));
    mtp_socket->is_free=1;
    mtp_socket->bound=-1;
    mtp_socket->closed=0;
    mtp_socket->receiver_window.rwnd_size = RECEIVER_BUFFER_SIZE;
    mtp_socket->receiver_window.rwnd_start = 0;
    mtp_socket->receiver_window.rwnd_end = 0;
    for(int j=0;j<RECEIVER_BUFFER_SIZE;j++)
    mtp_socket->receiver_window.receiver_buffer[j].sequence_number = -1;
        for(int j=0;j<SENDER_BUFFER_SIZE;j++)
        {
        mtp_socket->sender_window.sender_buffer[j].timestamp = -1;
        mtp_socket->sender_window.sender_buffer[j].ack_received = -1;
        }
        mtp_socket->sender_window.swnd_start=0;
        mtp_socket->sender_window.swnd_end=RECEIVER_BUFFER_SIZE;
        mtp_socket->sender_window.swnd_size=RECEIVER_BUFFER_SIZE;
        mtp_socket->sender_window.nospace=0;
        mtp_socket->receiver_window.nospace=0;
        mtp_socket->sender_window.flag=0;
        mtp_socket->sender_window.swnd_written=0;
}