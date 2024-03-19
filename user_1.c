#include "msocket.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char* argv[]) {
    // Create MTP socket
            printf("heems\n");

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
        printf("msocket done\n");

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
char buffer[1024];

int recv_bytes;
    printf("Socket created and bound successfully. \n");
int client_addr_len;
recv_bytes = m_recvfrom(sockfd, buffer, sizeof(buffer), 0,
                        (struct sockaddr *)&dest_addr, &client_addr_len);
  
                        
    // Continue with further operations using the socket...

    return 0;
}
