#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "mtp_socket.h"

// Shared data structure
typedef struct {
    int sock_id;
    char IP[20];
    int port;
    int errno_val;
} SOCK_INFO;

// Define global variables for shared memory and semaphores
SOCK_INFO *shared_sock_info;
sem_t Sem1, Sem2;

void *thread_R(void *arg);
void *thread_S(void *arg);

int main() {
    // Initialize shared memory for SOCK_INFO
    shared_sock_info = (SOCK_INFO *)malloc(sizeof(SOCK_INFO));
    if (shared_sock_info == NULL) {
        perror("Error allocating shared memory for SOCK_INFO");
        exit(EXIT_FAILURE);
    }

    // Initialize semaphores
    if (sem_init(&Sem1, 0, 0) != 0) {
        perror("Error initializing Sem1");
        exit(EXIT_FAILURE);
    }
    if (sem_init(&Sem2, 0, 0) != 0) {
        perror("Error initializing Sem2");
        exit(EXIT_FAILURE);
    }

    // Create threads R and S
    pthread_t thread_R_id, thread_S_id;
    if (pthread_create(&thread_R_id, NULL, thread_R, NULL) != 0) {
        perror("Error creating thread R");
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&thread_S_id, NULL, thread_S, NULL) != 0) {
        perror("Error creating thread S");
        exit(EXIT_FAILURE);
    }

    // Main thread waits on Sem1
    while (1) {
        sem_wait(&Sem1);

        // Process SOCK_INFO
        if (shared_sock_info->sock_id == 0 && shared_sock_info->port == 0 && strcmp(shared_sock_info->IP, "") == 0) {
            // m_socket call
            shared_sock_info->sock_id = m_socket(AF_INET, SOCK_DGRAM, 0);
            if (shared_sock_info->sock_id < 0) {
                shared_sock_info->errno_val = errno;
            }
        } else {
            // m_bind call
            int bind_result = m_bind(shared_sock_info->sock_id, shared_sock_info->IP, shared_sock_info->port);
            if (bind_result != 0) {
                shared_sock_info->sock_id = -1;
                shared_sock_info->errno_val = errno;
            }
        }

        // Signal on Sem2
        sem_post(&Sem2);
    }

    // Should never reach here
    return 0;
}

// Thread R function
void *thread_R(void *arg) {
    pthread_exit(NULL);
}

// Thread S function
void *thread_S(void *arg) {
    pthread_exit(NULL);
}
