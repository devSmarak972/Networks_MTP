#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "msocket.h"

#define BUFFER_SIZE 1024
#define FILENAME "large_file.txt"

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


    // Read file and send data
    FILE *file = fopen(FILENAME, "rb");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
      // Convert size to string
    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0L, SEEK_SET);

    printf("Sending file of size: %zu bytes.\n", file_size);

    char buffer[BUFFER_SIZE];
    sprintf(buffer, "%zu", file_size);
    ssize_t sent_bytes = 0;
     printf("Sending file size...\n");
    sent_bytes = m_sendto(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (sent_bytes < 0) {
        perror("Error sending file size");
        fclose(file);
        m_close(sockfd);
        return 1;
    }
    

    // Send file data
    printf("Sending file data...\n");

    size_t total_bytes_sent = 0;
    sent_bytes=0;
    while (total_bytes_sent < file_size) {
        bzero(buffer,BUFFER_SIZE);
        int bytes_read = fread(buffer, 1,BUFFER_SIZE, file);
        if (bytes_read <0) {
            perror("Error reading file");
            fclose(file);
            m_close(sockfd);
            return 1;
        }

        sent_bytes = 0;
        do {

            sent_bytes = m_sendto(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            
            if (sent_bytes < 0) {
                if (errno == ENOBUFS) {
                    // Buffer full, keep trying to send
                    continue;
                } else {
                    perror("Error sending file data");
                    fclose(file);
                    m_close(sockfd);
                    return 1;
                }
            }
        } while (sent_bytes < 0);

        total_bytes_sent += bytes_read;
    }
    printf("File sent successfully.\n");

    fclose(file);
    m_close(sockfd);

    return 0;
}
