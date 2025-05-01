#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/time.h>


#define MTU 1500
#define BUFF_LEN 10000  //buffer size
#define PACKET_SIZE 1518
#define QUEUE_SIZE 100

#define DEBUG_PRINT(mod, fmt, ...) \
    do { if (mod) printf(fmt, ##__VA_ARGS__); } while (0)

#define udp_debug_mod 0
#define tcp_debug_mod 0

bool tcp_ack_bool = false;
pthread_mutex_t tcp_ack_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t tcp_ack_cond = PTHREAD_COND_INITIALIZER;

bool udp_ack_bool = false;
pthread_mutex_t udp_ack_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t udp_ack_cond = PTHREAD_COND_INITIALIZER;

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
#define SA struct sockaddr

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
}UDPHeader;

typedef struct MACHeader{
    uint8_t sour_mac[6];        // source
    uint8_t des_mac[6];         // destination
    uint16_t fram_typ;        // frame type
    uint32_t crc;             // crc
}MACHeader; //18bytes

typedef struct Packet
{
  struct IPHeader ipheader;
  struct UDPHeader udpheader;
  struct MACHeader macheader;
  char buffer[MTU - 28];
}Packet;

typedef struct Queue{
        char *data[QUEUE_SIZE];
        struct timeval entry_time[QUEUE_SIZE]; 
	//struct timeval data[QUEUE_SIZE];
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

void enqueue(Queue *q, const char *item, int *qlength, struct timeval *time) {
	pthread_mutex_lock(&q->mutex);
	while (q->size >= QUEUE_SIZE) {
		pthread_cond_wait(&q->cond, &q->mutex);
	}
	q->data[q->rear] = strdup(item);
	q->entry_time[q->rear] = *time;
	q->rear = (q->rear + 1) % QUEUE_SIZE;
	q->size++;
	*qlength = q->size;
	pthread_cond_signal(&q->cond); 
	pthread_mutex_unlock(&q->mutex);
}

char *dequeue(Queue *q, int *qlength, struct timeval *time) {
    pthread_mutex_lock(&q->mutex);
    while (q->size <= 0) {
        pthread_cond_wait(&q->cond, &q->mutex);
    }
    char *item = q->data[q->front];
    *time = q->entry_time[q->front];
    q->front = (q->front + 1) % QUEUE_SIZE;
    q->size--;
    *qlength = q->size;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
    return item;
}



void destroyQueue(Queue* q) {
	pthread_mutex_destroy(&q->mutex);
	pthread_cond_destroy(&q->cond);
}

void *tcp_socket_client(void *argu){
    char buf[256];
    int sock_fd, conn_fd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0){
        printf("create error\n");
        pthread_exit(0);
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(ROUTER_PORT);

    if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("bind error\n");
        pthread_exit(0);
    }

    if (listen(sock_fd, 3) < 0) {
        printf("listen error\n");
        pthread_exit(0);
    }

    conn_fd = accept(sock_fd, (struct sockaddr *)&addr, &addrlen);
    if (conn_fd < 0) {
        printf("accept error\n");
        pthread_exit(0);
    }

    struct Queue *queue = (struct Queue*)argu;
    struct timeval now;
    int qlen, cnt = 0;

    while(cnt < 20){
        recv(conn_fd, buf, sizeof(buf), 0);
        gettimeofday(&now, NULL);
        enqueue(queue, strdup(buf), &qlen, &now);
        DEBUG_PRINT(tcp_debug_mod, "[TCP] Server received TCP packet #%02d\n", cnt+1);
        printf("TCP Packet from Client #%02d\n", cnt + 1);
        printf("   QueueLength       : %d\n", qlen);
        printf("   Timestamp         : %ld.%06ld\n\n", now.tv_sec, now.tv_usec);

        cnt++;
    }

    close(conn_fd);
    close(sock_fd);
    return NULL;
}

void *tcp_socket_server(void *argu){
    int sock_fd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0){
        printf("create error\n");
        pthread_exit(0);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    if(inet_pton(AF_INET, SERVER_IP, &addr.sin_addr) <= 0) {
        printf("SERVER_IP error\n");
        pthread_exit(0);
    }

    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("connect error\n");
        pthread_exit(0);
    }

    struct Queue *queue = (struct Queue*)argu;
    struct timeval now, entry, svc_start, svc_end;
    float qtime, stime, delay = 0, avgdelay = 0;
    int qlen, cnt = 0;

    while(cnt < 20){
        char *data = dequeue(queue, &qlen, &entry);
        gettimeofday(&now, NULL);
        DEBUG_PRINT(tcp_debug_mod, "[TCP] Server received TCP packet #%02d\n", cnt+1);
        qtime = (now.tv_sec - entry.tv_sec) * 1000 + (now.tv_usec - entry.tv_usec) / 1000.0;
        gettimeofday(&svc_start, NULL);
        usleep(30 * 1000);
        gettimeofday(&svc_end, NULL);
        stime = (svc_end.tv_sec - svc_start.tv_sec) * 1000 + (svc_end.tv_usec - svc_start.tv_usec) / 1000.0;

        delay = qtime + stime;
        avgdelay = (cnt == 0) ? delay : avgdelay * 0.7 + delay * 0.3;

        send(sock_fd, data, 256, 0);

        printf("TCP Packet to Server #%02d\n", cnt + 1);
        printf("   QueueLength       : %d\n", qlen);
        printf("   QueuingTime       : %.3f ms\n", qtime);
        printf("   ServiceTime       : %.3f ms\n", stime);
        printf("   QueuingDelay      : %.3f ms\n", delay);
        printf("   AvgQueuingDelay   : %.3f ms\n\n", avgdelay);

        free(data);
        cnt++;
        usleep(5000);
    }

    close(sock_fd);
    return NULL;
}



void *tcp_ack_to_client(void *argu){
    int sock_fd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    char buf[256] = {0};
    int cnt = 0;

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock_fd < 0){
        printf("create error\n");
        pthread_exit(0);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_ACK_TO_CLIENT_PORT);
    if(inet_pton(AF_INET, SERVER_IP, &addr.sin_addr) <= 0){
        printf("SERVER_IP error\n");
        pthread_exit(0);
    }

    while(cnt < 20){
        pthread_mutex_lock(&tcp_ack_mutex);
        if(!tcp_ack_bool){
            pthread_cond_wait(&tcp_ack_cond, &tcp_ack_mutex);
        } else {
            sendto(sock_fd, buf, sizeof(buf), 0, (struct sockaddr*)&addr, addrlen);
            tcp_ack_bool = false;
            DEBUG_PRINT(tcp_debug_mod, "[TCP] Server received TCP packet #%02d\n", cnt+1);
            printf("Send TCP ACK to Client #%02d\n", cnt + 1);
            printf("   Status            : ACK sent\n\n");
            cnt++;
        }
        pthread_mutex_unlock(&tcp_ack_mutex);
    }

    close(sock_fd);
    return NULL;
}

void *tcp_ack_fr_server(void *argu){
    int sock_fd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    char buf[256];
    int cnt = 0;

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock_fd < 0){
        printf("create error\n");
        pthread_exit(0);
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(TCP_ACK_TO_ROUTER_PORT);

    if (bind(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("bind error\n");
        pthread_exit(0);
    }

    while(cnt < 20){
        recvfrom(sock_fd, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addrlen);
        pthread_mutex_lock(&tcp_ack_mutex);
        tcp_ack_bool = true;
        pthread_cond_signal(&tcp_ack_cond);
        pthread_mutex_unlock(&tcp_ack_mutex);

        printf("Receive TCP ACK from Server #%02d\n", cnt + 1);
        printf("   Status            : ACK received\n\n");
        cnt++;
    }

    close(sock_fd);
    return NULL;
}

void *udp_socket_server(void *argu) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("UDP socket_server creation failed");
        pthread_exit(NULL);
    }
    else {
        DEBUG_PRINT(udp_debug_mod, "Create success\n");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(ROUTER_PORT);
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("UDP socket_server bind failed");
        pthread_exit(NULL);
    }
    struct Queue *q = (struct Queue *)argu;
    struct timeval now;
    int qlen;
    char buf[256];
    socklen_t len = sizeof(addr);
    for (int i = 0; i < 20; i++) {
        recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&addr, &len);
        gettimeofday(&now, NULL);
        enqueue(q, strdup(buf), &qlen, &now);
        printf("UDP Received from server, Queue: %d\n", qlen);
    }

    close(sockfd);
    return NULL;
}

void *udp_socket_client(void *argu) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("UDP socket_client creation failed");
        pthread_exit(NULL);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(CLIENT_PORT);
    if (inet_pton(AF_INET, CLIENT_IP, &addr.sin_addr) <= 0) {
        perror("inet_pton client_ip failed");
        pthread_exit(NULL);
    }

    struct Queue *queue = (struct Queue *)argu;
    struct timeval t_start, t_end, t_entry;
    float delay_total = 0, delay_avg = 0, delay_queue = 0, delay_service = 0;
    int queue_len = 0;

    for (int i = 0; i < 20; i++) {
        struct timeval t_service_start, t_service_end;

        char *payload = dequeue(queue, &queue_len, &t_entry);

        gettimeofday(&t_start, NULL);
        delay_queue = (t_start.tv_sec - t_entry.tv_sec) * 1000.0 +
                      (t_start.tv_usec - t_entry.tv_usec) / 1000.0;

        gettimeofday(&t_service_start, NULL);
        usleep(100 * 1000);  // 模擬服務時間
        gettimeofday(&t_service_end, NULL);

        delay_service = (t_service_end.tv_sec - t_service_start.tv_sec) * 1000.0 +
                        (t_service_end.tv_usec - t_service_start.tv_usec) / 1000.0;

        delay_total = delay_queue + delay_service;
        delay_avg = (i == 0) ? delay_total : delay_avg * 0.7 + delay_total * 0.3;

        printf("Sent UDP Packet #%02d\n", i + 1);
        printf("   Queue Size        : %d\n", queue_len);
        printf("   Queuing Delay     : %.3f ms\n", delay_queue);
        printf("   Service Delay     : %.3f ms\n", delay_service);
        printf("   Total Delay       : %.3f ms\n", delay_total);
        printf("   Smoothed Avg Delay: %.3f ms\n\n", delay_avg);

        sendto(sockfd, payload, strlen(payload), 0, (struct sockaddr *)&addr, sizeof(addr));
        free(payload);
        usleep(5000);
    }



    close(sockfd);
    return NULL;
}

void *udp_ack_fr_client(void *argu) {
    int sockfd;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    char buffer[256];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(UDP_ACK_TO_ROUTER_PORT);
    bind(sockfd, (SA *)&addr, sizeof(addr));

    for (int i = 0; i < 20; i++) {
        recvfrom(sockfd, buffer, sizeof(buffer), 0, (SA *)&addr, &len);
        pthread_mutex_lock(&udp_ack_mutex);
        udp_ack_bool = true;
        pthread_cond_signal(&udp_ack_cond);
        pthread_mutex_unlock(&udp_ack_mutex);
        DEBUG_PRINT(udp_debug_mod, "Router received UDP ACK from client\n");
    }
    close(sockfd);
    return NULL;
}

void *udp_ack_to_server(void *argu) {
    int sockfd;
    struct sockaddr_in addr;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    addr.sin_port = htons(UDP_ACK_TO_SERVER_PORT);

    char test[256] = {0};
    int cnt = 0;
    while (cnt < 20) {
        pthread_mutex_lock(&udp_ack_mutex);
        if (!udp_ack_bool)
            pthread_cond_wait(&udp_ack_cond, &udp_ack_mutex);
        else {
            sendto(sockfd, test, sizeof(test), 0, (SA *)&addr, sizeof(addr));
            udp_ack_bool = false;
            DEBUG_PRINT(udp_debug_mod, "Router sent UDP ACK to server\n");
            cnt++;
        }
        pthread_mutex_unlock(&udp_ack_mutex);
    }
    close(sockfd);
    return NULL;
}

int main() {
    //code
//=============<Linux thread>====================//

	Queue tcpQ, udpQ;
	pthread_t threads[8];
        initQueue(&tcpQ);
        initQueue(&udpQ);
        
	// 開 thread
	pthread_create(&threads[0], NULL, tcp_socket_client, &tcpQ);
	pthread_create(&threads[1], NULL, tcp_socket_server, &tcpQ); //g
	pthread_create(&threads[2], NULL, udp_socket_client, &udpQ); //g
	pthread_create(&threads[3], NULL, udp_socket_server, &udpQ);
	pthread_create(&threads[4], NULL, tcp_ack_fr_server, NULL);
	pthread_create(&threads[5], NULL, tcp_ack_to_client, NULL);
	pthread_create(&threads[6], NULL, udp_ack_to_server, NULL);
	pthread_create(&threads[7], NULL, udp_ack_fr_client, NULL);

	// 等待所有 thread 結束
	for (int i = 0; i < 8; i++) pthread_join(threads[i], NULL);
	
	//printf("YES 1\n");
	//pthread_join(threads[0], NULL);
	//pthread_join(threads[1], NULL);
	//pthread_join(threads[2], NULL);
	//pthread_join(threads[3], NULL);
	//pthread_join(threads[4], NULL);
	//pthread_join(threads[5], NULL);
	//pthread_join(threads[6], NULL);
	//pthread_join(threads[7], NULL);
	//printf("YES 2\n");
	//pthread_join(threads[3], NULL);
	
	
	destroyQueue(&tcpQ);
	destroyQueue(&udpQ);
	
	return 0;
}
