#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "msocket.h"
#define BUFFER_SIZE 1024
#define OUTPUT_FILE "received_file.txt"

int main(int argc, char *argv[]) {
       if(argc<5)
    {
        printf("usage: user source_ip source_port dest_ip dest_port\n");
        return 0;
    }
    int sockfd = m_socket(AF_INET, SOCK_MTP, 0);
    if (sockfd == -1) {
        perror("m_socket");
        return 1;
    }
    int port=atoi(argv[2]);
        // printf("msocket done\n");

    char* source_ip=argv[1];
    char* dest_ip=argv[3];

    int dest_port=atoi(argv[4]);
    // Define source and destination addresses
    struct sockaddr_in source_addr, dest_addr;
    memset(&source_addr, 0, sizeof(source_addr));
    memset(&dest_addr, 0, sizeof(dest_addr));

    source_addr.sin_family = AF_INET;
    source_addr.sin_addr.s_addr = inet_addr(source_ip); // Use any available interface
    source_addr.sin_port = htons(port); // Source port

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(dest_ip); // Destination IP address
    dest_addr.sin_port = htons(dest_port); // Destination port

    // Bind source address
    if (m_bind(sockfd, source_addr, sizeof(source_addr),dest_addr) == -1) {
        perror("m_bind");
        close(sockfd);
        return 1;
    }
printf("Socket created and bound successfully. \n");

    // Open output file for writing
    FILE *output_file = fopen(OUTPUT_FILE, "wb");
    if (output_file == NULL) {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }
 // Receive file size
    char buffer[BUFFER_SIZE];
    int file_size=0;
// Receive file size
ssize_t bytes_received=0;
    int client_addr_len;
    while(bytes_received<=0)
    {
    bytes_received= m_recvfrom(sockfd, buffer,BUFFER_SIZE, 0, (struct sockaddr *)&dest_addr, &client_addr_len);
    if (bytes_received < 0) {
        if(errno!=ENOMSG)
        {
        perror("Error receiving file size");
        fclose(output_file);
        m_close(sockfd);
        return 1;

        }

    }
    }
    file_size=atoi(buffer);
    printf("Receiving file with file size: %d bytes.\n", file_size);

    // Receive file data

    // Receive file data
    // ssize_t bytes_received;
    int eof_received = 0;
    size_t total_bytes_received = 0;
    bytes_received=0;
    while (total_bytes_received < file_size) {
        ssize_t bytes_to_receive = file_size - total_bytes_received;
        if (bytes_to_receive > BUFFER_SIZE) {
            bytes_to_receive = BUFFER_SIZE;
        }
        bytes_received = m_recvfrom(sockfd, buffer, bytes_to_receive, 0,
                        (struct sockaddr *)&dest_addr, &client_addr_len);
        if (bytes_received < 0) {
            if (errno == ENOMSG) {
                // No message available yet, continue waiting
                continue;
            } else {
                perror("Error receiving data");
                fclose(output_file);
                m_close(sockfd);
                return 1;
            }
        }
        // printf("recvfrom %s\n",buffer);


        fwrite(buffer, 1, bytes_received, output_file);
        total_bytes_received+=bytes_received;
        // printf("bytes received %d\n",(int)total_bytes_received);
    }
    printf("File received successfully.\n");

    fclose(output_file);
    m_close(sockfd);

    return 0;
}
