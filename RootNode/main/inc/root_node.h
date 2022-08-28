#include "spi_lcd.h"
#include "../../public/inc/node_network.h"
#include "driver/gptimer.h"

#ifndef ROOT_NODE
#define ROOT_NODE

#define NODE_ID(X) (X >> 1)
#define NODE_INDX(X) (NODE_ID(X)) - 1
#define OFFSET(X) (int)ceil((double)LCD_H_RES/MAX_BRANCH_NODES * (-.5 * MAX_BRANCH_NODES + .5 + (double)X))

static const char *TAG = "root_main";

typedef struct branch_node {
    int msg_fd;
    int ctrl_fd;
    gptimer_handle_t timer;
    bool timeout_occurred;
}branch_node;

typedef struct root_node {
    int listen_fd;                              // fd for accepting new fds
    int timeout_fd;                             // set when timeout occurs
    struct pollfd fd_db[MAX_BRANCH_NODES * 2];  // fds used for polling
    branch_node node_db[MAX_BRANCH_NODES];  // db holding branch node info
    bool end_server;
    bool close_conn;
    bool compress_db;
    int active_cnt;
    int current_size;
    int to_remove;
    branch_widget * bw[MAX_BRANCH_NODES];
    TaskHandle_t xlv_timer;
    TaskHandle_t xpoll_db;
    TaskHandle_t xtimeout;
}root_node;

typedef struct timeout_info {
    branch_node * bn;
    root_node * rn;
}timeout_info;

esp_err_t start_node(root_node * rn);

#endif