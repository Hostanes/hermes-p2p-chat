
#include "chat-gui.h"

int prep_Connection_Request() {
  char ip_address[16];
  int target_port;

  printf("Request a connection:\n");
  printf("Input IP address: ");
  if (fgets(ip_address, sizeof(ip_address), stdin) != NULL) {
    ip_address[strcspn(ip_address, "\n")] = '\0';

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

int send_Message() {
  printf("Sending a message\n");
  int user_id;
  printf("Input user ID: ");
  scanf("%d", &user_id);
  getchar();

  if (user_id < 0 || user_id >= user_count) {
    printf("Invalid user ID\n");
    return -1;
  }

  user_t *user = users[user_id];
  char write_buffer[WRITE_BUFFER_SIZE] = {0};

  printf("Enter message: ");
  if (fgets(write_buffer, WRITE_BUFFER_SIZE, stdin) == NULL) {
    perror("Failed to read input");
    return -1;
  }

  size_t len = strlen(write_buffer);
  if (len > 0 && write_buffer[len - 1] == '\n') {
    write_buffer[len - 1] = '\0';
    len--;
  }

  packet_core_t *packet = malloc(sizeof(packet_core_t));
  if (!packet) {
    perror("Failed to allocate memory for packet");
    return -1;
  }

  packet->type = TEXT;
  packet->length = len;
  strncpy(packet->content, write_buffer, sizeof(packet->content) - 1);
  packet->content[sizeof(packet->content) - 1] = '\0';

  send_packet(user->sockfd, packet);
  free(packet);

  printf("Message sent\n");
  return 0;
}

void *send_Thread() {
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
      send_Message();
      break;
    }
    msg_type = 'x';
  }
  return NULL;
}

int main(int argc, char const *argv[]) {
  pthread_t sender_th;
  pthread_t receiver_th;

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket create failed");
    return -1;
  }

  pthread_create(&sender_th, NULL, send_Thread, NULL);
  pthread_create(&receiver_th, NULL, receiver_thread, NULL);

  pthread_join(sender_th, NULL);
  pthread_join(receiver_th, NULL);

  close(server_fd);
  return 0;
}
