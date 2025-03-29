
#ifndef SERVER_LOGIC_H
#define SERVER_LOGIC_H

#include <arpa/inet.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CLIENTS 10
#define READ_BUFFER_SIZE 1024
#define WRITE_BUFFER_SIZE 1024

// Message types definitions
#define LEAVE 'l'
#define JOIN 'j'
#define TEXT 't'
#define CONNECT_REQUEST 'c'
#define CONNECT_ACCEPT 'a'
#define CONNECT_REJECT 'r'
#define CONNECT_CONFIRM 'f'

typedef struct {
  int user_id;
  char nametag[25];
  RSA *pub_key;
  struct sockaddr_in addr;
  int sockfd;
} user_t;

typedef struct {
  int room_id;
  char room_name[25];
  user_t *leader;
  user_t *members[10];
} room_t;

typedef struct {
  char type;
  char content[256];
  int length;
} packet_core_t;

// Global variables
extern room_t *rooms[4];
extern int room_count;
extern user_t *users[MAX_CLIENTS];
extern int user_count;
extern int server_fd;
extern int current_port;

// Function declarations
user_t *create_User(struct sockaddr_in addr, int sockfd);
void accept_Connection(packet_core_t packet, int new_socket,
                       struct sockaddr_in address);
void print_Text(packet_core_t *packet);
void *receiver_thread(void *arg);
int request_Connection(const char *peer_ip, int target_port);
void send_packet(int sockfd, packet_core_t *packet);

#endif // SERVER_LOGIC_H
