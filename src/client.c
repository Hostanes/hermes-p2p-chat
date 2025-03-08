
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

room_t *rooms[4];
int room_count = 0;

user_t *users[MAX_CLIENTS];
int user_count = 0;

int server_fd;
int current_port; // Current user's port

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
  struct sockaddr_in address;
  int addrlen = sizeof(address);
  int new_socket;

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(current_port); // Use the current port

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("Bind failed");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in sin;
  socklen_t len = sizeof(sin);
  if (getsockname(server_fd, (struct sockaddr *)&sin, &len) == -1) {
    perror("getsockname failed");
  } else {
    current_port = ntohs(sin.sin_port);
    printf("Server is listening on port %d\n", current_port);
  }

  if (listen(server_fd, 3) < 0) {
    perror("Listen failed");
    exit(EXIT_FAILURE);
  }

  printf("Listening for incoming connections...\n");

  while (1) {
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                             (socklen_t *)&addrlen)) < 0) {
      perror("Accept failed");
      continue;
    }
    printf("New connection accepted from %s:%d\n", inet_ntoa(address.sin_addr),
           ntohs(address.sin_port));

    packet_core_t packet;
    if (recv(new_socket, &packet, sizeof(packet), 0) < 0) {
      perror("Receive failed");
      close(new_socket);
      continue;
    }

    if (packet.type == CONNECT_REQUEST) {
      printf("Connection request received from %s\n", packet.content);

      printf("Do you want to accept the connection? (y/n): ");
      char response;
      scanf(" %c", &response);

      packet_core_t response_packet;
      if (response == 'y') {
        response_packet.type = CONNECT_ACCEPT;
        strcpy(response_packet.content, "Connection accepted");
      } else {
        response_packet.type = CONNECT_REJECT;
        strcpy(response_packet.content, "Connection rejected");
      }

      // Send response
      if (send(new_socket, &response_packet, sizeof(response_packet), 0) < 0) {
        perror("Send failed");
        close(new_socket);
        continue;
      }

      if (response == 'y') {
        // Wait for final confirmation
        if (recv(new_socket, &packet, sizeof(packet), 0) < 0) {
          perror("Receive failed");
          close(new_socket);
          continue;
        }

        if (packet.type == CONNECT_CONFIRM) {
          // Add user to list
          users[user_count] = create_User(address, new_socket);
          user_count++;
          printf("User added to list\n");
        }
      }
    }

    close(new_socket);
  }

  return NULL;
}

int request_Connection(const char *peer_ip, int target_port) {
  printf("Requesting connection to %s:%d\n", peer_ip, target_port);

  int sockfd;
  struct sockaddr_in server_addr;

  printf("Creating socket\n");
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    printf("socket creation failed\n");
    return -1;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(target_port);

  if (inet_pton(AF_INET, peer_ip, &server_addr.sin_addr) <= 0) {
    printf("Invalid server addr error\n");
    return -1;
  }

  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    printf("Connection failed\n");
    return -1;
  }

  printf("Connected to peer at %s:%d\n", peer_ip, target_port);

  // Send connection request
  packet_core_t packet;
  packet.type = CONNECT_REQUEST;
  strcpy(packet.content, "Connection request");

  if (send(sockfd, &packet, sizeof(packet), 0) < 0) {
    perror("Send failed");
    close(sockfd);
    return -1;
  }

  // Receive response
  if (recv(sockfd, &packet, sizeof(packet), 0) < 0) {
    perror("Receive failed");
    close(sockfd);
    return -1;
  }

  if (packet.type == CONNECT_ACCEPT) {
    printf("Connection accepted by peer\n");

    // Send final confirmation
    packet.type = CONNECT_CONFIRM;
    strcpy(packet.content, "Connection confirmed");

    if (send(sockfd, &packet, sizeof(packet), 0) < 0) {
      perror("Send failed");
      close(sockfd);
      return -1;
    }

    // Add user to list
    users[user_count] = create_User(server_addr, sockfd);
    user_count++;
    printf("User added to list\n");
  } else if (packet.type == CONNECT_REJECT) {
    printf("Connection rejected by peer\n");
  }

  close(sockfd);
  return 0;
}

int prep_Connection_Request() {
  char ip_address[16];
  int target_port;

  printf("Request a connection:\n");
  printf("Input IP address: ");
  if (fgets(ip_address, sizeof(ip_address), stdin) != NULL) {
    ip_address[strcspn(ip_address, "\n")] = '\0'; // Remove newline

    if (strlen(ip_address) > 0) {
      printf("Input target PORT number: ");
      scanf("%d", &target_port);
      getchar();

      printf("Connecting to %s:%d...\n", ip_address, target_port);
      request_Connection(ip_address, target_port);
    } else {
      printf("Invalid IP address entered.\n");
    }
  } else {
    printf("Error reading input.\n");
  }

  return 0;
}

void *send_Thread() {
  char write_buffer[WRITE_BUFFER_SIZE] = {0};

  printf("Input a message\n");

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
  pthread_t sender_th;
  pthread_t receiver_th;

  do {
    printf("Enter the PORT number to listen on (0 for dynamic allocation): ");
    scanf("%d", &current_port);
    getchar();
  } while (current_port < 0 || current_port > 65535);

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket create failed");
    return -1;
  }

  pthread_create(&sender_th, NULL, send_Thread, NULL);
  pthread_create(&receiver_th, NULL, accept_connections, NULL);

  pthread_join(sender_th, NULL);
  pthread_join(receiver_th, NULL);

  close(server_fd);

  return 0;
}
