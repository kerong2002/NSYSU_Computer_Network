#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

#define MTU 1500
#define BUFF_LEN 10000  //buffer size
#define PACKET_SIZE 1518

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

void *tcp_socket(void *argu) {
    sleep(2); // Delay to synchronize connections
    
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
    while (count < 10) {
        ssize_t bytes_received = read(client_fd, buffer, PACKET_SIZE);
        if (bytes_received > 0) {
            send(server_sock, buffer, bytes_received, 0); // Forward data to server
        }
        count++;
    }

    // Close sockets
    close(server_sock);
    close(client_fd);
    close(server_fd);
    return NULL;
}

void *udp_socket(void *argu) {
    //code
    sleep(2);
    struct Packet *packet = (struct Packet*)malloc(sizeof(struct Packet));  // Allocate memory for packet
    int router_socket;                   // Socket for the router
    int client_socket1, client_socket2;  // Sockets for clients
    struct sockaddr_in router_address;
    struct sockaddr_in server_address;
    struct sockaddr_in client1_address;
    struct sockaddr_in client2_address;
    socklen_t server_address_length;

    // Initialize server_address_length
    server_address_length = sizeof(server_address);

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
    router_address.sin_addr.s_addr = INADDR_ANY;  // Bind to all available interfaces
    router_address.sin_port = htons(ROUTER_PORT);
    printf("Listening on UDP port %d...\n", SERVER_PORT);
    // Bind the router socket to the router address and port
    if (bind(router_socket, (SA*)&router_address, sizeof(router_address)) != 0) {
        perror("Bind failed");
        close(router_socket);
        free(packet);
        return NULL;
    }
    else {
        printf("Bind successful.\n");
    }

    // Setup client1 address
    memset(&client1_address, 0, sizeof(client1_address));
    client1_address.sin_family = AF_INET;
    client1_address.sin_addr.s_addr = INADDR_ANY;  // Bind to all available interfaces
    client1_address.sin_port = htons(CLIENT_PORT);

    // Setup client2 address
    memset(&client2_address, 0, sizeof(client2_address));
    client2_address.sin_family = AF_INET;
    client2_address.sin_addr.s_addr = INADDR_ANY;  // Bind to all available interfaces
    client2_address.sin_port = htons(CLIENTTWO_PORT);

    // Server address setup (normally would be different if required)
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(SERVER_PORT);

    int cnt = 0;
    struct IPHeader *ipH = (struct IPHeader*)malloc(sizeof(struct IPHeader));
    while (cnt < 20) {
        ssize_t received_bytes = recvfrom(router_socket, packet, sizeof(*packet), 0,
                                          (SA*)&server_address, &server_address_length);

        if (received_bytes < 0) {
            perror("Receive failed");   // check receive
            break;
        }

        *ipH = packet->ipheader;  // Extract IP header from the received packet
        printf("Received UDP packet\n");

        // Check the destination IP and forward the packet accordingly
        if (ipH->destination_ip == 0x0A000301) {
            client_socket1 = socket(AF_INET, SOCK_DGRAM, 0);
            sendto(client_socket1, packet, sizeof(*packet), 0,
                   (SA*)&client1_address, sizeof(client1_address));
            printf("Sent to client1 UDP.\n");
            close(client_socket1);
        }
        else if (ipH->destination_ip == 0x0A000302) {
            client_socket2 = socket(AF_INET, SOCK_DGRAM, 0);
            sendto(client_socket2, packet, sizeof(*packet), 0,
                   (SA*)&client2_address, sizeof(client2_address));
            printf("Sent to client2 UDP.\n");
            close(client_socket2);
        }
        cnt++;
    }

    // Close the router socket and free allocated memory
    close(router_socket);
    free(packet);
    free(ipH);

    return NULL;
}

int main() {
    //code
//=============<Linux thread>====================//
    pthread_t tcp_thread; // TCP thread
    pthread_t udp_thread; // UDP thread

    pthread_create(&tcp_thread, NULL, &tcp_socket, NULL);
	pthread_create(&udp_thread, NULL, &udp_socket, NULL);
	pthread_join(tcp_thread, NULL);
	pthread_join(udp_thread, NULL);
	return 0;
}
