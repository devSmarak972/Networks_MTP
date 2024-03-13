#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "mtp_socket.h"

#define MESSAGE_SIZE 1024
#define MAX_SEQUENCE_NUMBER 15
#define RECEIVER_BUFFER_SIZE 5
#define SENDER_BUFFER_SIZE 10

typedef struct {
    int sequence_number;
    char message[MESSAGE_SIZE];
} Message;

typedef struct {
    int sequence_number;
    int rwnd_size;
} Acknowledgement;

typedef struct {
    Message sender_buffer[SENDER_BUFFER_SIZE];
    int swnd_start;
    int swnd_end;
    int swnd_size;
} SenderWindow;

typedef struct {
    Message receiver_buffer[RECEIVER_BUFFER_SIZE];
    int rwnd_start;
    int rwnd_end;
    int rwnd_size;
} ReceiverWindow;

int main() {
    int sender_socket_fd, receiver_socket_fd;
    struct sockaddr_in sender_addr, receiver_addr;
    socklen_t sender_addr_len, receiver_addr_len;

    // Create sender socket
    sender_socket_fd = m_socket(AF_INET, SOCK_STREAM, 0);
    if (sender_socket_fd < 0) {
        perror("Error creating sender socket");
        exit(EXIT_FAILURE);
    }

    // Create receiver socket
    receiver_socket_fd = m_socket(AF_INET, SOCK_STREAM, 0);
    if (receiver_socket_fd < 0) {
        perror("Error creating receiver socket");
        exit(EXIT_FAILURE);
    }

    // Set up sender address
    memset(&sender_addr, 0, sizeof(sender_addr));
    sender_addr.sin_family = AF_INET;
    sender_addr.sin_addr.s_addr = INADDR_ANY;
    sender_addr.sin_port = htons(12345);

    // Set up receiver address
    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_addr.s_addr = INADDR_ANY;
    receiver_addr.sin_port = htons(12346);

    // Bind sender socket to address
    if (m_bind(sender_socket_fd, (struct sockaddr *)&sender_addr, sizeof(sender_addr)) < 0) {
        perror("Error binding sender socket");
        exit(EXIT_FAILURE);
    }

    // Bind receiver socket to address
    if (m_bind(receiver_socket_fd, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr)) < 0) {
        perror("Error binding receiver socket");
        exit(EXIT_FAILURE);
    }

    // Listen for connections on receiver socket
    if (listen(receiver_socket_fd, 1) < 0) {
        perror("Error listening on receiver socket");
        exit(EXIT_FAILURE);
    }

    // Accept connection from sender
    receiver_addr_len = sizeof(receiver_addr);
    int sender_conn_fd = m_accept(receiver_socket_fd, (struct sockaddr *)&receiver_addr, &receiver_addr_len);
    if (sender_conn_fd < 0) {
        perror("Error accepting connection from sender");
        exit(EXIT_FAILURE);
    }

    SenderWindow sender_window = {0};
    ReceiverWindow receiver_window = {0};

    // Send a message within the sender window
    Message message;
    message.sequence_number = 0;
    strcpy(message.message, "This is a message.");
    m_sendto(sender_socket_fd, &message, sizeof(Message), 0, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr));
    sender_window.sender_buffer[sender_window.swnd_end] = message;
    sender_window.swnd_end = (sender_window.swnd_end + 1) % SENDER_BUFFER_SIZE;
    sender_window.swnd_size++;

    // Receive a message within the receiver window
    Message received_message;
    m_recvfrom(sender_socket_fd, &received_message, sizeof(Message), 0, (struct sockaddr *)&sender_addr, &sender_addr_len);
    if (received_message.sequence_number == receiver_window.rwnd_end + 1) {
        receiver_window.receiver_buffer[receiver_window.rwnd_end] = received_message;
        receiver_window.rwnd_end = (receiver_window.rwnd_end + 1) % RECEIVER_BUFFER_SIZE;
        receiver_window.rwnd_size++;
    }

    // Send acknowledgement
    send_acknowledgement(sender_conn_fd, receiver_window.rwnd_end, RECEIVER_BUFFER_SIZE);

    // Close sockets
    m_close(sender_conn_fd);
    m_close(sender_socket_fd);
    m_close(receiver_socket_fd);

    return 0;
}
