
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

// TODO move below to header file

#define PORT 8081
#define MAX_CLIENTS 10
#define READ_BUFFER_SIZE 1024
#define WRITE_BUFFER_SIZE 1024

// message types definitions

#define LEAVE 'l'
#define JOIN 'j'
#define TEXT 't'

typedef struct {
  int user_id; // local variable
  char nametag[25];
  RSA *pub_key;
  struct sockaddr_in addr;
  int sockfd;
} user_t;

typedef struct {
  int room_id; // local variable
  char room_name[25];
  user_t *leader;
  user_t *members[10]; // TODO use T-dynamic-array
} room_t;

typedef struct {
  char type;
  char message[256];
  user_t *sender;
} msg_t;

typedef struct {
  char type;
  char content[256];

} packet_core_t;

// temp structs
typedef struct {

} receive_parameter;

typedef struct {

} write_parameter;

room_t *rooms[4];
int room_count = 0;

user_t *users[5];
int user_count = 0;

int server_fd;

user_t *create_User(struct sockaddr_in addr, int sockfd) {
  user_t *user = (user_t *)malloc(sizeof(user_t));

  user->user_id = user_count;
  user->addr = addr;
  user->pub_key = 0;

  snprintf(user->nametag, sizeof(user->nametag), "User%d", user_count + 1);
  user->sockfd = sockfd;

  return user;
}

void *accept_connections(void *arg) {
  //

  return NULL;
}

void *receive_Thread() {}

int send_Text(char msg_type) { return 0; }

int request_Connection(const char *peer_ip) {
  printf("Request connection\n");

  int sockfd;
  struct sockaddr_in server_addr;

  printf("Creating socket\n");
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    printf("socket creation failed\n");
    return -1;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);

  if (inet_pton(AF_INET, peer_ip, &server_addr.sin_addr) <= 0) {
    printf("Invalid server addr error\n");
    return -1;
  }

  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    printf("Connection failed\n");
    return -1;
  }

  printf("Connected to peer at %s\n", peer_ip);

  user_t new_user;
  users[user_count] = create_User(server_addr, sockfd);
  user_count++;

  return 0;
}

int prep_Connection_Request() {
  char ip_address[16];

  printf("Request a connection:\n");
  printf("Input IP addr \n ");
  if (fgets(ip_address, sizeof(ip_address), stdin) != NULL) {
    ip_address[strcspn(ip_address, "\n")] = '\0';

    if (strlen(ip_address) > 0) {
      printf("Connecting to %s...\n", ip_address);
    } else {
      printf("Invalid IP addr entered.\n");
    }
  } else {
    printf("Error reading input.\n");
  }

  return 0;
}

void *send_Thread() {

  char write_buffer[WRITE_BUFFER_SIZE] = {0};

  while (1) {

    char msg_type;

    scanf(" %c", &msg_type);
    getchar();

    switch (msg_type) {
    case LEAVE:

      break;
    case JOIN:
      prep_Connection_Request();
      break;
    case TEXT:

      break;
    }
    msg_type = 'x';
  }

  return NULL;
}

int main(int argc, char const *argv[]) {

  pthread_t *sender_th;
  pthread_t *receiver_th;

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket create failed");
    return -1;
  }

  pthread_create(sender_th, NULL, send_Thread, NULL);
  pthread_create(receiver_th, NULL, receive_Thread, NULL);

  pthread_join(*sender_th, NULL);
  pthread_join(*receiver_th, NULL);

  close(server_fd);

  return 0;
}
