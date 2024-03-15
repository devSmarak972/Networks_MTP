#include "msocket.h"
#include <stdio.h>
#include <string.h>
int main() {
    // Create MTP socket
            printf("heems\n");

    int sockfd = m_socket(AF_INET, SOCK_MTP, 0);
    if (sockfd == -1) {
        perror("m_socket");
        return 1;
    }
printf("msocket done\n");
    // Define source and destination addresses
    struct sockaddr_in source_addr, dest_addr;
    memset(&source_addr, 0, sizeof(source_addr));
    memset(&dest_addr, 0, sizeof(dest_addr));

    source_addr.sin_family = AF_INET;
    source_addr.sin_addr.s_addr = INADDR_ANY; // Use any available interface
    source_addr.sin_port = htons(12345); // Source port

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = INADDR_ANY; // Destination IP address
    dest_addr.sin_port = htons(8080); // Destination port

    // Bind source address
    if (m_bind(sockfd, source_addr, sizeof(source_addr),dest_addr) == -1) {
        perror("m_bind");
        close(sockfd);
        return 1;
    }

    printf("Socket created and bound successfully.\n");

    // Continue with further operations using the socket...

    return 0;
}
