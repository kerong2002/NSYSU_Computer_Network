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

int count;
char last_payload[5000] = "`abc"; // 保持上次傳輸的 payload

void tcp_msg_sender(int fd, struct sockaddr* dst){

    // payload
    char payload[5000]; // 每次都使用新的 payload 來修改
    strcpy(payload, last_payload); // 使用上一次傳輸的 payload
    for(int i = 0; i < strlen(payload); i++) {
        payload[i]++; // 修改 payload，將每個字符都加一
    }
    strncpy(last_payload, payload, 5000); // 保存本次傳輸的 payload 以供下次使用
    size_t payload_length = strlen(payload);
	
    char buffer[PACKET_SIZE] = {0}; 
    
    struct MACHeader macHeader;
    uint8_t des_mac[6] = {0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};
    uint8_t sour_mac[6] = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16};
    uint16_t fram_typ = 0x0800; // IP header type (0x0800)
    memcpy(macHeader.des_mac , des_mac , 6);
    memcpy(macHeader.sour_mac , sour_mac, 6);
    macHeader.fram_typ = htons(fram_typ);
    uint32_t crc = 0;

    struct IPHeader ipHeader;
    uint8_t version_ihl = 0x45; //4 bits Version + 4 bits Header Length
    uint8_t type_of_service = 0x00;
    uint16_t total_length = sizeof(IPHeader) + sizeof(TCPHeader) +payload_length;
    uint16_t identification = 0xAAAA;
    uint16_t flags_fragment_offset = 0x4000;
    uint8_t time_to_live = 64; //TTL
    uint8_t protocol = 0x06; // TCP (0x06)
    uint16_t header_checksum = 0x0000;
    //uint32_t options=0x0000;
    const char*source_ip_str = "10.17.164.10"; // source IP address
    const char* destination_ip_str ="10.17.89.69";//(rand() % 2 == 0) ? "10.27.79.68" : "10.17.89.69"; 
    
    
    
    // 轉換 IP address為二進制形式
    inet_pton(AF_INET, source_ip_str, &ipHeader.source_ip);
    inet_pton(AF_INET, destination_ip_str, &ipHeader.destination_ip);
    

    ipHeader.version_ihl = version_ihl;
    ipHeader.type_of_service = type_of_service;
    ipHeader.total_length = htons(total_length);
    ipHeader.identification = htons(identification);
    ipHeader.flags_fragment_offset = htons(flags_fragment_offset);
    ipHeader.time_to_live = time_to_live;
    ipHeader.protocol = protocol;
    ipHeader.header_checksum = htons(header_checksum);
    //ipHeader.options=htons(options);

    struct TCPHeader tcpHeader;
    uint16_t source_port = 12345; // Source port
    uint16_t destination_port = 80; // Destination port 
    uint32_t sequence_number = 0x00000001; // Sequence
    uint32_t ack_number = 0x12345678; // Ack
    uint8_t offset_reserved_flags = 0x14;
    uint16_t window_size = 0xFFFF; // 65535
    uint16_t checksum = 0x0000;
    uint16_t urgent_pointer = 0x0000;

    tcpHeader.source_port = htons(source_port);
    tcpHeader.destination_port = htons(destination_port);
    tcpHeader.sequence_number = htonl(sequence_number);
    tcpHeader.ack_number = htonl(ack_number);
    tcpHeader.offset_reserved_flags = offset_reserved_flags;
    tcpHeader.window_size = htons(window_size);
    tcpHeader.checksum = htons(checksum);
    tcpHeader.urgent_pointer = htons(urgent_pointer);
    
    // 將 MAC header 、IP header 和 TCP header複製到buffer
    buffer[PACKET_SIZE] = {0};
    memcpy(buffer, &macHeader, sizeof(MACHeader));
    memcpy(buffer + sizeof(MACHeader), &ipHeader, sizeof(IPHeader));
    memcpy(buffer + sizeof(MACHeader) + sizeof(IPHeader), &tcpHeader, sizeof(TCPHeader));
    memcpy(buffer + sizeof(MACHeader) + sizeof(IPHeader) + sizeof(TCPHeader), payload , payload_length);

    // send to server
    send(fd, buffer, sizeof(buffer) , 0 ); //(socket , buffer ,buffer size,  address length)
    printf("client send tcp packet\n");
}

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
		cnt+=1;
    		memset(buf, 0, BUFF_LEN);
    		len = sizeof(clent_addr);
    		count = recvfrom(fd, buf, BUFF_LEN, 0, (struct sockaddr*)&clent_addr, &len);

    		if(count == -1)
    		{
    			printf("client recieve data fail!\n");
    			return;
    		}
    		else{
    			printf("client rcv UDP packet %d !\n", cnt);
    		}


    		memcpy(packet, buf, sizeof(*packet));
    		*iphdr = packet->ipheader;
    		*udphdr = packet->udpheader;
    		*machdr = packet->macheader;

    	}

    	printf("end");
}

void print_serv_addr(struct sockaddr_in serv_addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(serv_addr.sin_addr), ip, INET_ADDRSTRLEN);
    printf("IP: %s\n", ip);
    printf("Port: %d\n", ntohs(serv_addr.sin_port));
}

void *tcp_socket(void *argu)
{
	//code
	sleep(2);
	int cnt = 0;
    int server_fd;
    struct sockaddr_in serv_addr;

    // Create Socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error.");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(ROUTER_PORT); 

    // change ip address(Internet Presentation to Numeric)
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address / Address not supported");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Try to connect Server
    if (connect(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        close(server_fd);
        pthread_exit(NULL);
    }

    print_serv_addr(serv_addr);

    while (cnt < 10) {
        tcp_msg_sender(server_fd, (struct sockaddr*)&serv_addr);
        cnt++;
        sleep(1);
    }

    close(server_fd);
    return NULL;
}

void *udp_socket(void *argu) {
    sleep(2);
    int client_fd;
    struct sockaddr_in client_addr;

    client_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_fd < 0) {
        perror("Socket creation failed");
        return NULL;
    }

    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Bind to any available interface
    client_addr.sin_port = htons(CLIENT_PORT);  // Client port for receiving packets

    if (bind(client_fd, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        perror("Socket bind failed");
        close(client_fd);
        return NULL;
    }

    rcv_UDPpacket(client_fd);
    close(client_fd);
    return NULL;
}


int main(){
	//code
//=============<Linux thread>===================//
    pthread_t tcp_thread; // TCP thread
    pthread_t udp_thread; // UDP thread

    pthread_create(&tcp_thread, NULL, &tcp_socket, NULL);
	pthread_create(&udp_thread, NULL, &udp_socket, NULL);
	pthread_join(tcp_thread, NULL);
	pthread_join(udp_thread, NULL);

	return 0;
}
