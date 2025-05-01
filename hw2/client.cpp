#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
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
#define BUFF_LEN 10000
#define PACKET_SIZE 1518
#define CLIENT_IP "127.0.0.1"
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9000
#define ROUTER_PORT 9002
#define CLIENT_PORT 9003
#define CLIENTTWO_PORT 9004
#define TCP_ACK_TO_ROUTER_PORT 9010
#define TCP_ACK_TO_CLIENT_PORT 9011
#define UDP_ACK_TO_ROUTER_PORT 9012
#define UDP_ACK_TO_SERVER_PORT 9013
#define QUEUE_SIZE 100

#define DEBUG_PRINT(mod, fmt, ...) \
    do { if (mod) printf(fmt, ##__VA_ARGS__); } while (0)

#define udp_debug_mod 0
#define tcp_debug_mod 0

bool udp_ack_bool = false;
pthread_mutex_t udp_ack_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t udp_ack_cond = PTHREAD_COND_INITIALIZER;

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
    //uint32_t options;

}IPHeader; //20bytes

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

int count;
char last_payload[5000] = "`abc"; // 保持上次傳輸的 payload
void rcv_UDPpacket(int fd){
	struct IPHeader *iphdr = (struct IPHeader *)malloc(sizeof(struct IPHeader));
	struct UDPHeader *udphdr = (struct UDPHeader *)malloc(sizeof(struct UDPHeader));
	struct MACHeader *machdr = (struct MACHeader *)malloc(sizeof(struct MACHeader));
	struct Packet *packet = (struct Packet *)malloc(sizeof(struct Packet));
	socklen_t len;

	struct sockaddr_in clent_addr;

	char buf[BUFF_LEN];
        int cnt = 0;
	while(cnt<20)
	{
		cnt++;
                memset(buf, 0, BUFF_LEN);
                len = sizeof(clent_addr);
                int recv_len = recvfrom(fd, buf, BUFF_LEN, 0, (struct sockaddr*)&clent_addr, &len);
                
                // Check if the receive was successful
                if (recv_len < 0)
                {
                    printf("Error receiving UDP packet!\n");
                    return;
                }

                // Debugging logs
                if (udp_debug_mod){
                printf("Received UDP packet: %d bytes\n", recv_len);
                }
                // obtain lock
                pthread_mutex_lock(&udp_ack_mutex);

                //======<critical section>=====//
                udp_ack_bool = true; // set to true when packet received
                pthread_cond_signal(&udp_ack_cond); // signal udp_ack thread
                //======<critical section>=====//

                // release lock
                pthread_mutex_unlock(&udp_ack_mutex);

                memcpy(packet, buf, sizeof(*packet));
                *iphdr = packet->ipheader;
                *udphdr = packet->udpheader;
                *machdr = packet->macheader;
	}
    	//printf("end");
}

void tcp_msg_sender(int fd) {
    // === 1. Payload 產生 ===
    char payload[256];
    static char seed_char = 'A';
    for (int i = 0; i < sizeof(payload); i++) {
        payload[i] = (seed_char + i) % 126;
        if (payload[i] < 32) payload[i] += 32; // 避免不可顯示字元
    }
    seed_char++;  // 下一次略變化
    size_t payload_length = sizeof(payload);

    // === 2. 封包建構區塊 ===
    char buffer[PACKET_SIZE] = {0};

    // MAC Header
    MACHeader mac = {
        .sour_mac = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16},
        .des_mac =  {0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F},
        .fram_typ = htons(0x0800),
        .crc = 0
    };

    // IP Header
    IPHeader ip = {
        .version_ihl = 0x45,
        .type_of_service = 0,
        .total_length = htons(sizeof(IPHeader) + sizeof(TCPHeader) + payload_length),
        .identification = htons(0xAAAA),
        .flags_fragment_offset = htons(0x4000),
        .time_to_live = 64,
        .protocol = 0x06,
        .header_checksum = 0
    };
    inet_pton(AF_INET, "10.17.164.10", &ip.source_ip);
    inet_pton(AF_INET, "10.17.89.69", &ip.destination_ip);

    // TCP Header
    TCPHeader tcp = {
        .source_port = htons(12345),
        .destination_port = htons(80),
        .sequence_number = htonl(1),
        .ack_number = htonl(0x12345678),
        .offset_reserved_flags = 0x14,
        .window_size = htons(0xFFFF),
        .checksum = htons(0),
        .urgent_pointer = htons(0)
    };

    // === 3. 組裝封包到 buffer ===
    size_t offset = 0;
    memcpy(buffer + offset, &mac, sizeof(mac)); offset += sizeof(mac);
    memcpy(buffer + offset, &ip, sizeof(ip)); offset += sizeof(ip);
    memcpy(buffer + offset, &tcp, sizeof(tcp)); offset += sizeof(tcp);
    memcpy(buffer + offset, payload, payload_length); offset += payload_length;

    // === 4. 傳送 ===
    int sent = send(fd, buffer, offset, 0);
    if (sent < 0) perror("send() failed");
}



void *tcp_socket(void *argu){
    sleep(1);
    int sock_fd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    char test[256];
    struct Queue *queue = (struct Queue *)argu;
    struct timeval current_time;

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0){
        printf("create error\n");
        pthread_exit(0);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ROUTER_PORT);
    if(inet_pton(AF_INET, SERVER_IP, &addr.sin_addr) <= 0) { 
        printf("SERVER_IP error\n");
        pthread_exit(0);
    }

    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("connect error\n");
        pthread_exit(0);
    }

    for (int i = 0; i < 20; i++) {
        gettimeofday(&current_time, NULL);
        enqueue(queue, &current_time);
        send(sock_fd, test, sizeof(test), 0);
        //tcp_msg_sender(sock_fd);
        DEBUG_PRINT(tcp_debug_mod, "[TCP] Client sent TCP packet #%02d\n", i+1);
        fflush(stdout);
        usleep(2500);
    }

    close(sock_fd);
    return NULL;
}


void *tcp_ack(void *argu){
    int sock_fd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    char test[256];
    float rtt, ete, avg_ete = 0;
    struct Queue *queue = (struct Queue *)argu;
    struct timeval now, then;

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock_fd < 0){
        printf("create error\n");
        pthread_exit(0);
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(TCP_ACK_TO_CLIENT_PORT);

    if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("bind error\n");
        pthread_exit(0);
    }

    for (int i = 0; i < 20; i++) {
        recvfrom(sock_fd, test, sizeof(test), 0, (struct sockaddr*)&addr, &addrlen);
        then = dequeue(queue);
        gettimeofday(&now, NULL);
        DEBUG_PRINT(tcp_debug_mod, "SENT TCP packet\n");
        rtt = (now.tv_sec - then.tv_sec) * 1000 + (now.tv_usec - then.tv_usec) / 1000.0;
        ete = rtt / 2.0;
        avg_ete = (i == 0) ? ete : avg_ete * 0.7 + ete * 0.3;

        printf("Receive TCP ACK #%02d\n", i+1);
        printf("   RTT              : %.3f ms\n", rtt);
        printf("   ETE              : %.3f ms\n", ete);
        printf("   AvgETE           : %.3f ms\n\n", avg_ete);

    }

    close(sock_fd);
    return NULL;
}


void* udp_socket(void* argu) {

    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock_fd < 0){
	printf("create error\n");
  	pthread_exit(0);
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(CLIENT_PORT);
    
    //sleep(2);
    if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
	printf("bind error\n");
	pthread_exit(0);
    } 
    char buf[BUFF_LEN];
    struct sockaddr_in from;
    socklen_t len = sizeof(from);

    for (int i = 0; i < 20; i++) {
        memset(buf, 0, BUFF_LEN);
        int recv_len = recvfrom(sock_fd, buf, BUFF_LEN, 0, (struct sockaddr*)&from, &len);
        DEBUG_PRINT(udp_debug_mod, "Received UDP packet: %d bytes\n", recv_len);
        pthread_mutex_lock(&udp_ack_mutex);
        udp_ack_bool = true;
        pthread_cond_signal(&udp_ack_cond);
        pthread_mutex_unlock(&udp_ack_mutex);
    }

    close(sock_fd);
    return NULL;
}


void* udp_ack(void* argu) {
    sleep(2);
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(UDP_ACK_TO_ROUTER_PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    char msg[256] = {0};
    int cnt = 0;
    while (cnt < 20) {
        pthread_mutex_lock(&udp_ack_mutex);
        if (udp_ack_bool) {
            sendto(sock_fd, msg, sizeof(msg), 0, (struct sockaddr*)&addr, sizeof(addr));
            udp_ack_bool = false;
            cnt++;
            DEBUG_PRINT(udp_debug_mod, "Send UDP ACK to router\n");
        } else {
            pthread_cond_wait(&udp_ack_cond, &udp_ack_mutex);
        }
        pthread_mutex_unlock(&udp_ack_mutex);
    }

    close(sock_fd);
    return NULL;
}

int main(){
	//code
//=============<Linux thread>===================//

    //================================//
    //           CLIENT               //
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
    Queue q;
    initQueue(&q);
    pthread_t threads[4];

    pthread_create(&threads[0], NULL, tcp_socket, &q);
    pthread_create(&threads[1], NULL, tcp_ack, &q);
    pthread_create(&threads[2], NULL, udp_socket, NULL);
    pthread_create(&threads[3], NULL, udp_ack, NULL);
    
    
    for (int i = 0; i < 4; i++) pthread_join(threads[i], NULL);
    destroyQueue(&q);
	return 0;
}
