#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "msocket.h"

#include <sys/ipc.h>
#include <sys/shm.h>

// Shared data structure for MTP sockets


// Function prototypes for threads R and S
void *thread_R(void *arg);
void *thread_S(void *arg);
void *garbage_collector(void *arg);
sem_t *semaphore; // Semaphore to synchronize access to shared memory
sem_t *sem_c; // Semaphore to synchronize access to shared memory
int selfpipe[2];
sem_t* sem_recv;
int transmission=0;
MTPSocketInfo *shared_memory ;

void initialize_semaphores() {
    sem_unlink("sem");
    sem_unlink("semc");
    sem_unlink("Sem1");
    sem_unlink("Sem2");

    semaphore = sem_open("sem", O_CREAT, 0666, 1);
    if (semaphore == SEM_FAILED) {
        perror("sem_open semaphore");
        exit(EXIT_FAILURE);
    }

    sem_c = sem_open("semc", O_CREAT, 0666, 1);
    if (sem_c == SEM_FAILED) {
        perror("sem_open sem_c");
        exit(EXIT_FAILURE);
    }

    sem1 = sem_open("Sem1", O_CREAT, 0666, 0);
    if (sem1 == SEM_FAILED) {
        perror("sem_open Sem1");
        exit(EXIT_FAILURE);
    }

    sem2 = sem_open("Sem2", O_CREAT, 0666, 0);
    if (sem2 == SEM_FAILED) {
        perror("sem_open Sem2");
        exit(EXIT_FAILURE);
    }
}


int main() {

    // printf("entry\n");
     key_t key = ftok("makefile", 'S'); // Generate a key for the shared memory segment
    // printf("key %d\n",key);
    // Create or access the shared memory segment
    pipe(selfpipe);
    int shmid = shmget(key, NUM_SOCKETS * sizeof(MTPSocketInfo), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }
    transmission=0;

    // Attach the shared memory segment to the process address space
  shared_memory = (MTPSocketInfo*)shmat(shmid, NULL, 0);
    if (shared_memory == (MTPSocketInfo *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    bzero(shared_memory,NUM_SOCKETS * sizeof(MTPSocketInfo));
    sem_unlink("sem");
    sem_unlink("semc");
    // sem_unlink("sem_recv");

    // Initialize semaphore
    semaphore = sem_open("sem", O_CREAT|O_EXCL, 0666, 1);
    if (semaphore == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    sem_c = sem_open("semc", O_CREAT|O_EXCL, 0666, 1);
    if (semaphore == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    
    //   sem_recv=sem_open("/semrecv", O_CREAT, 0666, 0);

    initialize_semaphores();
     
    // Initialize shared memory structures
    // Set all MTP socket entries as free
    for (int i = 0; i < NUM_SOCKETS; i++) {
        shared_memory[i].is_free = 1;
        shared_memory[i].bound = -1;
        shared_memory[i].receiver_window.rwnd_size = RECEIVER_BUFFER_SIZE;
        shared_memory[i].receiver_window.rwnd_start = 0;
        shared_memory[i].receiver_window.rwnd_end = 0;
        for(int j=0;j<RECEIVER_BUFFER_SIZE;j++)
        shared_memory[i].receiver_window.receiver_buffer[j].sequence_number = -1;
        for(int j=0;j<SENDER_BUFFER_SIZE;j++)
        {
        shared_memory[i].sender_window.sender_buffer[j].timestamp = -1;
        shared_memory[i].sender_window.sender_buffer[j].ack_received = -1;
        }
        shared_memory[i].sender_window.swnd_start=0;
        shared_memory[i].sender_window.swnd_end=RECEIVER_BUFFER_SIZE;
        shared_memory[i].sender_window.swnd_size=RECEIVER_BUFFER_SIZE;
        shared_memory[i].sender_window.nospace=0;
        shared_memory[i].receiver_window.nospace=0;
        shared_memory[i].sender_window.flag=0;
        shared_memory[i].sender_window.swnd_written=0;

    }
        //  printf("ened init:%d\n",shared_memory[0].sender_window.swnd_end);


    // Create threads R, S, and garbage collector G
    pthread_t thread_R_id, thread_S_id, garbage_collector_id;
    if (pthread_create(&thread_R_id, NULL, thread_R, (void *)shared_memory) != 0) {
        perror("Error creating thread R");
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&thread_S_id, NULL, thread_S, (void *)shared_memory) != 0) {
        perror("Error creating thread S");
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&garbage_collector_id, NULL, garbage_collector, (void *)shared_memory) != 0) {
        perror("Error creating garbage collector thread");
        exit(EXIT_FAILURE);
    }



    // while(1)
    // {

        

    // for (int i = 0; i < MAX_SOCKETS; i++) {
    //     if(shared_memory[i].is_free==1)continue;
    //     if(shared_memory[i].bound==1)
    //     {
    //         if (sem_wait(sem_c) == -1) {
    //         perror("sem_wait");
    //         exit(EXIT_FAILURE);
    //     }
    //         int ssockfd = socket(AF_INET, SOCK_DGRAM, 0);
    //         if (ssockfd < 0) {
    //             return -1;
    //         }
    //         int rsockfd = socket(AF_INET, SOCK_DGRAM, 0);
    //         if (rsockfd < 0) {
    //             return -1;
    //         }
    //         printf("socket creation \n");

    //         // Initialize the socket entry
    //         shared_memory[i].send_socket_id = ssockfd;
    //         shared_memory[i].recv_socket_id = rsockfd;
    //         shared_memory[i].bound=0;
    //         if (sem_post(sem_c) == -1) {
    //             perror("sem_post");
    //             exit(EXIT_FAILURE);
    //         }
    //     }   
    //     else if(shared_memory[i].bound==2)
    //     {
    //         if (sem_wait(sem_c) == -1) {
    //         perror("sem_wait");
    //         exit(EXIT_FAILURE);
    //     }
                    
    //         int ret = bind(shared_memory[i].recv_socket_id, (struct sockaddr *)&shared_memory[i].source_addr, sizeof(shared_memory[i].source_addr));
    //         if (ret == 0) {
                    
    //             shared_memory[i].bound = 0;
    //             printf("binding\n");
    //         }
    //         else{
    //             perror("error in binding");
    //             shared_memory[i].bound = -1;
    //         }
    //         if (sem_post(sem_c) == -1) {
    //         perror("sem_post");
    //         exit(EXIT_FAILURE);
    //     } 
            
    //     }

    //     }
            
    // }

    while(1) {
    // Loop through all MTP sockets to check their status
    for (int i = 0; i < NUM_SOCKETS; i++) {
        if(sem_wait(sem_c) == -1) { // Wait for access to shared memory
            perror("sem_wait on sem_c failed");
            continue;
        }
        if(shared_memory[i].is_free) {sem_post(sem_c); continue;} // Skip if the socket is not in use

        // Handle socket creation request
        if(shared_memory[i].bound == 1) {
            int ssockfd = socket(AF_INET, SOCK_DGRAM, 0); // Create a new socket for sending
            int rsockfd = socket(AF_INET, SOCK_DGRAM, 0); // Create a new socket for receiving

            if (ssockfd < 0 || rsockfd < 0) {
                perror("Socket creation failed");
                continue;
            }

            printf("socket creation \n");

            // Assign socket IDs for sending and receiving to the shared memory
            shared_memory[i].send_socket_id = ssockfd;
            shared_memory[i].recv_socket_id = rsockfd;
            shared_memory[i].bound=0;
            
            if (sem_post(sem_c) == -1) {
                perror("sem_post");
                exit(EXIT_FAILURE);
            }
        }   
        else if(shared_memory[i].bound==2)
        {
            if (sem_wait(sem_c) == -1) {
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
                    
            int ret = bind(shared_memory[i].recv_socket_id, (struct sockaddr *)&shared_memory[i].source_addr, sizeof(shared_memory[i].source_addr));
            if (ret == 0) {
                    
                shared_memory[i].bound = 0;
                  char signal = 'X'; // Or any data you want to write
                write(selfpipe[1], &signal, sizeof(signal));
                printf("binding..\n");
            }

            // Attempt to bind the receiving socket
            int bindResult = bind(shared_memory[i].recv_socket_id,
                                  (struct sockaddr *)&shared_memory[i].source_addr,
                                  sizeof(shared_memory[i].source_addr));
            if (bindResult == 0) {
                shared_memory[i].bound = 0; // Mark as successfully bound
            } else {
                perror("Binding failed");
                shared_memory[i].bound = -1; // Mark as bind failed
            }
        //  printf("ened initend:%d\n",shared_memory[i].sender_window.swnd_end);


            if (sem_post(sem_c) == -1) {
            perror("sem_post");
            exit(EXIT_FAILURE);
        } 
            
        }

            if (sem_post(sem2) == -1) { // Signal completion of bind operation to msocket.c
                perror("sem_post on Sem2 failed");
                sem_post(sem_c);
                continue;
            }
        }
        sem_post(sem_c); // Release access to shared memory
    }

    usleep(100000); // Sleep briefly to reduce CPU usage
}

    


    // Wait for threads to finish
    if (pthread_join(thread_R_id, NULL) != 0) {
        perror("Error joining thread R");
        exit(EXIT_FAILURE);
    }
    if (pthread_join(thread_S_id, NULL) != 0) {
        perror("Error joining thread S");
        exit(EXIT_FAILURE);
    }
    if (pthread_join(garbage_collector_id, NULL) != 0) {
        perror("Error joining garbage collector thread");
        exit(EXIT_FAILURE);
    }


    // Free shared memory
    // free(shared_memory);

    // Detach the shared memory segment from the process address space
    if (shmdt(shared_memory) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }

    // Optionally, delete the shared memory segment
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(EXIT_FAILURE);
    }

    return 0;
}




// Function prototype
void process_received_message(int i,Message received_message, ReceiverWindow *receiver_window, MTPSocketInfo mtp_socket_info);

void *thread_R(void *arg) {
    double p=DROP_PROBABILITY;
    // MTPSocketInfo *shared_memory = (MTPSocketInfo *)arg;
    fd_set readfds;
int max_sd = 0;
    struct timeval timeout;
    int timeout_secs=TIMEOUT_SECONDS;

while (1) {
    FD_ZERO(&readfds);
    max_sd = 0;
    FD_SET(selfpipe[0], &readfds);
    max_sd=selfpipe[0];
    // Add sockets to the fd_set and update max_sd
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!shared_memory[i].is_free && shared_memory[i].bound == 3 && shared_memory[i].receiver_window.rwnd_size > 0) {
            FD_SET(shared_memory[i].recv_socket_id, &readfds);
            if (shared_memory[i].recv_socket_id > max_sd) {
                max_sd = shared_memory[i].recv_socket_id;
            }
        }
    }
        // printf("select in recv\n");
    // Use select to monitor sockets for activity
       timeout.tv_sec = timeout_secs;
        timeout.tv_usec = 0;
    int activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);
    if (activity < 0) {
        perror("Error in select");
        continue;
    }
    if(activity==0)
    {
        //TIMEOUT OCCURRED
        for (int i = 0; i < MAX_SOCKETS; i++) {
            if (!shared_memory[i].is_free && shared_memory[i].bound == 3 && shared_memory[i].receiver_window.rwnd_size > 0) {
            if(shared_memory[i].receiver_window.nospace && shared_memory[i].receiver_window.rwnd_size>0)
            {
                //no space is set but space available in receiver buffer
            int l=lastInOrder(&shared_memory[i].receiver_window);
            shared_memory[i].receiver_window.rwnd_end=l;
            send_acknowledgement(shared_memory[i].recv_socket_id, l-1,shared_memory[i].receiver_window.rwnd_size,shared_memory[i]);

            }
            }
        }
    }
    // Check each socket for activity
    if (FD_ISSET(selfpipe[0], &readfds)) {
        // Do something to handle the self-pipe signal
        // For example, consume the signal by reading from the pipe
        char buf[1];
        read(selfpipe[0], buf, sizeof(buf));
    }
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!shared_memory[i].is_free && shared_memory[i].bound == 3 && shared_memory[i].receiver_window.rwnd_size > 0) {
            if (FD_ISSET(shared_memory[i].recv_socket_id, &readfds)) {
                Message received_message;
                uint8_t buffer[sizeof(Message)];

                int bytes_received = recvfrom(shared_memory[i].recv_socket_id, buffer, sizeof(buffer), MSG_DONTWAIT, NULL, NULL);
                if (bytes_received < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // No data available yet, handle accordingly
                        continue;
                    } else {
                        perror("Error receiving data");
                        continue;
                    }
                } else if (bytes_received > 0) {

                    if(!dropMessage(p))
                    {
                    // printf("bytes recv: %d\n", bytes_received);
                    deserializeMsg(buffer, &received_message);
                    int prev_rsize = shared_memory[i].receiver_window.rwnd_size;
                    if (received_message.is_ack)
                        process_received_acknowledgement(received_message, &shared_memory[i].sender_window);
                    else {
                        // printf("----------------------\nrwnd_start: %d rwnd_end :%d rwnd size: %d\n", shared_memory[i].receiver_window.rwnd_start, shared_memory[i].receiver_window.rwnd_end, shared_memory[i].receiver_window.rwnd_size);
                        process_received_message(i, received_message, &(shared_memory[i].receiver_window), shared_memory[i]);
                        // printf("**********************\nrwnd_start: %d rwnd_end :%d rwnd size: %d %d\n", shared_memory[i].receiver_window.rwnd_start, shared_memory[i].receiver_window.rwnd_end, shared_memory[i].receiver_window.rwnd_size,shared_memory[i].receiver_window.receiver_buffer[shared_memory[i].receiver_window.rwnd_start%RECEIVER_BUFFER_SIZE].sequence_number);
                        if(shared_memory[i].receiver_window.rwnd_size==RECEIVER_BUFFER_SIZE)
                        shared_memory[i].receiver_window.nospace=1;
                        // if (prev_rsize == RECEIVER_BUFFER_SIZE && shared_memory[i].receiver_window.rwnd_size == RECEIVER_BUFFER_SIZE - 1) {
                        //     printf("signalled\n\n");
                        //     // sem_post(shared_memory[i].receiver_window.sem_recv);
                        // }
                    }
                    // printf("received data %s %d\n", received_message.message, received_message.sequence_number);
                    }
                    bzero(&received_message, sizeof(Message));
                } else {
                    printf("No data\n");
                    // no data received
                }
            }
        }
    }
}

pthread_exit(NULL);

}

void process_received_acknowledgement(Message ack, SenderWindow *sender_window) {
    // Update rwnd size
       // Lock the semaphore before accessing/modifying shared memory
    if (sem_wait(semaphore) == -1) {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
    //    printf("hello ack: seq num %d %d\n",ack.sequence_number,ack.rwnd_size);
    // sender_window->rwnd_size = ack.rwnd_size;

    // Update last in-order sequence number
    int last_in_order_seq = ack.sequence_number;
    int rwnd_size = ack.rwnd_size;

    // Mark buffer as free for all previous sequence numbers
    for (int i = sender_window->swnd_start; i != (last_in_order_seq+1); i++) {
        int index = i % SENDER_BUFFER_SIZE;
        if(sender_window->sender_buffer[index].frame_no>last_in_order_seq)continue;

        // printf("deleting num %d\n",sender_window->sender_buffer[index].frame_no);
        memset(sender_window->sender_buffer[index].message,'\0',sizeof(sender_window->sender_buffer[index].message));
        sender_window->sender_buffer[index].sequence_number = -1; // Mark as free
        sender_window->sender_buffer[index].timestamp = -1; // Mark as free
        sender_window->sender_buffer[index].ack_received = 1; // Mark as free
    }
    
    sender_window->swnd_start = last_in_order_seq + 1; // Update start index of receive window
    sender_window->swnd_end=sender_window->swnd_start+rwnd_size;
    sender_window->swnd_size=rwnd_size;
    // printf("swnd updated : swnd_end %d start %d\n",sender_window->swnd_end,sender_window->swnd_start);
   // Lock the semaphore before accessing/modifying shared memory
    if (sem_post(semaphore) == -1) {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
}

void send_acknowledgement(int socket_fd, int sequence_number, int rwnd_size,MTPSocketInfo mtp_socket_info) {
    Message ack;
    ack.sequence_number = sequence_number;
    ack.rwnd_size = rwnd_size;
    ack.is_ack=1;//zero for message non zero for ack
    uint8_t ack_buffer[sizeof(Message)];
    // Convert ACK structure to byte array
    serializeMsg(&ack,ack_buffer);
    // Send ACK message
    // shared_memory[i].receiver_window.receiver_buffer[j].
    if(sendto(socket_fd, ack_buffer, sizeof(ack_buffer), 0,(struct sockaddr *)&(mtp_socket_info.dest_addr), sizeof(mtp_socket_info.dest_addr)) == -1)
    {
        perror("ack send failed");
    }
    printf("ack sent\n");
    // send(socket_fd, ack_buffer, sizeof(Message), 0);
}
// Process received message and update receiver window
void process_received_message(int i, Message received_message, ReceiverWindow *receiver_window, MTPSocketInfo mtp_socket_info) {
    if (sem_wait(semaphore) == -1) {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
    int left=RECEIVER_BUFFER_SIZE-(receiver_window->rwnd_end-receiver_window->rwnd_start)%RECEIVER_BUFFER_SIZE;

    // printf("recevd message: seq number:%d rwnd end:%d\n",received_message.sequence_number,receiver_window->rwnd_end);
    if (received_message.sequence_number == (receiver_window->rwnd_end)%MAX_SEQNUM  ) {
        // In-order message
        int index = receiver_window->rwnd_end % RECEIVER_BUFFER_SIZE;
        // printf("seq num: %d %d %d\n",received_message.sequence_number,receiver_window->receiver_buffer[index].sequence_number,index );
        if (receiver_window->receiver_buffer[index].sequence_number == -1) {
            // Message not yet received, store in receiver buffer
            memcpy(receiver_window->receiver_buffer[index].message,received_message.message,strlen(received_message.message));
            // printf("inorder message not duplicate %d %s\n",index,receiver_window->receiver_buffer[index].message);
            // printf("inorder message not duplicate %d \n",index);
            receiver_window->receiver_buffer[index].sequence_number=received_message.sequence_number;
            receiver_window->rwnd_end++;
            receiver_window->rwnd_size--;
            int value;

    // Debugging output
    printf("Received message: seq num: %d, rwnd end: %d\n", received_message.sequence_number, receiver_window->rwnd_end);

    int index = received_message.sequence_number % RECEIVER_BUFFER_SIZE;

            // Get the value of the semaphore
            int l=lastInOrder(receiver_window);
            printf("sending ack %d\n",l-1);
            receiver_window->rwnd_end=l;
            send_acknowledgement(shared_memory[i].recv_socket_id, l-1,receiver_window->rwnd_size,mtp_socket_info);

            // send_acknowledgement(shared_memory[i].recv_socket_id, receiver_window->rwnd_end-1,receiver_window->rwnd_size,mtp_socket_info);
        } else {
            int idx;
            // Duplicate message, still send ACK with last in-order sequence number
            // for( idx=0;idx<RECEIVER_BUFFER_SIZE;idx++)
            // {
            //     if(receiver_window->receiver_buffer[idx].sequence_number==0)
            //     {
            //         break;
            //     }
            // }'
            // printf("Inorder duplicate\n");
            int l=lastInOrder(receiver_window);
            receiver_window->rwnd_end=l;
            send_acknowledgement(shared_memory[i].recv_socket_id, l-1,receiver_window->rwnd_size,mtp_socket_info);

            // send_acknowledgement(shared_memory[i].recv_socket_id, receiver_window->rwnd_end-1,receiver_window->rwnd_size,mtp_socket_info);
        }
    } else if (received_message.sequence_number > receiver_window->rwnd_end%MAX_SEQNUM &&
               received_message.sequence_number < (receiver_window->rwnd_start+RECEIVER_BUFFER_SIZE)%MAX_SEQNUM  ) {
        // Out-of-order message
        int index = received_message.sequence_number % RECEIVER_BUFFER_SIZE;
        if (receiver_window->receiver_buffer[index].sequence_number == -1) {
            // Store if within window and not a duplicate
            memcpy(receiver_window->receiver_buffer[index].message,received_message.message,strlen(received_message.message));
            // printf("ahead message %d %s\n",index,receiver_window->receiver_buffer[index].message);
            receiver_window->rwnd_size--;
            receiver_window->receiver_buffer[index].sequence_number=received_message.sequence_number;
            // int l=lastInOrder(receiver_window);
            // receiver_window->rwnd_end=l;
            // send_acknowledgement(shared_memory[i].recv_socket_id,l-1, receiver_window->rwnd_size,mtp_socket_info);
        }

        int idx;
            // Duplicate message, still send ACK with last in-order sequence number
                      int l=lastInOrder(receiver_window);
            receiver_window->rwnd_end=l;
            send_acknowledgement(shared_memory[i].recv_socket_id, l-1,receiver_window->rwnd_size,mtp_socket_info);
        // Send ACK with last in-order sequence number
        // send_acknowledgement(shared_memory[i].recv_socket_id,receiver_window->rwnd_end-1, receiver_window->rwnd_size,mtp_socket_info);
    } 
    // else if(received_message.sequence_number<receiver_window->rwnd_end+left) {

    //     // in window range but ahead of rwnd_end
    //     int index = received_message.sequence_number % RECEIVER_BUFFER_SIZE;
    //     if (receiver_window->receiver_buffer[index].sequence_number == -1) {
    //         // Store if within window and not a duplicate
    //         memcpy(receiver_window->receiver_buffer[index].message,received_message.message,strlen(received_message.message));
    //         printf("here mid %d %s\n",index,receiver_window->receiver_buffer[index].message);
    //         receiver_window->rwnd_size--;
    //         receiver_window->receiver_buffer[index].sequence_number=received_message.sequence_number;
    //         int l=lastInOrder(receiver_window);
    //         receiver_window->rwnd_end=l;

    //         send_acknowledgement(shared_memory[i].recv_socket_id,l-1, receiver_window->rwnd_size,mtp_socket_info);
    //     }

    // }
    else{
        // printf("out of window %d\n",received_message.sequence_number);
    }
    if (sem_post(semaphore) == -1) {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
}
int lastInOrder(ReceiverWindow* receiver_window)
{
    for(int k=receiver_window->rwnd_start;k<receiver_window->rwnd_start+RECEIVER_BUFFER_SIZE;k++)
    {
        int index = k % RECEIVER_BUFFER_SIZE;
        if (receiver_window->receiver_buffer[index].sequence_number == -1) {
            return k;
        }
        // printf("%d\n",k);
    }
    return RECEIVER_BUFFER_SIZE;

}

void handle_timeout(int socket_id, SenderWindow *sender_window, struct sockaddr_in dest_addr);
int send_message(int socket_id, Message message, struct sockaddr_in dest_addr);


void *thread_S(void *arg) {
    // MTPSocketInfo *mtp_socket_info = (MTPSocketInfo *)arg;

    while (1) {
        // Send messages from sender buffer within sender window
        usleep((int)(TIMEOUT_SECONDS*1e6/4));
       for (int i = 0; i < NUM_SOCKETS; i++) {
            // Check if the MTPSocketInfo entry is initialized
            if (!shared_memory[i].is_free) {
                // Send any unacknowledged packets in the sender buffer within the window
                // printf("start: %d end: %d \n",shared_memory[i].sender_window.swnd_start,shared_memory[i].sender_window.swnd_end);
                    // printf("end %d\n",shared_memory[i].sender_window.swnd_end);
                for (int idx = shared_memory[i].sender_window.swnd_start; idx != (shared_memory[i].sender_window.swnd_end); idx+=1) {
                    int j=idx%SENDER_BUFFER_SIZE;
                    Message *message = &(shared_memory[i].sender_window.sender_buffer[j]);
                    if(strlen(message->message)==0 || message->message==NULL)
                    {
                    // printf("no message %d\n",j);
                        continue;
                    }
                        if (sem_wait(semaphore) == -1) {
                            perror("sem_wait");
                            exit(EXIT_FAILURE);
                        }
                    message->sequence_number=message->frame_no%16;
                    // printf("num %d\n",message->sequence_number);
                    if (message->timestamp==-1 || (message->ack_received!=1 && is_timeout(message,TIMEOUT_SECONDS))) {
                        // Send message
                        message->is_ack=0;
                        // printf("message in send \n %s %d %d\n",message->message,message->sequence_number,message->timestamp);
                            uint8_t serialized_data[sizeof(Message)];
                        serializeMsg(message, serialized_data);
                        transmission++;
                        if (sendto(shared_memory[i].send_socket_id, serialized_data, sizeof(serialized_data), 0,
                                   (struct sockaddr *)&(shared_memory[i].dest_addr), sizeof(shared_memory[i].dest_addr)) == -1) {
                            perror("Error sending message");
                      }
                    //   printf("TOT TRansmission %d\n",transmission);

                            // Handle error, possibly terminate the program or take appropriate action
                  
                        message->timestamp=time(NULL);
                        message->ack_received=0;
                         // Lock the semaphore before accessing/modifying shared memory
                           // Lock the semaphore before accessing/modifying shared memory
                    }
                    if (sem_post(semaphore) == -1) {
                        perror("sem_post");
                        exit(EXIT_FAILURE);
                    }
                
                }
            }
       }

    }


    // Close socket
    // close(sender_socket_fd);

    pthread_exit(NULL);
}

void handle_timeout(int socket_id, SenderWindow *sender_window, struct sockaddr_in dest_addr) {
    // Retransmit messages in sender window
    for (int i = sender_window->swnd_start; i != (sender_window->swnd_start+SENDER_BUFFER_SIZE)%SENDER_BUFFER_SIZE; i++) {
        if (send_message(socket_id, sender_window->sender_buffer[i], dest_addr) == -1) {
            perror("Error retransmitting message");
            // Handle error, possibly terminate the program or take appropriate action
        }
    }
}

int send_message(int socket_id, Message message, struct sockaddr_in dest_addr) {
    // Send message
    int bytes_sent = sendto(socket_id, &message, sizeof(Message), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (bytes_sent == -1) {
        perror("sendto");
        return -1; // Return -1 on error
    }
    return 0; // Return 0 on success
}



int is_timeout(Message *message, int timeout_seconds) {
    // Get the current time
    time_t current_time = time(NULL);

    // Calculate the elapsed time since the message was sent
    int elapsed_time = current_time - message->timestamp;

    // Check if the elapsed time exceeds the timeout threshold
    return (elapsed_time >= timeout_seconds);
}
// Garbage collector function
void *garbage_collector(void *arg) {
    MTPSocketInfo *shared_memory = (MTPSocketInfo *)arg;

    while(1)
    {
        usleep(TIMEOUT_SECONDS*1e6/2);
    // Implement garbage collector logic here
    if (sem_wait(semaphore) == -1) {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
    int flag=1;
    for (int i = 0; i < NUM_SOCKETS; i++) {
        if(shared_memory[i].closed)
        {
        printf("clearing socket %d\n",i);
        for(int j=0;j<SENDER_BUFFER_SIZE;j++)
        {
            if(shared_memory[i].sender_window.sender_buffer[j].ack_received==0)
            {
                flag=0;
                // printf("in garbage : %d\n",shared_memory[i].sender_window.sender_buffer[j].frame_no);
            }
        }
        if(flag)
        {

        printf("closing socket %d\n",i);
        close(shared_memory[i].recv_socket_id);
        close(shared_memory[i].send_socket_id);
        resetSM(&shared_memory[i]);
        }
        // int tot_messages=shared_memory[i].sender_window.swnd_written;
        // if(transmission!=0)
        // transmission=0;
        // printf("\naverage number of transmissions to send the file: %lf\n",(double)(tot_messages)/transmission);

        }
    }

    if (sem_post(semaphore) == -1) {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
    }

    // return 0;
    pthread_exit(NULL);
}


