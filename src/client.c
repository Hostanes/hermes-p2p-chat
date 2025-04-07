#include "client.h"

/* NCurses Initialization */
void init_ncurses(app_state_t *state) {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);

  int maxy, maxx;
  getmaxyx(stdscr, maxy, maxx);
  state->chat_win = newwin(maxy * 3 / 4, maxx, 0, 0);
  scrollok(state->chat_win, TRUE);
  state->input_win = newwin(maxy / 4, maxx, maxy * 3 / 4, 0);
  keypad(state->input_win, TRUE);

  refresh();
  wrefresh(state->chat_win);
  wrefresh(state->input_win);
}

/* Timeout Handler */
void *join_timeout_handler(void *arg) {
  join_timeout_args_t *args = (join_timeout_args_t *)arg;
  app_state_t *state = args->state;

  sleep(JOIN_TIMEOUT);

  bool found = false;
  for (int i = 0; i < state->user_count; i++) {
    if (state->users[i].user_id == args->user_id &&
        strncmp(state->users[i].nametag, "Pending-", 8) != 0) {
      found = true;
      break;
    }
  }

  if (!found && args->retries > 0) {
    wprintw(state->chat_win, "Join timeout, retrying (%d attempts left)...\n",
            args->retries);
    wrefresh(state->chat_win);

    message_t msg;
    msg.type = MSG_JOIN_REQUEST;
    snprintf(msg.sender_name, 25, "%s", state->local_name);
    msg.data.join.requested_user_id = args->user_id;

    sendto(state->sockfd, &msg, sizeof(msg), 0,
           (struct sockaddr *)&args->dest_addr, sizeof(args->dest_addr));

    args->retries--;
    sleep(JOIN_TIMEOUT);

    if (!found && args->retries == 0) {
      wprintw(state->chat_win, "Failed to connect to %s:%d after retries\n",
              inet_ntoa(args->dest_addr.sin_addr),
              ntohs(args->dest_addr.sin_port));
      wrefresh(state->chat_win);

      for (int i = 0; i < state->user_count; i++) {
        if (state->users[i].user_id == args->user_id &&
            strncmp(state->users[i].nametag, "Pending-", 8) == 0) {
          for (int j = i; j < state->user_count - 1; j++) {
            state->users[j] = state->users[j + 1];
          }
          state->user_count--;
          break;
        }
      }
    }
  }

  free(args);
  return NULL;
}

/* Join Handling */
void send_rejection(app_state_t *state, struct sockaddr_in *addr) {
  message_t msg;
  msg.type = MSG_REJECT;
  strncpy(msg.sender_name, state->local_name, 25);
  sendto(state->sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)addr,
         sizeof(struct sockaddr_in));
}

void handle_incoming_join(app_state_t *state, message_t *msg,
                          struct sockaddr_in *addr) {
  if (state->user_count >= MAX_USERS) {
    send_rejection(state, addr);
    return;
  }

  user_t *new_user = &state->users[state->user_count++];
  new_user->user_id = state->user_count;
  strncpy(new_user->nametag, msg->sender_name, 25);
  memcpy(&new_user->addr, addr, sizeof(struct sockaddr_in));
  new_user->sockfd = state->sockfd;

  room_t *new_room = &state->rooms[state->room_count++];
  new_room->room_id = state->room_count;
  snprintf(new_room->room_name, 25, "Room%d", new_room->room_id);
  new_room->leader = new_user;
  new_room->members[0] = new_user;

  message_t response;
  response.type = MSG_JOIN_ACCEPT;
  strncpy(response.sender_name, state->local_name, 25);
  sendto(state->sockfd, &response, sizeof(response), 0, (struct sockaddr *)addr,
         sizeof(struct sockaddr_in));

  wprintw(state->chat_win, "%s has joined your chat\n", msg->sender_name);
  wrefresh(state->chat_win);
}

void handle_join_accept(app_state_t *state, message_t *msg,
                        struct sockaddr_in *addr) {
  for (int i = 0; i < state->user_count; i++) {
    if (strncmp(state->users[i].nametag, "Pending-", 8) == 0 &&
        state->users[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
        state->users[i].addr.sin_port == addr->sin_port) {

      strncpy(state->users[i].nametag, msg->sender_name, 25);

      room_t *new_room = &state->rooms[state->room_count++];
      new_room->room_id = state->room_count;
      snprintf(new_room->room_name, 25, "Room%d", new_room->room_id);
      new_room->leader = &state->users[i];
      new_room->members[0] = &state->users[i];

      wprintw(state->chat_win, "Joined %s successfully\n", msg->sender_name);
      wrefresh(state->chat_win);
      return;
    }
  }
  wprintw(state->chat_win, "Received unexpected join accept\n");
  wrefresh(state->chat_win);
}

void handle_join(app_state_t *state, const char *address_port) {
  char address[50];
  int port;

  if (sscanf(address_port, "%[^:]:%d", address, &port) != 2) {
    wprintw(state->chat_win, "Invalid address format. Use IP:PORT\n");
    wrefresh(state->chat_win);
    return;
  }

  for (int i = 0; i < state->user_count; i++) {
    if (state->users[i].addr.sin_addr.s_addr == inet_addr(address) &&
        ntohs(state->users[i].addr.sin_port) == port) {
      wprintw(state->chat_win, "Already connected to %s:%d\n", address, port);
      wrefresh(state->chat_win);
      return;
    }
  }

  if (state->user_count >= MAX_USERS) {
    wprintw(state->chat_win, "Maximum users reached\n");
    wrefresh(state->chat_win);
    return;
  }

  message_t msg;
  memset(&msg, 0, sizeof(message_t));
  msg.type = MSG_JOIN_REQUEST;
  snprintf(msg.sender_name, 25, "%s", state->local_name);
  msg.data.join.requested_user_id = state->user_count + 1;

  struct sockaddr_in dest_addr;
  memset(&dest_addr, 0, sizeof(dest_addr));
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(port);
  if (inet_pton(AF_INET, address, &dest_addr.sin_addr) <= 0) {
    wprintw(state->chat_win, "Invalid address: %s\n", address);
    wrefresh(state->chat_win);
    return;
  }

  if (sendto(state->sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&dest_addr,
             sizeof(dest_addr)) < 0) {
    wprintw(state->chat_win, "Failed to send join request\n");
    wrefresh(state->chat_win);
    perror("sendto");
    return;
  }

  user_t *pending_user = &state->users[state->user_count++];
  pending_user->user_id = state->user_count;
  snprintf(pending_user->nametag, 25, "Pending-%s", address);
  memcpy(&pending_user->addr, &dest_addr, sizeof(dest_addr));
  pending_user->sockfd = state->sockfd;

  wprintw(state->chat_win, "Join request sent to %s:%d (User ID will be %d)\n",
          address, port, pending_user->user_id);
  wrefresh(state->chat_win);

  pthread_t timeout_thread;
  join_timeout_args_t *args = malloc(sizeof(join_timeout_args_t));
  args->state = state;
  args->user_id = pending_user->user_id;
  args->retries = MAX_RETRIES;
  args->dest_addr = dest_addr;

  if (pthread_create(&timeout_thread, NULL, join_timeout_handler, args) != 0) {
    wprintw(state->chat_win, "Warning: Couldn't start timeout monitor\n");
    wrefresh(state->chat_win);
    free(args);
  } else {
    pthread_detach(timeout_thread);
  }
}

/* Message Handling */
void handle_message(app_state_t *state, int user_id, const char *message) {
  user_t *target = NULL;
  for (int i = 0; i < state->user_count; i++) {
    if (state->users[i].user_id == user_id) {
      target = &state->users[i];
      break;
    }
  }

  if (!target) {
    wprintw(state->chat_win, "User ID %d not found\n", user_id);
    wrefresh(state->chat_win);
    return;
  }

  message_t msg;
  msg.type = MSG_CHAT;
  strncpy(msg.sender_name, state->local_name, 25);
  strncpy(msg.data.chat.text, message, MAX_MSG_LEN - 50);

  if (sendto(state->sockfd, &msg, sizeof(msg), 0,
             (struct sockaddr *)&target->addr, sizeof(target->addr)) < 0) {
    perror("Send failed");
    wprintw(state->chat_win, "Failed to send message to %d\n", user_id);
    wrefresh(state->chat_win);
    return;
  }

  wprintw(state->chat_win, "You to %s: %s\n", target->nametag, message);
  wrefresh(state->chat_win);
}

/*
  reads a users input command and handles it
  TODO refactor a bit
*/
void process_command(app_state_t *state, const char *input) {
  char cmd[10];
  char arg1[50];
  char arg2[MAX_MSG_LEN];

  if (sscanf(input, "/%s %s %[^\n]", cmd, arg1, arg2) < 1) {
    wprintw(state->chat_win, "Invalid command format\n");
    wrefresh(state->chat_win);
    return;
  }

  if (strcmp(cmd, "j") == 0) {
    handle_join(state, arg1);
  } else if (strcmp(cmd, "m") == 0) {
    int user_id;
    if (sscanf(arg1, "%d", &user_id) != 1) {
      wprintw(state->chat_win, "Invalid user ID\n");
      wrefresh(state->chat_win);
      return;
    }
    handle_message(state, user_id, arg2);
  } else if (strcmp(cmd, "port") == 0) {
    wprintw(state->chat_win, "Your current port: %d\n", state->port);
    wrefresh(state->chat_win);
  } else if (strcmp(cmd, "users") == 0) {
    wprintw(state->chat_win, "Connected users:\n");
    for (int i = 0; i < state->user_count; i++) {
      wprintw(state->chat_win, "%d: %s (%s:%d)\n", state->users[i].user_id,
              state->users[i].nametag, inet_ntoa(state->users[i].addr.sin_addr),
              ntohs(state->users[i].addr.sin_port));
    }
    wrefresh(state->chat_win);
  } else if (strcmp(cmd, "help") == 0) {
    wprintw(state->chat_win, "Commands:\n");
    wprintw(state->chat_win, "/j IP:PORT - Join a chat\n");
    wprintw(state->chat_win, "/m USERID message - Send message\n");
    wprintw(state->chat_win, "/port - Show your port\n");
    wprintw(state->chat_win, "/users - List connected users\n");
    wprintw(state->chat_win, "/help - Show this help\n");
    wrefresh(state->chat_win);
  } else {
    wprintw(state->chat_win, "Unknown command: %s\n", cmd);
    wrefresh(state->chat_win);
  }
}

/*
  Receiver thread, runs alongside submitto or send thread
  handles incoming messages:
  - MSG_JOIN_REQUEST,
  - MSG_JOIN_ACCEPT,
  - MSG_CHAT,
  - MSG_PING,
  - MSG_REJECT, currently only used when max users is reached and cant accept
  any more users
*/
void *recepto(void *arg) {
  app_state_t *state = (app_state_t *)arg;
  char buffer[MAX_MSG_LEN];
  struct sockaddr_in cliaddr;
  socklen_t len = sizeof(cliaddr);

  while (1) {
    int n = recvfrom(state->sockfd, buffer, MAX_MSG_LEN, 0,
                     (struct sockaddr *)&cliaddr, &len);
    if (n > 0) {
      message_t msg;
      memcpy(&msg, buffer, sizeof(message_t));

      switch (msg.type) {
      case MSG_JOIN_REQUEST:
        handle_incoming_join(state, &msg, &cliaddr);
        break;
      case MSG_JOIN_ACCEPT:
        handle_join_accept(state, &msg, &cliaddr);
        break;
      case MSG_CHAT:
        wprintw(state->chat_win, "%s: %s\n", msg.sender_name,
                msg.data.chat.text);
        wrefresh(state->chat_win);
        break;
      case MSG_REJECT:
        for (int i = 0; i < state->user_count; i++) {
          if (strncmp(state->users[i].nametag, "Pending-", 8) == 0 &&
              state->users[i].addr.sin_addr.s_addr == cliaddr.sin_addr.s_addr) {
            wprintw(state->chat_win, "Join rejected by %s\n", msg.sender_name);
            for (int j = i; j < state->user_count - 1; j++) {
              state->users[j] = state->users[j + 1];
            }
            state->user_count--;
            break;
          }
        }
        wrefresh(state->chat_win);
        break;
      default:
        wprintw(state->chat_win, "Unknown message type\n");
        wrefresh(state->chat_win);
      }
    }
  }
  return NULL;
}

/*
  Semder thread, runs alongside recepto or receive thread
  Handles user input not just outgoing messages
  commands:
  - /j IP:PORT - Join a chat
  - /m USERID message - Send message
  - /port - Show your port
  - /users - List connected users
  - /help - Show this help
*/
void *submitto(void *arg) {
  app_state_t *state = (app_state_t *)arg;
  char input[MAX_MSG_LEN];
  int ch, pos = 0;

  while (1) {
    // ncurses functions
    wmove(state->input_win, 0, 0);
    wclrtoeol(state->input_win);
    mvwprintw(state->input_win, 0, 0, "> ");
    wrefresh(state->input_win);

    pos = 0;
    memset(input, 0, MAX_MSG_LEN);

    while ((ch = wgetch(state->input_win)) != '\n') {
      if (ch == KEY_BACKSPACE || ch == 127) {
        if (pos > 0) {
          pos--;
          input[pos] = '\0';
        }
      } else if (pos < MAX_MSG_LEN - 1) {
        input[pos++] = ch;
      }

      // ncurses functions
      wmove(state->input_win, 0, 2);
      wclrtoeol(state->input_win);
      mvwprintw(state->input_win, 0, 2, "%s", input);
      wrefresh(state->input_win);
    }

    if (input[0] == '/') {
      process_command(state, input);
    } else if (strlen(input) > 0) {
      wprintw(state->chat_win, "not a command, see /help\n");
      wrefresh(state->chat_win);
    }
  }
  return NULL;
}

/* Network Initialization */
int init_network(app_state_t *state) {
  state->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (state->sockfd < 0) {
    fprintf(stderr, "ERROR: sock creation failed");
    return -1;
  }

  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(0);

  if (bind(state->sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    fprintf(stderr, "ERROR: sock bind failed");
    return -1;
  }

  socklen_t len = sizeof(servaddr);
  if (getsockname(state->sockfd, (struct sockaddr *)&servaddr, &len) < 0) {
    perror("getsockname failed");
    return -1;
  }

  state->port = ntohs(servaddr.sin_port);
  return 0;
}

int main() {
  app_state_t state;
  memset(&state, 0, sizeof(state));
  state.port = 0;
  state.local_user_id = 1;
  snprintf(state.local_name, 25, "User%d", state.local_user_id);

  if (init_network(&state) < 0) {
    return EXIT_FAILURE;
  }

  init_ncurses(&state);
  wprintw(state.chat_win, "P2P Chat started on port %d\n", state.port);
  wprintw(state.chat_win, "Type /help for commands\n");
  wrefresh(state.chat_win);

  pthread_t receive_thread, submit_thread;

  if (pthread_create(&receive_thread, NULL, recepto, &state) != 0) {
    perror("Failed to create receive thread");
    endwin();
    return EXIT_FAILURE;
  }

  if (pthread_create(&submit_thread, NULL, submitto, &state) != 0) {
    perror("Failed to create submit thread");
    endwin();
    return EXIT_FAILURE;
  }

  pthread_join(receive_thread, NULL);
  pthread_join(submit_thread, NULL);

  close(state.sockfd);
  endwin();
  return EXIT_SUCCESS;
}
