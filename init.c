#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "msocket.h"
#define MAX_SOCKETS 25
#define RECEIVER_BUFFER_SIZE 5

#define SENDER_BUFFER_SIZE 10
#define TIMEOUT_SECONDS 5
#define NUM_SOCKETS 25
#define SHARED_MEMORY_NAME "mtp_shared_memory"

// Shared data structure for MTP sockets

typedef struct {
    int sequence_number;
    char message[1024]; // Assuming message size is 1024 bytes
} Message;

typedef struct {
    int sequence_number;
    int rwnd_size;
} Acknowledgement;


typedef struct {
    Message receiver_buffer[RECEIVER_BUFFER_SIZE];
    int rwnd_end;
    int rwnd_size;
} ReceiverWindow;
typedef struct {
    Message sender_buffer[SENDER_BUFFER_SIZE];
    int swnd_start;
    int swnd_end;
    int swnd_size;
} SenderWindow;

typedef struct {
     int is_free;
    pid_t process_id;
    int recv_socket_id;
    char other_end_IP[20];
    int other_end_port;
    int send_socket_id;
    struct sockaddr_in dest_addr;
    ReceiverWindow receiver_window;
    SenderWindow sender_window;
} MTPSocketInfo;
// Function prototypes for threads R and S
void *thread_R(void *arg);
void *thread_S(void *arg);
void *garbage_collector(void *arg);

MTPSocketInfo *shared_memory ;
int main() {
    shared_memory= (MTPSocketInfo *)malloc(NUM_SOCKETS * sizeof(MTPSocketInfo));
    // Initialize shared memory for MTP socket information
    if (shared_memory == NULL) {
        perror("Error allocating shared memory for MTP sockets");
        exit(EXIT_FAILURE);
    }

    // Initialize shared memory structures
    // Set all MTP socket entries as free
    for (int i = 0; i < NUM_SOCKETS; i++) {
        shared_memory[i].is_free = 1;
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
    free(shared_memory);

    return 0;
}




// Function prototype
void process_received_message(int i,Message received_message, ReceiverWindow *receiver_window);

void *thread_R(void *arg) {
    MTPSocketInfo *shared_memory = (MTPSocketInfo *)arg;

    fd_set readfds;
    int max_sd = 0;

    while (1) {
        FD_ZERO(&readfds);
        max_sd = 0;

        // Add all UDP sockets to the set
        for (int i = 0; i < MAX_SOCKETS; i++) {
            if (shared_memory[i].recv_socket_id > 0) {
                FD_SET(shared_memory[i].recv_socket_id, &readfds);
                if (shared_memory[i].recv_socket_id > max_sd) {
                    max_sd = shared_memory[i].recv_socket_id;
                }
            }
        }

        // Use select to monitor for activity on UDP sockets
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Error in select");
            continue;
        }

        // Check each socket for activity
        for (int i = 0; i < MAX_SOCKETS; i++) {
            if (FD_ISSET(shared_memory[i].recv_socket_id, &readfds)) {
                Message received_message;
                // Receive message from the socket
                int bytes_received = recvfrom(shared_memory[i].recv_socket_id, &received_message, sizeof(Message), 0, NULL, NULL);
                if (bytes_received < 0) {
                    perror("Error in recvfrom");
                    continue;
                }
                // if (bytes_received == sizeof(Message)) {
                    // Process the received message
                    process_received_message(i,received_message, &(shared_memory[i].receiver_window));
                // }
            }
        }
    }

    pthread_exit(NULL);
}

void send_acknowledgement(int socket_fd, int sequence_number, int rwnd_size) {
    Acknowledgement ack;
    ack.sequence_number = sequence_number;
    ack.rwnd_size = rwnd_size;

    // Convert ACK structure to byte array
    char ack_buffer[sizeof(Acknowledgement)];
    memcpy(ack_buffer, &ack, sizeof(Acknowledgement));

    // Send ACK message
    send(socket_fd, ack_buffer, sizeof(Acknowledgement), 0);
}
// Process received message and update receiver window
void process_received_message(int i,Message received_message, ReceiverWindow *receiver_window) {
    if (received_message.sequence_number == receiver_window->rwnd_end + 1) {
        // In-order message
        int index = receiver_window->rwnd_end % RECEIVER_BUFFER_SIZE;
        if (receiver_window->receiver_buffer[index].sequence_number == 0) {
            // Message not yet received, store in receiver buffer
            receiver_window->receiver_buffer[index] = received_message;
            receiver_window->rwnd_end++;
            receiver_window->rwnd_size++;
        } else {
            // Duplicate message, still send ACK with last in-order sequence number
            send_acknowledgement(shared_memory[i].recv_socket_id, receiver_window->rwnd_end - 1);
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
        // Send ACK with last in-order sequence number
        send_acknowledgement(shared_memory[i].recv_socket_id, receiver_window->rwnd_end - 1);
    } else {
        // Message out of window range, ignore
    }
}


void handle_timeout(int socket_id, SenderWindow *sender_window, struct sockaddr_in dest_addr);
int send_message(int socket_id, Message message, struct sockaddr_in dest_addr);

void *thread_S(void *arg) {
    MTPSocketInfo *mtp_socket_info = (MTPSocketInfo *)arg;

    while (1) {
        // Send messages from sender buffer within sender window
        for (int i = mtp_socket_info->sender_window.swnd_start; i < mtp_socket_info->sender_window.swnd_end; i++) {
            if (send_message(mtp_socket_info->send_socket_id, mtp_socket_info->sender_window.sender_buffer[i], mtp_socket_info->dest_addr) == -1) {
                perror("Error sending message");
                // Handle error, possibly terminate the program or take appropriate action
            }
        }

        // Wait for ACK or timeout
        sleep(TIMEOUT_SECONDS);

        // Handle timeout if no ACK received
        handle_timeout(mtp_socket_info->send_socket_id, &(mtp_socket_info->sender_window), mtp_socket_info->dest_addr);
    }


    // Close socket
    close(sender_socket_fd);

    pthread_exit(NULL);
}

void handle_timeout(int socket_id, SenderWindow *sender_window, struct sockaddr_in dest_addr) {
    // Retransmit messages in sender window
    for (int i = sender_window->swnd_start; i < sender_window->swnd_end; i++) {
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



// Garbage collector function
void *garbage_collector(void *arg) {
    MTPSocketInfo *shared_memory = (MTPSocketInfo *)arg;

    // Implement garbage collector logic here

    pthread_exit(NULL);
}

int last_acknowledged_sequence_number = 0;

// Function to send the next packet
void send_next_packet(int socket_fd) {
    // Find the earliest timed-out, unacknowledged packet
    int next_packet_index = -1;
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    long min_timeout = LONG_MAX;
    for (int i = 0; i < SENDER_BUFFER_SIZE; i++) {
        if (!sent_packets[i].is_acknowledged && timed_out(sent_packets[i].send_time)) {
            long timeout = calculate_timeout(sent_packets[i].send_time, current_time);
            if (timeout < min_timeout) {
                min_timeout = timeout;
                next_packet_index = i;
            }
        }
    }

    // If a timed-out packet is found, resend it
    if (next_packet_index != -1) {
        send_message(socket_fd, sent_packets[next_packet_index].message);
        gettimeofday(&sent_packets[next_packet_index].send_time, NULL);
    }
}

// Function to check if a packet has timed out
int timed_out(struct timeval send_time) {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    long elapsed_time = calculate_timeout(send_time, current_time);
    return elapsed_time > TIMEOUT_THRESHOLD;
}

// Function to calculate the timeout duration
long calculate_timeout(struct timeval send_time, struct timeval current_time) {
    long elapsed_time = (current_time.tv_sec - send_time.tv_sec) * 1000000L +
                         (current_time.tv_usec - send_time.tv_usec);
    return elapsed_time;
}

// Function to handle acknowledgment
void handle_acknowledgment(int sequence_number) {
    // Mark the packet with the given sequence number as acknowledged
    for (int i = 0; i < SENDER_BUFFER_SIZE; i++) {
        if (sent_packets[i].sequence_number == sequence_number) {
            sent_packets[i].is_acknowledged = 1;
            break;
        }
    }

    // Update the last acknowledged sequence number
    last_acknowledged_sequence_number = sequence_number;
}

// Main loop to send packets
void send_packets(int socket_fd) {
    while (1) {
        // Send the next packet if it is ready
        send_next_packet(socket_fd);
        
        // Check for incoming acknowledgments
        Acknowledgement ack;
        if (receive_acknowledgement(socket_fd, &ack) == 0) {
            // Handle acknowledgment
            handle_acknowledgment(ack.sequence_number);
        }
    }
}
