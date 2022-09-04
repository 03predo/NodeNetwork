#include "../../public/inc/node_network.h"
#include "dht11.h"
#include "driver/gpio.h"

#ifndef BRANCH_NODE
#define BRANCH_NODE

#define MSG_ID(X) (X<<1) + 1
#define CTRL_ID(X) (X<<1)
#define NODE_ID(X) X >> 1
#define MAX_SIG 2
#define BUTTON_PIN 39
#define TEMP_PIN GPIO_NUM_4

typedef struct signal_data {
    char name[20];
    int value;
}signal_data;

typedef struct branch_node {
    int node_id;
    int msg_fd;
    int ctrl_fd;
    bool button_state;
    int temp;
    bool end_node;
    signal_data sd_db[MAX_SIG];
    SemaphoreHandle_t msg_mutex;
    SemaphoreHandle_t ctrl_mutex;
    TaskHandle_t xpoll_ctrl;
    TaskHandle_t xbutton_handler;
    TaskHandle_t xshutdown;
    TaskHandle_t xtemp_handler;
}branch_node;

esp_err_t start_node(branch_node * bn, const char * branch_ip);
#endif