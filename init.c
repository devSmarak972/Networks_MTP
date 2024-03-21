#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
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
sem_t *sem1;
sem_t *sem2;

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

    printf("entry\n");
    key_t key = ftok("makefile", 'S'); // Generate a key for the shared memory segment
    if (key == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }
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
    
    memset(shared_memory, 0, NUM_SOCKETS * sizeof(MTPSocketInfo));

    initialize_semaphores();
     
    // Initialize shared memory structures
    // Set all MTP socket entries as free
    for (int i = 0; i < NUM_SOCKETS; i++) {
        shared_memory[i].is_free = 1;
        shared_memory[i].receiver_window.rwnd_size = RECEIVER_BUFFER_SIZE;
        shared_memory[i].receiver_window.rwnd_start = 0;
        shared_memory[i].receiver_window.rwnd_end = 0;
        shared_memory[i].receiver_window.nospace = 0; // Initialize the nospace flag
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
            shared_memory[i].bound = 0; // Mark as successfully created
        }

        // Handle bind request using semaphores
        if(shared_memory[i].bound == 2) {
            if (sem_wait(sem1) == -1) { // Wait for a bind request signal from msocket.c
                perror("sem_wait on Sem1 failed");
                sem_post(sem_c);
                continue;
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
    float p = 0.05; // Probability of dropping a message
    fd_set readfds;
    struct timeval timeout;
    int max_sd = 0;

    while (1) {
        FD_ZERO(&readfds);
        max_sd = 0;
        int cnt=0;

        for (int i = 0; i < MAX_SOCKETS; i++) {
            if(shared_memory[i].is_free) continue;
            cnt++;

            if(shared_memory[i].bound == 3) {
                int socket_fd = shared_memory[i].recv_socket_id;
                FD_SET(socket_fd, &readfds);
                max_sd = (socket_fd > max_sd) ? socket_fd : max_sd;

                if(shared_memory[i].receiver_window.rwnd_size == 0) {
                    shared_memory[i].receiver_window.nospace = 1;
                    continue;
                }
            }
        }

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0) {
            perror("select error");
        } else if (activity == 0) {
            // Timeout, check for nospace flag
            for (int i = 0; i < MAX_SOCKETS; i++) {
                if(shared_memory[i].is_free) continue;

                if(shared_memory[i].bound == 3 && shared_memory[i].receiver_window.nospace && shared_memory[i].receiver_window.rwnd_size > 0) {
                    // Send a duplicate ACK with the last acknowledged sequence number but with the updated rwnd size
                    send_acknowledgement(shared_memory[i].recv_socket_id, shared_memory[i].receiver_window.rwnd_end-1, shared_memory[i].receiver_window.rwnd_size, shared_memory[i]);

                    // Reset the nospace flag
                    shared_memory[i].receiver_window.nospace = 0;
                }
            }
        } else {
            // Activity detected
            for (int i = 0; i < MAX_SOCKETS; i++) {
                if(shared_memory[i].is_free) continue;

                if(shared_memory[i].bound == 3 && FD_ISSET(shared_memory[i].recv_socket_id, &readfds)) {
                    Message received_message;
                    uint8_t buffer[sizeof(Message)];

                    int bytes_received = recvfrom(shared_memory[i].recv_socket_id, buffer, sizeof(buffer), 0, NULL, NULL);
                    if (bytes_received < 0) {
                        perror("Error receiving data");
                    } else {
                        printf("bytes recv: %d\n",bytes_received);
                        deserializeMsg(buffer, &received_message);

                        if(dropMessage(p)){
                            continue;
                        }
                        if(received_message.is_ack)
                            process_received_acknowledgement(received_message,&shared_memory[i].sender_window);
                        else {
                            printf("----------------------\nrwnd_start: %d rwnd_end :%d rwnd size: %d\n",shared_memory[i].receiver_window.rwnd_start,shared_memory[i].receiver_window.rwnd_end,shared_memory[i].receiver_window.rwnd_size);
                            process_received_message(i,received_message, &(shared_memory[i].receiver_window),shared_memory[i]);
                            printf("**********************\nrwnd_start: %d rwnd_end :%d rwnd size: %d\n",shared_memory[i].receiver_window.rwnd_start,shared_memory[i].receiver_window.rwnd_end,shared_memory[i].receiver_window.rwnd_size);
                            if(shared_memory[i].receiver_window.rwnd_size==RECEIVER_BUFFER_SIZE)
                                printf("signalled\n\n");
                            sem_post(shared_memory[i].receiver_window.sem_recv);
                        }
                        printf("received data %s %d\n",received_message.message,received_message.sequence_number);
                        bzero(&received_message,sizeof(Message));
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
       printf("hello ack: seq num %d\n",ack.sequence_number);
    if (sem_wait(semaphore) == -1) {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
    // sender_window->rwnd_size = ack.rwnd_size;

    // Update last in-order sequence number
    int last_in_order_seq = ack.sequence_number;
    int rwnd_size = ack.rwnd_size;
    sender_window->swnd_end=sender_window->swnd_start+rwnd_size;
    sender_window->swnd_size=rwnd_size;

    // Mark buffer as free for all previous sequence numbers
    for (int i = sender_window->swnd_start; i <= last_in_order_seq; i++) {
        int index = i % SENDER_BUFFER_SIZE;
        memset(sender_window->sender_buffer[index].message,'\0',strlen(sender_window->sender_buffer[index].message));
        sender_window->sender_buffer[index].sequence_number = 0; // Mark as free
        sender_window->sender_buffer[index].timestamp = -1; // Mark as free
        sender_window->sender_buffer[index].ack_received = 1; // Mark as free
    }
    sender_window->swnd_start = last_in_order_seq + 1; // Update start index of receive window
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
    ack.is_ack=1;
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

    // Debugging output
    printf("Received message: seq num: %d, rwnd end: %d\n", received_message.sequence_number, receiver_window->rwnd_end);

    int index = received_message.sequence_number % RECEIVER_BUFFER_SIZE;

    if (received_message.sequence_number == receiver_window->rwnd_end && receiver_window->rwnd_size > 0) {
        // In-order message received
        memcpy(receiver_window->receiver_buffer[index].message, received_message.message, strlen(received_message.message) + 1);
        receiver_window->receiver_buffer[index].sequence_number = received_message.sequence_number;
        
        // Updating window and buffer
        receiver_window->rwnd_end++;
        receiver_window->rwnd_size--;

        // Debugging output
        printf("Stored in-order message at index %d. New rwnd_end: %d, rwnd_size: %d\n", index, receiver_window->rwnd_end, receiver_window->rwnd_size);

        // Checking and handling if buffer was previously full
        if (receiver_window->nospace && receiver_window->rwnd_size > 0) {
            // Resetting nospace flag and sending duplicate ACK with updated window size
            receiver_window->nospace = 0;
            printf("Buffer was full, now has space. Sending duplicate ACK.\n");
            send_acknowledgement(mtp_socket_info.recv_socket_id, receiver_window->rwnd_end - 1, receiver_window->rwnd_size, mtp_socket_info);
        } else {
            // Regular ACK for in-order message
            send_acknowledgement(mtp_socket_info.recv_socket_id, receiver_window->rwnd_end - 1, receiver_window->rwnd_size, mtp_socket_info);
        }
    } else if (received_message.sequence_number > receiver_window->rwnd_end && receiver_window->rwnd_size > 0) {
        // Out-of-order message within window and space available
        if (receiver_window->receiver_buffer[index].sequence_number == 0) {
            receiver_window->receiver_buffer[index] = received_message;
            receiver_window->rwnd_size--;
            // Debugging output for out-of-order message
            printf("Stored out-of-order message at index %d. Updated rwnd_size: %d\n", index, receiver_window->rwnd_size);
        }
        send_acknowledgement(mtp_socket_info.recv_socket_id, receiver_window->rwnd_end - 1, receiver_window->rwnd_size, mtp_socket_info);
    }
    // Else part is not needed for messages out of window range or when buffer is full; they're implicitly ignored

    if (sem_post(semaphore) == -1) {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
}


void handle_timeout(int socket_id, SenderWindow *sender_window, struct sockaddr_in dest_addr);
int send_message(int socket_id, Message message, struct sockaddr_in dest_addr);

void *thread_S(void *arg) {
    while (1) {
        usleep(SLEEP_DURATION); // Sleep for a period shorter than T/2

        for (int i = 0; i < NUM_SOCKETS; i++) {
            if (!shared_memory[i].is_free && shared_memory[i].bound == 3) { // Ensure the socket is active
                // Handle message retransmission and sending new messages
                for (int j = shared_memory[i].sender_window.swnd_start; j < shared_memory[i].sender_window.swnd_end; j++) {
                    int idx = j % SENDER_BUFFER_SIZE;
                    Message *msg = &shared_memory[i].sender_window.sender_buffer[idx];

                    if (sem_wait(semaphore) == -1) {
                        perror("sem_wait");
                        continue;
                    }

                    if (!msg->ack_received && is_timeout(msg, TIMEOUT_SECONDS)) {
                        // Retransmit message due to timeout
                        printf("Retransmitting message: %s\n", msg->message);
                        send_message(shared_memory[i].send_socket_id, *msg, shared_memory[i].dest_addr);
                        msg->timestamp = time(NULL); // Update timestamp upon retransmission
                    } else if (msg->timestamp == -1 && shared_memory[i].sender_window.swnd_size > 0) {
                        // There's space in the window, send new message
                        printf("Sending new message: %s\n", msg->message);
                        send_message(shared_memory[i].send_socket_id, *msg, shared_memory[i].dest_addr);
                        msg->timestamp = time(NULL); // Update timestamp for new message
                    }

                    if (sem_post(semaphore) == -1) {
                        perror("sem_post");
                        continue;
                    }
                }
            }
        }
    }

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

int is_timeout(Message *msg, int timeout_seconds) {
    time_t current_time = time(NULL);
    double elapsed = difftime(current_time, msg->timestamp);
    return elapsed >= timeout_seconds;
}

int send_message(int socket_id, Message msg, struct sockaddr_in dest_addr) {
    uint8_t buffer[sizeof(Message)];
    serializeMsg(&msg, buffer);
    ssize_t sent_bytes = sendto(socket_id, buffer, sizeof(buffer), 0,
                                (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (sent_bytes == -1) {
        perror("sendto");
        return -1;
    }
    return 0;
}
// Garbage collector function
void *garbage_collector(void *arg) {
    MTPSocketInfo *shared_memory = (MTPSocketInfo *)arg;

    // Implement garbage collector logic here

    pthread_exit(NULL);
}


