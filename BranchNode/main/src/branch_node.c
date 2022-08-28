#include "branch_node.h"

static const char *TAG = "branch_node";

void IRAM_ATTR button_isr_handler(void* parameters) {
    branch_node * bn = (branch_node *) parameters;
    bn->button_state = true;
    xTaskResumeFromISR(bn->xbutton_handler);
}

void shutdown_handler(void * parameters){
    branch_node * bn = (branch_node *) parameters;
    vTaskSuspend(NULL);
    bn->end_node = true;
    gpio_isr_handler_remove(39);
    vTaskDelete(bn->xbutton_handler);
    xSemaphoreTake(bn->ctrl_mutex, portMAX_DELAY);
    vTaskDelete(bn->xpoll_ctrl);
    xSemaphoreGive(bn->ctrl_mutex);
    xSemaphoreTake(bn->msg_mutex, portMAX_DELAY);
    vTaskDelete(bn->xpost);
    xSemaphoreGive(bn->msg_mutex);
    close(bn->ctrl_fd);
    close(bn->msg_fd);
    ESP_LOGI(TAG, LOG_FMT("branch shutdown complete"));
    vTaskDelete(NULL);

}

void msg_handler(branch_node * bn, node_msg * nm){
    int ret = -1;
    char buf[20];
    switch(nm->msg_type){
        case RECV_COMPLETE:
            ESP_LOGI(TAG, LOG_FMT("received %s %s from BranchID=%d"), mt_string((enum msg_type) nm->msg_type), rs_string((enum recv_status) nm->value), NODE_ID(nm->node_id));
            switch(nm->value){
                case SUCCESS:
                    break;
                default:
                    close(bn->ctrl_fd);
                    bn->ctrl_fd = -1;
                    close(bn->msg_fd);
                    bn->msg_fd = -1;
                    break;
            }
            break;
        case KEEP_ALIVE:
            ESP_LOGI(TAG, LOG_FMT("received %s from BranchID=%d"), mt_string((enum msg_type) nm->msg_type), NODE_ID(nm->node_id));
            xSemaphoreTake(bn->msg_mutex, portMAX_DELAY);
            xSemaphoreGive(bn->msg_mutex);
            vTaskResume(bn->xpost);
            sprintf(buf, MSG_FMT, CTRL_ID(bn->node_id), RECV_COMPLETE, SUCCESS);
            ret = send(bn->ctrl_fd, buf, sizeof(buf), 0);
            if(ret < 0){
                ESP_LOGI(TAG, LOG_FMT("error in recv (%d)"), errno);
                break;
            }
            break;
        case SHUTDOWN:
            ESP_LOGI(TAG, LOG_FMT("received %s from BranchID=%d"), mt_string((enum msg_type) nm->msg_type), NODE_ID(nm->node_id));
            vTaskResume(bn->xshutdown);
            

    }
}

void poll_ctrl(void * parameters){
    branch_node * bn = (branch_node *) parameters;
    while(1){
        xSemaphoreTake(bn->ctrl_mutex, portMAX_DELAY);
        struct pollfd fd_db[1];
        fd_db[0].fd = bn->ctrl_fd;
        fd_db[0].events = POLLIN;
        ESP_LOGI(TAG, "waiting on poll");
        int ret = poll(fd_db, 1, 2 * 60 * 1000);
        if(ret < 0){
            ESP_LOGE(TAG, LOG_FMT("error in poll (%d)"), errno);
            break;
        }else if(ret == 0){
            ESP_LOGW(TAG, LOG_FMT("timeout occured"));
        }
        char buf[20];
        ret = recv(bn->ctrl_fd, buf, sizeof(buf), 0);
        if(ret < 0){
            if(errno != EWOULDBLOCK){
                ESP_LOGE(TAG, LOG_FMT("error in recv (%d)"), errno);
                break;
            }
        }
        if(ret == 0){
            ESP_LOGI(TAG, LOG_FMT("connection closed on %d"), bn->ctrl_fd);
            break;
        }
        ESP_LOGI(TAG, LOG_FMT("%d bytes received"), ret);
        node_msg nm;
        parse_msg(TAG, &nm, buf, ret);
        msg_handler(bn, &nm);
        xSemaphoreGive(bn->ctrl_mutex);
        if(bn->end_node){
            ESP_LOGI(TAG, LOG_FMT("suspending"));
            vTaskSuspend(NULL);
        }
    }
    vTaskDelete(NULL);
}

void post(void * parameters){
    branch_node * bn = (branch_node *) parameters;
    char buf[20];
    vTaskSuspend(NULL);
    while(1){
        xSemaphoreTake(bn->msg_mutex, portMAX_DELAY);
        sprintf(buf, MSG_FMT, MSG_ID(bn->node_id), POST, bn->button_state);
        ESP_LOGI(TAG, LOG_FMT("sending POST BUTTON %d"), bn->button_state);
        if(send(bn->msg_fd, buf, sizeof(buf), 0) < 0){
            ESP_LOGE(TAG, LOG_FMT("error in send (%d)"), errno);
            close(bn->msg_fd);
            break;
        }
        char recv_buf[20];
        int ret = recv(bn->msg_fd, recv_buf, sizeof(recv_buf), 0);
        if(ret < 0){
            ESP_LOGE(TAG, LOG_FMT("error in recv (%d)"), errno);
            close(bn->msg_fd);
            break;
        }
        node_msg nm;
        parse_msg(TAG, &nm, recv_buf, ret);
        msg_handler(bn, &nm);
        xSemaphoreGive(bn->msg_mutex);
        vTaskSuspend(NULL);
    }
    vTaskDelete(NULL);
}

void button_handler(void * parameters){
    branch_node * bn = (branch_node *) parameters;
    while(1){
        vTaskSuspend(NULL);
        xSemaphoreTake(bn->msg_mutex, portMAX_DELAY);
        xSemaphoreGive(bn->msg_mutex);
        vTaskResume(bn->xpost);
        ESP_LOGI(TAG, LOG_FMT("BUTTON HANDLER %d"), bn->button_state);
        while(true){
            if(gpio_get_level(39) == 0){
                bn->button_state = false;
                xSemaphoreTake(bn->msg_mutex, portMAX_DELAY);
                xSemaphoreGive(bn->msg_mutex);
                vTaskResume(bn->xpost);
                vTaskDelay(200/portTICK_PERIOD_MS);
                break;
            }
            vTaskDelay(10/portTICK_PERIOD_MS);
        }
    }
    vTaskDelete(NULL);
}

esp_err_t start_node(branch_node * bn, const char * branch_ip){
    bn->node_id = branch_ip[10] - '1';
    bn->msg_fd = -1;
    bn->ctrl_fd = -1;
    bn->end_node = false;
    bn->xshutdown = NULL;
    bn->xbutton_handler = NULL;
    bn->xpoll_ctrl = NULL;
    bn->xpost = NULL;

    struct sockaddr_in serv_addr = {
        .sin_family   = PF_INET,
        .sin_addr     = {
            .s_addr = inet_addr(ROOT_IP)
        },
        .sin_port     = htons(NETWORK_PORT)
    };
    bn->msg_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(bn->msg_fd < 0){
        ESP_LOGE(TAG, LOG_FMT("error in socket (%d)"), errno);
        close(bn->msg_fd);
        return ESP_FAIL;
    }
    if (connect(bn->msg_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0) {
        ESP_LOGE(TAG, LOG_FMT("error in connect (%d)"), errno);
        close(bn->msg_fd);
        return ESP_FAIL;
    }
    char buf[20];
    sprintf(buf, MSG_FMT, MSG_ID(bn->node_id), NODE_INFO, 0);
    if(send(bn->msg_fd, buf, sizeof(buf), 0) < 0){
        ESP_LOGE(TAG, LOG_FMT("error in send (%d)"), errno);
        close(bn->msg_fd);
        return ESP_FAIL;
    }
    char recv_buf[20];
    int ret = recv(bn->msg_fd, recv_buf, sizeof(recv_buf), 0);
    if(ret < 0){
        ESP_LOGE(TAG, LOG_FMT("error in recv (%d)"), errno);
        close(bn->msg_fd);
        return ESP_FAIL;
    }
    node_msg nm;
    parse_msg(TAG, &nm, recv_buf, ret);
    msg_handler(bn, &nm);
    bn->ctrl_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(bn->ctrl_fd < 0){
        ESP_LOGE(TAG, LOG_FMT("error in socket (%d)"), errno);
        close(bn->ctrl_fd);
        return ESP_FAIL;
    }
    if (connect(bn->ctrl_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0) {
        ESP_LOGE(TAG, LOG_FMT("error in connect (%d)"), errno);
        close(bn->ctrl_fd);
        return ESP_FAIL;
    }
    sprintf(buf, MSG_FMT, CTRL_ID(bn->node_id), NODE_INFO, 0);
    if(send(bn->ctrl_fd, buf, sizeof(buf), 0) < 0){
        ESP_LOGE(TAG, LOG_FMT("error in send (%d)"), errno);
        close(bn->ctrl_fd);
        return ESP_FAIL;
    }
    memset(recv_buf, 0, 19);
    ret = recv(bn->ctrl_fd, recv_buf, sizeof(recv_buf), 0);
    if(ret < 0){
        ESP_LOGE(TAG, LOG_FMT("error in recv (%d)"), errno);
        close(bn->ctrl_fd);
        return ESP_FAIL;
    }
    nm.node_id = -1;
    nm.msg_type = -1;
    nm.value = -1;
    parse_msg(TAG, &nm, recv_buf, ret);
    msg_handler(bn, &nm);
    gpio_reset_pin(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_ANYEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, (void*)bn);
    bn->msg_mutex = xSemaphoreCreateMutex();
    bn->ctrl_mutex = xSemaphoreCreateMutex();
    xTaskCreate(shutdown_handler, "shutdown", 10000, (void*)bn, 1, &bn->xshutdown);
    xTaskCreate(button_handler, "button_handler", 4000, (void*)bn, 2, &bn->xbutton_handler);
    xTaskCreate(poll_ctrl, "poll_ctrl", 10000, (void*)bn, 1, &bn->xpoll_ctrl);
    xTaskCreate(post, "post", 10000, (void*)bn, 1, &bn->xpost);
    return ESP_OK;
}