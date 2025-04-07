
#include <arpa/inet.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <openssl/evp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MAX_USERS 10
#define MAX_ROOMS 10
#define MAX_MSG_LEN 1024
#define JOIN_TIMEOUT 5
#define MAX_RETRIES 3

typedef enum {
  MSG_JOIN_REQUEST,
  MSG_JOIN_ACCEPT,
  MSG_CHAT,
  MSG_PING,
  MSG_REJECT
} message_type_t;

typedef struct {
  message_type_t type;
  char sender_name[25];
  union {
    struct {
      int requested_user_id;
    } join;
    struct {
      char text[MAX_MSG_LEN - 50];
    } chat;
  } data;
} message_t;

typedef struct {
  int user_id;
  char nametag[25];
  EVP_PKEY *pub_key;
  struct sockaddr_in addr;
  int sockfd;
} user_t;

typedef struct {
  int room_id;
  char room_name[25];
  user_t *leader;
  user_t *members[2];
} room_t;

typedef struct {
  user_t users[MAX_USERS];
  room_t rooms[MAX_ROOMS];
  int user_count;
  int room_count;
  int sockfd;
  int port;
  char local_name[25];
  int local_user_id;
  WINDOW *chat_win;
  WINDOW *input_win;
} app_state_t;

typedef struct {
  app_state_t *state;
  int user_id;
  int retries;
  struct sockaddr_in dest_addr;
} join_timeout_args_t;
