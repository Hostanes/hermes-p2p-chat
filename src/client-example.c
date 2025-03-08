
#include <arpa/inet.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// TODO move below to header file

#define PORT 8081
#define MAX_CLIENTS 10
#define READ_BUFFER_SIZE 1024
#define WRITE_BUFFER_SIZE 1024

typedef struct {
  char nametag[25];
  RSA *pub_key;
  struct sockaddr_in addr;
  int sockfd;
} user_t;

typedef struct {
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
  int sock;

} receive_parameter;

typedef struct {
  int sock;

} write_parameter;

void *receive_message(void *r_param_v) {
  char read_buffer[READ_BUFFER_SIZE] = {0};
  receive_parameter *r_param = (receive_parameter *)r_param_v;
  while (1) {
    memset(read_buffer, 0, READ_BUFFER_SIZE);
    recv(r_param->sock, read_buffer, READ_BUFFER_SIZE, 0);
    printf("Received: %s\n", read_buffer);
  }
}

void *write_message(void *w_param_v) {
  char write_buffer[WRITE_BUFFER_SIZE] = {0};
  write_parameter *w_param = (write_parameter *)w_param_v;

  while (1) {
    printf("Enter message: ");
    fgets(write_buffer, WRITE_BUFFER_SIZE, stdin);
    send(w_param->sock, write_buffer, strlen(write_buffer), 0);
    printf("Message sent\n");
  }
}

void connect_to_peer(const char *peer_ip) {
  int sock = 0;
  struct sockaddr_in serv_addr;
  pthread_t thread_read;
  pthread_t thread_write;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("\n Socket creation error \n");
    return;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);

  if (inet_pton(AF_INET, peer_ip, &serv_addr.sin_addr) <= 0) {
    printf("\nInvalid address/ Address not supported \n");
    return;
  }

  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    printf("\nConnection Failed \n");
    return;
  }

  printf("Connected to peer at %s\n", peer_ip);

  receive_parameter r_param;
  r_param.sock = sock;
  pthread_create(&thread_read, NULL, receive_message, &r_param);
  pthread_create(&thread_write, NULL, receive_message, &r_param);

  close(sock);
}

void wait_for_peer() {
  int server_fd, new_socket;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);
  char read_buffer[READ_BUFFER_SIZE] = {0};
  char write_buffer[WRITE_BUFFER_SIZE] = {0};

  pthread_t thread_read;
  pthread_t thread_write;

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 3) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  printf("Waiting for peer to connect...\n");
  if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                           (socklen_t *)&addrlen)) < 0) {
    perror("accept");
    exit(EXIT_FAILURE);
  }

  printf("Peer connected\n");

  receive_parameter r_param;
  r_param.sock = new_socket;
  pthread_create(&thread_read, NULL, receive_message, &r_param);
  pthread_create(&thread_write, NULL, receive_message, &r_param);

  pthread_join(thread_read, NULL);
  pthread_join(thread_write, NULL);

  close(new_socket);
  close(server_fd);
}

int main(int argc, char const *argv[]) {
  if (argc == 2) {
    connect_to_peer(argv[1]);
  } else {
    wait_for_peer();
  }

  return 0;
}
