#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <string>
#include <cstring>

#include <stdbool.h>
#include <sys/time.h>

using namespace std;

#define MTU 1500
#define BUFF_LEN 10000  //buffer size
#define PACKET_SIZE 1518

#define CLIENT_IP "127.0.0.1"
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9000
#define ROUTER_PORT 9002
#define CLIENT_PORT 9003
#define CLIENTTWO_PORT 9004
#define TCP_ACK_TO_ROUTER_PORT 9010
#define TCP_ACK_TO_CLIENT_PORT 9011     //for router
#define UDP_ACK_TO_ROUTER_PORT 9012
#define UDP_ACK_TO_SERVER_PORT 9013     //for router

#define udp_debug_mod 0
#define tcp_debug_mod 0
#define DEBUG_PRINT(mod, fmt, ...) do { if (mod) printf(fmt, ##__VA_ARGS__); } while (0)

#define SA struct sockaddr

#define QUEUE_SIZE 100
bool tcp_ack_bool = false;
pthread_mutex_t tcp_ack_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t tcp_ack_cond = PTHREAD_COND_INITIALIZER;
typedef struct IPHeader{
    uint8_t version_ihl;
    uint8_t type_of_service;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment_offset;
    uint8_t time_to_live;
    uint8_t protocol;
    uint16_t header_checksum;
    uint32_t source_ip;
    uint32_t destination_ip;
    uint32_t options;
    
}IPHeader;

typedef struct UDPHeader
{
  uint32_t source_port:16,
           dest_port:16;
  uint32_t Segment_Length:16,
           Checksum:16;
}UDPHeader; //8bytes

typedef struct MACHeader{
    uint8_t sour_mac[6];        // source
    uint8_t des_mac[6];         // destination
    uint16_t fram_typ;        // frame type
    uint32_t crc;             // crc
}MACHeader; //18bytes

typedef struct TCPHeader {
    uint16_t source_port;
    uint16_t destination_port;
    uint32_t sequence_number;
    uint32_t ack_number;
    uint16_t offset_reserved_flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_pointer;
}TCPHeader; //20bytes

typedef struct Packet
{
  struct IPHeader ipheader;
  struct UDPHeader udpheader;
  struct MACHeader macheader;
  char buffer[PACKET_SIZE - 46];
}Packet;  //UDP Packet


typedef struct Queue{
	struct timeval data[QUEUE_SIZE];
	int front;
	int rear; //end
	int size;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
}Queue;


void initQueue(Queue* q) {
	q->front = 0;
	q->rear = 0;
	q->size = 0;

	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond, NULL);
}

void enqueue(Queue* q, struct timeval* item) {
	pthread_mutex_lock(&q->mutex);
	while(q->size >= QUEUE_SIZE) {
		pthread_cond_wait(&q->cond, &q->mutex);
	}
	q->data[q->rear] = *item;
	q->rear = (q->rear + 1) % QUEUE_SIZE;
	q->size++;
	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex);
}

struct timeval dequeue(Queue* q){
	struct timeval item;
	pthread_mutex_lock(&q->mutex);
	while(q->size <= 0) {
		pthread_cond_wait(&q->cond, &q->mutex);
	}
	item = q->data[q->front];
	q->front = (q->front + 1) % QUEUE_SIZE;
	q->size--;
	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex);
	return item;
}

void destroyQueue(Queue* q) {
	pthread_mutex_destroy(&q->mutex);
	pthread_cond_destroy(&q->cond);
}

int count = 0;

void udp_msg_sender(int fd, struct sockaddr* dst) {
    struct MACHeader machdr = {
        .sour_mac = {0x12, 0x34, 0x56, 0x78, 0x90, 0x98},
        .des_mac = {0x21, 0x43, 0x65, 0x87, 0x90, 0x89},
        .fram_typ = 0x0000,
        .crc = 0x00000000
    };

    struct IPHeader iphdr = {
        .version_ihl = 0x45,
        .type_of_service = 0,
        .total_length = htons(MTU),
        .identification = htons(0xAAAA),
        .flags_fragment_offset = htons(0x4000),
        .time_to_live = 100,
        .protocol = 0x11,
        .header_checksum = 0,
        .source_ip = htonl(0x0A115945),
        .destination_ip = htonl(0x0A000301),
        .options = 0
    };

    struct UDPHeader udphdr = {
        .source_port = htons(10000),
        .dest_port = htons(10010),
        .Segment_Length = htons(MTU - 38),
        .Checksum = 0
    };

    struct Packet packet = {
        .ipheader = iphdr,
        .udpheader = udphdr,
        .macheader = machdr
    };

    memset(packet.buffer, 1, sizeof(packet.buffer));
    sendto(fd, &packet, sizeof(packet), 0, dst, sizeof(struct sockaddr_in));
}

void *tcp_socket(void *argu){
    //sleep(1);
    char test[256];
    int sock_fd, new_fd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0){
        printf("create error\n");
        pthread_exit(0);
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(SERVER_PORT);

    if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("tcp socket bind error\n");
        pthread_exit(0);
    }

    if (listen(sock_fd, 3) < 0) {
        printf("listen error\n");
        pthread_exit(0);
    }

    new_fd = accept(sock_fd, (struct sockaddr *)&addr, &addrlen);
    if (new_fd < 0) {
        printf("accept error\n");
        pthread_exit(0);
    }

    for (int i = 0; i < 20; i++) {
        recv(new_fd, test, sizeof(test), 0);
        pthread_mutex_lock(&tcp_ack_mutex);
        tcp_ack_bool = true;
        pthread_cond_signal(&tcp_ack_cond);
        pthread_mutex_unlock(&tcp_ack_mutex);
        DEBUG_PRINT(tcp_debug_mod, "[TCP] Server received TCP packet #%02d\n", i+1);
    }

    close(new_fd);
    close(sock_fd);
    return NULL;
}

void *tcp_ack(void *argu){
    int sock_fd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    char test[256];
    int cnt = 1;

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock_fd < 0){
        printf("create error\n");
        pthread_exit(0);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_ACK_TO_ROUTER_PORT);
    if(inet_pton(AF_INET, SERVER_IP, &addr.sin_addr) <= 0) { 
        printf("IP error\n");
        pthread_exit(0);
    }

    while(cnt <= 20){
        pthread_mutex_lock(&tcp_ack_mutex);
        if(!tcp_ack_bool){
            pthread_cond_wait(&tcp_ack_cond, &tcp_ack_mutex);
        } else {
            sendto(sock_fd, test, sizeof(test), 0, (struct sockaddr*)&addr, addrlen);
            tcp_ack_bool = false;
            DEBUG_PRINT(tcp_debug_mod, "[TCP] Server sent TCP ACK #%02d\n", cnt);
            cnt++;
        }
        pthread_mutex_unlock(&tcp_ack_mutex);
    }

    close(sock_fd);
    return NULL;
}


void *udp_socket(void *argu) {

     int sockfd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    //memset(&ser_addr, 0, sizeof(ser_addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ROUTER_PORT);
    inet_pton(AF_INET, CLIENT_IP, &addr.sin_addr);
    struct Queue *q = (struct Queue *)argu;
    sleep(2);
    char buf[256];
    for (int i = 0; i < 20; i++) {
        struct timeval now;
        gettimeofday(&now, NULL);
        enqueue(q, &now);
        //udp_msg_sender(sockfd, (struct sockaddr *)&addr);
        sendto(sockfd, buf, sizeof(buf), 0,(struct sockaddr*)&addr, (socklen_t)addrlen);
        DEBUG_PRINT(udp_debug_mod, "Server sent custom UDP packet %d\n", i+1);
        usleep(2500);
    }
    close(sockfd);
    return NULL;
}

void *udp_ack(void *argu) {
    int sockfd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(UDP_ACK_TO_SERVER_PORT);
    bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    
    struct Queue *queue = (struct Queue *)argu;
    float rtt = 0, ete = 0, avg_ete = 0;

    for (int i = 0; i < 20; i++) {
        char buffer[256];
        recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, &addrlen);

        struct timeval t_now = {}, t_then = dequeue(queue);
        gettimeofday(&t_now, NULL);

        rtt = (t_now.tv_sec - t_then.tv_sec) * 1000.0 +
              (t_now.tv_usec - t_then.tv_usec) / 1000.0;
        ete = rtt / 2.0;
        avg_ete = (i == 0) ? ete : avg_ete * 0.7 + ete * 0.3;

        printf("Receive UDP ACK #%02d\n", i + 1);
        printf("   RTT              : %.3f ms\n", rtt);
        printf("   ETE              : %.3f ms\n", ete);
        printf("   AvgETE           : %.3f ms\n\n", avg_ete);
    }

    close(sockfd);
    return NULL;
}


int main()
{
	//code
//=============<Linux thread>===================//
    Queue timestampQ;
    initQueue(&timestampQ);
    pthread_t threads[4];
    //================================//
    //           SERVER               //
    //--------------------------------//
    //    TCP -> server (tcp_socket)  //
    //    ACK <- TCP    (tcp_ack)     //
    //    UDP <- server (udp_socket)  //
    //    ACK -> UDP    (udp_ack)     //
    //================================//

    // (tcp socket) ROUNTER_PORT
    // (tcp ack)    TCP_ACK_TO_ROUTER_PORT
    // (udp socket) ROUTER_PORT
    // (udp ack)    UDP_ACK_TO_ROUTER_PORT
    
    // 開 thread
    pthread_create(&threads[0], NULL, tcp_socket, NULL);
    pthread_create(&threads[1], NULL, udp_socket, &timestampQ);
    pthread_create(&threads[2], NULL, tcp_ack,    NULL);
    
    pthread_create(&threads[3], NULL, udp_ack,    &timestampQ);

    // 等待所有 thread 結束
    for (int i = 0; i < 4; i++) pthread_join(threads[i], NULL);
    
    destroyQueue(&timestampQ);
    return 0;
}
