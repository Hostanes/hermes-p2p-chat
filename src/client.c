#include "client.h"

// Initialize global variables
room_t *rooms[4];
int room_count = 0;
user_t *users[MAX_CLIENTS];
int user_count = 0;
int server_fd;
int current_port = 0;

user_t *create_User(struct sockaddr_in addr, int sockfd) {
  user_t *user = (user_t *)malloc(sizeof(user_t));
  user->user_id = user_count;
  user->addr = addr;
  user->pub_key = 0;
  snprintf(user->nametag, sizeof(user->nametag), "User%d", user_count + 1);
  user->sockfd = sockfd;
  return user;
}

void accept_Connection(packet_core_t packet, int new_socket,
                       struct sockaddr_in address) {
  printf("Connection request received from %s\n", packet.content);

  packet_core_t response_packet;
  response_packet.type = CONNECT_ACCEPT;
  strcpy(response_packet.content, "Connection accepted");

  if (send(new_socket, &response_packet, sizeof(response_packet), 0) < 0) {
    perror("Send failed");
    close(new_socket);
    return;
  }

  if (recv(new_socket, &packet, sizeof(packet), 0) < 0) {
    perror("Receive failed");
    close(new_socket);
    return;
  }

  if (packet.type == CONNECT_CONFIRM) {
    users[user_count] = create_User(address, new_socket);
    user_count++;
    printf("User added to list\n");
  }
}

void print_Text(packet_core_t *packet) {
  packet->content[packet->length] = '\0';
  printf("Received message: %s\n", packet->content);
}

void *receiver_thread(void *arg) {
  struct sockaddr_in address;
  int addrlen = sizeof(address);
  int new_socket;

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(current_port);

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
    switch (packet.type) {
    case CONNECT_REQUEST:
      accept_Connection(packet, new_socket, address);
      break;
    case TEXT:
      print_Text(&packet);
      break;
    }
  }
  return NULL;
}

int request_Connection(const char *peer_ip, int target_port) {
  printf("Requesting connection to %s:%d\n", peer_ip, target_port);

  int sockfd;
  struct sockaddr_in server_addr;

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

  packet_core_t packet;
  packet.type = CONNECT_REQUEST;
  strcpy(packet.content, "Connection request");

  if (send(sockfd, &packet, sizeof(packet), 0) < 0) {
    perror("Send failed");
    close(sockfd);
    return -1;
  }

  if (recv(sockfd, &packet, sizeof(packet), 0) < 0) {
    perror("Receive failed");
    close(sockfd);
    return -1;
  }

  if (packet.type == CONNECT_ACCEPT) {
    printf("Connection accepted by peer\n");

    packet.type = CONNECT_CONFIRM;
    strcpy(packet.content, "Connection confirmed");

    if (send(sockfd, &packet, sizeof(packet), 0) < 0) {
      perror("Send failed");
      close(sockfd);
      return -1;
    }

    users[user_count] = create_User(server_addr, sockfd);
    user_count++;
    printf("User added to list\n");
  } else if (packet.type == CONNECT_REJECT) {
    printf("Connection rejected by peer\n");
  }

  close(sockfd);
  return 0;
}

void send_packet(int sockfd, packet_core_t *packet) {
  uint32_t length = htonl(packet->length);
  send(sockfd, &packet->type, sizeof(packet->type), 0);
  send(sockfd, &length, sizeof(length), 0);
  send(sockfd, packet->content, packet->length, 0);
}
