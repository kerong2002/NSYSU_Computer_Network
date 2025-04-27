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

void *tcp_cts_socket(void *argu) {
    int server_fd, client_fd;
    struct sockaddr_in router_addr, server_addr;
    socklen_t addr_len = sizeof(router_addr);
    char buffer[PACKET_SIZE] = {0};
    int count = 0;
    
    // Create a TCP socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    // Set SO_REUSEADDR to allow rebinding to the same port if it's in TIME_WAIT
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEPORT) failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    // Configure router address settings
    router_addr.sin_family = AF_INET;
    router_addr.sin_addr.s_addr = INADDR_ANY;
    router_addr.sin_port = htons(ROUTER_PORT);
    
    // Bind the socket to the router address
    if (bind(server_fd, (struct sockaddr *)&router_addr, sizeof(router_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Accept an incoming connection
    client_fd = accept(server_fd, (struct sockaddr *)&router_addr, &addr_len);
    if (client_fd < 0) {
        perror("Accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Create a new socket to connect to the main server
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Failed to create server socket");
        close(client_fd);
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Set up the server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid server IP address");
        close(server_sock);
        close(client_fd);
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Connect to the main server
    if (connect(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to connect to server");
        close(server_sock);
        close(client_fd);
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Receive and forward data
    while (count < 20) {
        ssize_t bytes_received = read(client_fd, buffer, PACKET_SIZE);
		printf("Router: Received TCP\n");
        if (bytes_received > 0) {
            send(server_sock, buffer, bytes_received, 0); // Forward data to server
			printf("Router: Sent TCP\n");
        }
        count++;
    }

    // Close sockets
    close(server_sock);
    close(client_fd);
    close(server_fd);
    return NULL;
}

void *tcp_stc_ack(void *argu) {
    int server_fd, client_fd;
    struct sockaddr_in router_addr, server_addr;
    socklen_t addr_len = sizeof(router_addr);
    char buffer[PACKET_SIZE] = {0};
    int count = 0;
    
    // Create a TCP socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    // Set SO_REUSEADDR to allow rebinding to the same port if it's in TIME_WAIT
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEPORT) failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    // Configure router address settings
    router_addr.sin_family = AF_INET;
    router_addr.sin_addr.s_addr = INADDR_ANY;
    router_addr.sin_port = htons(ROUTER_PORT);
    
    // Bind the socket to the router address
    if (bind(server_fd, (struct sockaddr *)&router_addr, sizeof(router_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Accept an incoming connection
    client_fd = accept(server_fd, (struct sockaddr *)&router_addr, &addr_len);
    if (client_fd < 0) {
        perror("Accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Create a new socket to connect to the main server
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Failed to create server socket");
        close(client_fd);
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Set up the server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid server IP address");
        close(server_sock);
        close(client_fd);
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Connect to the main server
    if (connect(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to connect to server");
        close(server_sock);
        close(client_fd);
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Receive and forward data
    while (count < 20) {
        ssize_t bytes_received = read(client_fd, buffer, PACKET_SIZE);
		printf("Router: Received TCP\n");
        if (bytes_received > 0) {
            send(server_sock, buffer, bytes_received, 0); // Forward data to server
			printf("Router: Sent TCP\n");
        }
        count++;
    }

    // Close sockets
    close(server_sock);
    close(client_fd);
    close(server_fd);
    return NULL;
}



void *udp_stc_socket(void *argu) {
    //sleep(2);
    struct Packet *packet = (struct Packet*)malloc(sizeof(struct Packet));
    if (packet == NULL) {
        perror("Memory allocation failed");
        return NULL;
    }

    int router_socket, client_socket;
    struct sockaddr_in router_address, server_address, client_address;
    socklen_t server_address_length = sizeof(server_address);

    // Create a UDP socket for the router
    router_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (router_socket < 0) {
        perror("Socket creation failed");
        free(packet);
        return NULL;
    }

    // Setup router address
    memset(&router_address, 0, sizeof(router_address));
    router_address.sin_family = AF_INET;
    router_address.sin_addr.s_addr = INADDR_ANY;
    router_address.sin_port = htons(ROUTER_PORT);

    printf("Listening on UDP port %d...\n", ROUTER_PORT);

    // Bind the router socket
    if (bind(router_socket, (SA*)&router_address, sizeof(router_address)) != 0) {
        perror("Bind failed");
        close(router_socket);
        free(packet);
        return NULL;
    }
    printf("Bind successful.\n");

    // Setup client address
    memset(&client_address, 0, sizeof(client_address));
    client_address.sin_family = AF_INET;
    client_address.sin_addr.s_addr = INADDR_ANY;
    client_address.sin_port = htons(CLIENT_PORT);

    int cnt = 0;
    struct IPHeader *ipH = (struct IPHeader*)malloc(sizeof(struct IPHeader));
    if (ipH == NULL) {
        perror("Memory allocation failed");
        close(router_socket);
        free(packet);
        return NULL;
    }

    while (cnt < 20) {
        ssize_t received_bytes = recvfrom(router_socket, packet, sizeof(*packet), 0,
                                          (SA*)&server_address, &server_address_length);

        if (received_bytes < 0) {
            perror("Receive failed");
            break;
        }

        *ipH = packet->ipheader;
        printf("Received UDP packet\n");

        // 只轉發到 client1
        if (ipH->destination_ip == 0x0A000301) { // 目標 IP 是 client1
            client_socket = socket(AF_INET, SOCK_DGRAM, 0);
            if (client_socket < 0) {
                perror("Client socket creation failed");
                break;
            }

            sendto(client_socket, packet, sizeof(*packet), 0,
                   (SA*)&client_address, sizeof(client_address));
            printf("Sent to client UDP.\n");

            close(client_socket);
        }
        cnt++;
    }

    // Cleanup
    close(router_socket);
    free(packet);
    free(ipH);

    return NULL;
}



void *udp_socket_s(void *argu) {
    int client_socket;
    struct sockaddr_in client_address, router_address;
    socklen_t router_address_length = sizeof(router_address);

    struct Packet *packet = (struct Packet*)malloc(sizeof(struct Packet));
    if (packet == NULL) {
        perror("Memory allocation failed");
        return NULL;
    }

    // 創建 UDP socket
    client_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_socket < 0) {
        perror("Socket creation failed");
        free(packet);
        return NULL;
    }

    // 綁定自己的 IP 和 PORT
    memset(&client_address, 0, sizeof(client_address));
    client_address.sin_family = AF_INET;
    client_address.sin_addr.s_addr = INADDR_ANY;
    client_address.sin_port = htons(CLIENT_PORT);

    if (bind(client_socket, (SA*)&client_address, sizeof(client_address)) != 0) {
        perror("Bind failed");
        close(client_socket);
        free(packet);
        return NULL;
    }

    printf("Client waiting for UDP packet...\n");

    while (1) {
        ssize_t received_bytes = recvfrom(client_socket, packet, sizeof(*packet), 0,
                                          (SA*)&router_address, &router_address_length);
        if (received_bytes < 0) {
            perror("Receive failed");
            break;
        }

        printf("Client received packet, sending ACK...\n");

        // 準備 ACK packet
        struct Packet ack_packet;
        memset(&ack_packet, 0, sizeof(ack_packet));
        ack_packet.ipheader.source_ip = 0x0A000301;      // client IP
        ack_packet.ipheader.destination_ip = 0x0A000201; // server IP (自己設定)

        // 送回 ACK 給 Router，Router 再轉發到 Server
        sendto(client_socket, &ack_packet, sizeof(ack_packet), 0,
               (SA*)&router_address, router_address_length);
        printf("ACK sent back to Server.\n");
    }

    close(client_socket);
    free(packet);

    return NULL;
}


int main() {
    //code
//=============<Linux thread>====================//

	Queue tcpQ, udpQ;
	pthread_t threads[8];

	// 開 thread
	pthread_create(&threads[0], NULL, tcp_socket_c, &tcpQ);
	pthread_create(&threads[1], NULL, tcp_socket_s, &tcpQ);
	pthread_create(&threads[2], NULL, udp_socket_c, &udpQ);
	pthread_create(&threads[3], NULL, udp_socket_s, &udpQ);
	pthread_create(&threads[4], NULL, tcp_ack_to_c, NULL);
	pthread_create(&threads[5], NULL, tcp_ack_fr_s, NULL);
	pthread_create(&threads[6], NULL, udp_ack_to_s, NULL);
	pthread_create(&threads[7], NULL, udp_ack_fr_c, NULL);

	// 等待所有 thread 結束
	for (int i = 0; i < 8; i++) {
		pthread_join(threads[i], NULL);
	}
	
	destroyQueue(&tcpQ);
	destroyQueue(&udpQ);
	
	return 0;
}
