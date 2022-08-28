#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <esp_http_server.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#ifndef NODE_NETWORK
#define NODE_NETWORK

#define LOG_FMT(x)      "%s: " x, __func__
#define MAX_BRANCH_NODES 3 // nodes with 2 sockets per
#define MSG_FMT "X%X X%x X%x;"
#define ROOT_ID 0
#define ROOT_IP "192.168.2.1"
#define NETWORK_PORT 99
#define POLL_TIMEOUT 2 * 60 * 1000  // in milliseconds
#define KEEP_ALIVE_TIMEOUT 3 * 1000
#define SOCKET_TIMEOUT 10 * 1000000 // in microseconds

enum msg_type {
    RECV_COMPLETE = 0,
    NODE_INFO,
    KEEP_ALIVE,
    POST,
    SHUTDOWN
};

enum field_name {
    NODE_ID = 0,
    MSG_TYPE,
    MSG_VALUE,
};

enum recv_status {
    FAILURE = 0,
    SUCCESS = 1
};

typedef struct node_msg {
    int node_id;
    int msg_type;
    int value;
}node_msg;

enum signal_name {
    BUTTON = 0,
    TEMP = 1,
};

int hex_to_int(char c);
void parse_msg(const char * TAG, node_msg * nm, char * buf, int len);
char * rs_string(enum recv_status rs);
char * mt_string(enum msg_type mt);
char * sn_string(enum signal_name sn);

#endif