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

MTPSocketInfo *shared_memory ;
int main() {

    printf("entry\n");
     key_t key = ftok("makefile", 'S'); // Generate a key for the shared memory segment
    printf("key %d\n",key);
    // Create or access the shared memory segment
    int shmid = shmget(key, NUM_SOCKETS * sizeof(MTPSocketInfo), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Attach the shared memory segment to the process address space
  shared_memory = (MTPSocketInfo*)shmat(shmid, NULL, 0);
    if (shared_memory == (MTPSocketInfo *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    bzero(shared_memory,NUM_SOCKETS * sizeof(MTPSocketInfo));
    sem_unlink("sem");
    sem_unlink("semc");

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
    

 
    // Initialize shared memory structures
    // Set all MTP socket entries as free
    for (int i = 0; i < NUM_SOCKETS; i++) {
        shared_memory[i].is_free = 1;
        char semaphore_name[20];
        snprintf(semaphore_name, sizeof(semaphore_name), "/semrecv_%d", i); // Generate unique semaphore name
        sem_unlink(semaphore_name); // Remove named semaphore

        shared_memory[i].receiver_window.sem_recv=sem_open(semaphore_name, O_CREAT, 0666, 0);
        if(shared_memory[i].receiver_window.sem_recv==SEM_FAILED)
        {
        perror("sem failed");
        exit(EXIT_FAILURE);
        }
    }

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



    while(1)
    {

        

    for (int i = 0; i < MAX_SOCKETS; i++) {
        if(shared_memory[i].is_free==1)continue;
        if(shared_memory[i].bound==1)
        {
            if (sem_wait(sem_c) == -1) {
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
            int ssockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if (ssockfd < 0) {
                return -1;
            }
            int rsockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if (rsockfd < 0) {
                return -1;
            }
            printf("socket creation \n");

            // Initialize the socket entry
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
                printf("binding\n");
            }
            else{
                perror("error in binding");
                shared_memory[i].bound = -1;
            }
            if (sem_post(sem_c) == -1) {
            perror("sem_post");
            exit(EXIT_FAILURE);
        } 
            
        }

        }
            
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
    // MTPSocketInfo *shared_memory = (MTPSocketInfo *)arg;

    fd_set readfds;
    int max_sd = 0;

    while (1) {
        FD_ZERO(&readfds);
        max_sd = 0;
        int cnt=0;
   
        // Add all UDP sockets to the set
            // for (int i = 0; i < MAX_SOCKETS; i++) {
            //     printf("index %d i %d\n",i,shared_memory[i].is_free);
            //     if(shared_memory[i].is_free==1)
            //     {
            //         continue;

            //     }

            //     cnt++;
            //     if (shared_memory[i].recv_socket_id > 0) {
            //         FD_SET(shared_memory[i].recv_socket_id, &readfds);
            //         if (shared_memory[i].recv_socket_id > max_sd) {
            //             max_sd = shared_memory[i].recv_socket_id;
            //         }
            //     }
            // }
        // FD_SET()
        // if(cnt==0)
        // continue;

        // Use select to monitor for activity on UDP sockets
        // int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        // if (activity < 0) {
        //     perror("Error in select");
        //     continue;
        // }

        // // Check each socket for activity
        for (int i = 0; i < MAX_SOCKETS; i++) {
            if(shared_memory[i].is_free)continue;
            cnt++;
            // printf("bound %d \n",shared_memory[i].bound);

           
           if(shared_memory[i].bound==3)
            {
            // if (FD_ISSET(shared_memory[i].recv_socket_id, &readfds)) {
                Message received_message;
                // Receive message from the socket
                uint8_t buffer[sizeof(Message)];

                int bytes_received = recvfrom(shared_memory[i].recv_socket_id, buffer, sizeof(buffer), MSG_DONTWAIT, NULL, NULL);
                if (bytes_received < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No data available yet, handle accordingly
                    // printf("No data available yet\n");
                } else {
                    perror("Error receiving data");
                    continue;   
                }
                }
                else if(bytes_received>0)
                {
                    printf("bytes recv: %d\n",bytes_received);
                    deserializeMsg(buffer, &received_message);
                    received_message.is_ack=0;
                    if(received_message.is_ack)
                    process_received_acknowledgement(received_message,&shared_memory[i].sender_window);
                    else
                    process_received_message(i,received_message, &(shared_memory[i].receiver_window),shared_memory[i]);
                    printf("received data %s %d\n",received_message.message,received_message.sequence_number);
                }
                else
                {
                    printf("No dat\n");
                    // no data received
                }
                // if (bytes_received == sizeof(Message)) {
                    // Process the received message
                // }
            // }
            }
            // printf("bound %d \n",shared_memory[i].bound);
        }
                // printf("Num Sockets: %d\n",cnt);


    }
    

    pthread_exit(NULL);
}

void process_received_acknowledgement(Message ack, ReceiverWindow *receiver_window) {
    // Update rwnd size
       // Lock the semaphore before accessing/modifying shared memory
       printf("hello\n");
    if (sem_wait(semaphore) == -1) {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
    receiver_window->rwnd_size = ack.rwnd_size;

    // Update last in-order sequence number
    int last_in_order_seq = ack.sequence_number;

    // Mark buffer as free for all previous sequence numbers
    for (int i = receiver_window->rwnd_start; i <= last_in_order_seq; i++) {
        int index = i % SENDER_BUFFER_SIZE;
        receiver_window->receiver_buffer[index].sequence_number = 0; // Mark as free
        receiver_window->receiver_buffer[index].timestamp = -1; // Mark as free
        receiver_window->receiver_buffer[index].ack_received = 1; // Mark as free
    }
    receiver_window->rwnd_start = last_in_order_seq + 1; // Update start index of receive window
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

    // Convert ACK structure to byte array
    char ack_buffer[sizeof(Message)];
    memcpy(ack_buffer, &ack, sizeof(Message));

    // Send ACK message

    if(sendto(socket_fd, ack_buffer, sizeof(Message), 0,(struct sockaddr *)&(mtp_socket_info.dest_addr), sizeof(mtp_socket_info.dest_addr)) == -1)
    {
        perror("ack send failed");
    }

    // send(socket_fd, ack_buffer, sizeof(Message), 0);
}
// Process received message and update receiver window
void process_received_message(int i,Message received_message, ReceiverWindow *receiver_window,MTPSocketInfo mtp_socket_info) {
       // Lock the semaphore before accessing/modifying shared memory
    if (sem_wait(semaphore) == -1) {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
    printf("recevd message: %d %d\n",received_message.sequence_number,receiver_window->rwnd_end);
    if (received_message.sequence_number == receiver_window->rwnd_end ) {
        // In-order message
        int index = receiver_window->rwnd_end % RECEIVER_BUFFER_SIZE;
        
        if (receiver_window->receiver_buffer[index].sequence_number == 0) {
            // Message not yet received, store in receiver buffer
            memcpy(receiver_window->receiver_buffer[index].message,received_message.message,sizeof received_message);
            printf("here %d %s\n",index,receiver_window->receiver_buffer[index].message);
            receiver_window->receiver_buffer[index].sequence_number=received_message.sequence_number;
            receiver_window->rwnd_end++;
            receiver_window->rwnd_size++;
            sem_post(shared_memory[i].receiver_window.sem_recv);
        } else {
            int idx;
            // Duplicate message, still send ACK with last in-order sequence number
            // for( idx=0;idx<RECEIVER_BUFFER_SIZE;idx++)
            // {
            //     if(receiver_window->receiver_buffer[idx].sequence_number==0)
            //     {
            //         break;
            //     }
            // }
            send_acknowledgement(shared_memory[i].recv_socket_id, receiver_window->rwnd_end-1,receiver_window->rwnd_size,mtp_socket_info);
        }
    } else if (received_message.sequence_number > receiver_window->rwnd_end &&
               received_message.sequence_number <= receiver_window->rwnd_end + RECEIVER_BUFFER_SIZE) {
        // Out-of-order message
        int index = received_message.sequence_number % RECEIVER_BUFFER_SIZE;
        if (receiver_window->receiver_buffer[index].sequence_number == 0) {
            // Store if within window and not a duplicate
            receiver_window->receiver_buffer[index] = received_message;
            receiver_window->rwnd_size++;
        }
        int idx;
            // Duplicate message, still send ACK with last in-order sequence number
          
        // Send ACK with last in-order sequence number
        send_acknowledgement(shared_memory[i].recv_socket_id,receiver_window->rwnd_end-1, receiver_window->rwnd_size,mtp_socket_info);
    } else {
        // Message out of window range, ignore
    }
       // Lock the semaphore before accessing/modifying shared memory
    if (sem_post(semaphore) == -1) {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
}


void handle_timeout(int socket_id, SenderWindow *sender_window, struct sockaddr_in dest_addr);
int send_message(int socket_id, Message message, struct sockaddr_in dest_addr);

void *thread_S(void *arg) {
    // MTPSocketInfo *mtp_socket_info = (MTPSocketInfo *)arg;

    while (1) {
        // Send messages from sender buffer within sender window

       for (int i = 0; i < NUM_SOCKETS; i++) {
            // Check if the MTPSocketInfo entry is initialized
            if (!shared_memory[i].is_free) {
                // Send any unacknowledged packets in the sender buffer within the window
                for (int j = shared_memory[i].sender_window.swnd_start; j != (shared_memory[i].sender_window.swnd_end+SENDER_BUFFER_SIZE)%SENDER_BUFFER_SIZE; j++) {
                    Message *message = &(shared_memory[i].sender_window.sender_buffer[j]);
                    message->sequence_number=j%SENDER_BUFFER_SIZE;
                    if(message==NULL)continue;

                    if (message->timestamp==-1 || (!message->ack_received && is_timeout(message,TIMEOUT_SECONDS))) {
                        // Send message
                        printf("message in send \n %s\n",message->message);
                            uint8_t serialized_data[sizeof(Message)];
                    serializeMsg(message, serialized_data);
                        if (sendto(shared_memory[i].send_socket_id, serialized_data, sizeof(serialized_data), 0,
                                   (struct sockaddr *)&(shared_memory[i].dest_addr), sizeof(shared_memory[i].dest_addr)) == -1) {
                            perror("Error sending message");

                            // Handle error, possibly terminate the program or take appropriate action
                  
                      }
                    deserializeMsg(serialized_data,message);
                    printf("deserialized: %d %s\n",message->sequence_number,message->message);
                        message->timestamp=time(NULL);
                        message->ack_received=0;
                      memset(message->message,'\0',strlen(message->message));
                         // Lock the semaphore before accessing/modifying shared memory
                        if (sem_wait(semaphore) == -1) {
                            perror("sem_wait");
                            exit(EXIT_FAILURE);
                        }
                        message->timestamp=time(NULL);
                           // Lock the semaphore before accessing/modifying shared memory
                    if (sem_post(semaphore) == -1) {
                        perror("sem_post");
                        exit(EXIT_FAILURE);
                    }
                    }
                
                }
            }
       }

        // Handle timeout if no ACK received
        // handle_timeout(mtp_socket_info->send_socket_id, &(mtp_socket_info->sender_window), mtp_socket_info->dest_addr);
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

    // Implement garbage collector logic here

    pthread_exit(NULL);
}


