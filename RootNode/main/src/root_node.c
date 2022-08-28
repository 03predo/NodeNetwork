#include "root_node.h"

gptimer_alarm_config_t alarm_config = {
    .reload_count = 0,
    .alarm_count = SOCKET_TIMEOUT,
    .flags.auto_reload_on_alarm = true,
};
gptimer_config_t timer_config = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT,
    .direction = GPTIMER_COUNT_UP,
    .resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
};

static bool timeout_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void * parameters);
void handle_timeout(void * parameters);
void delete_branch(root_node * rn, branch_node * bn, branch_widget * bw);
void shutdown_root(root_node * rn);
static void msg_handler(root_node * rn, node_msg * nm, int fd);
void poll_db(void * parameters);
esp_err_t start_node(root_node * rn);

static bool timeout_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void * parameters){
    timeout_info * ti = (timeout_info *) parameters;
    ti->bn->timeout_occurred = true;
    vTaskResume(ti->rn->xtimeout);
    return true;
}

void handle_timeout(void * parameters){
    vTaskSuspend(NULL);
    root_node * rn = (root_node *) parameters;
    char recv_buf[20];
    char send_buf[20];
    sprintf(send_buf, MSG_FMT, ROOT_ID, KEEP_ALIVE, 0);
    int fd = -1, ret = -1;
    while(1){
        vTaskSuspend(rn->xpoll_db);
        for(int i = 0; i < MAX_BRANCH_NODES; ++i){
            if(!(rn->node_db[i].timeout_occurred)){
                continue;
            }
            ret = recv(rn->node_db[i].ctrl_fd, recv_buf, sizeof(recv_buf), 0);
            if(ret < 0 && errno != EWOULDBLOCK){
                ESP_LOGE(TAG, LOG_FMT("error in recv (%d), fd = %d"), errno, rn->node_db[i].ctrl_fd);
                delete_branch(rn, &rn->node_db[i], rn->bw[i]);
                break;
            }else if(ret == 0){
                ESP_LOGI(TAG, LOG_FMT("connection closed on %d"), rn->node_db[i].ctrl_fd);
                delete_branch(rn, &rn->node_db[i], rn->bw[i]);
                break;
            }
            fd = rn->node_db[i].ctrl_fd;
            ESP_LOGI(TAG, LOG_FMT("timeout occured on fd=%d"), fd);
            rn->node_db[i].timeout_occurred = false;
            
            ESP_LOGI(TAG, LOG_FMT("sending KEEP_ALIVE to fd=%d"), fd);
            ret = send(fd, send_buf, sizeof(send_buf), 0);
            if(ret < 0){
                ESP_LOGE(TAG, LOG_FMT("error in send (%d)"), errno);
                delete_branch(rn, &rn->node_db[i], rn->bw[i]);
                break;
            }
            struct pollfd fd_db[1];
            fd_db[0].fd = fd;
            fd_db[0].events = POLLIN;
            ret = poll(fd_db, 1, KEEP_ALIVE_TIMEOUT);
            if(ret < 0){
                ESP_LOGI(TAG, LOG_FMT("error in poll (%d)"), errno);
                delete_branch(rn, &rn->node_db[i], rn->bw[i]);
                break;
            }else if(ret == 0){
                ESP_LOGI(TAG, LOG_FMT("poll timeout occured"));
                delete_branch(rn, &rn->node_db[i], rn->bw[i]);
                break;
            }
            memset(recv_buf, 0, sizeof(recv_buf));
            ret = recv(fd, recv_buf, sizeof(recv_buf), 0);
            if(ret < 0){
                ESP_LOGE(TAG, LOG_FMT("error in recv (%d)"), errno);
                delete_branch(rn, &rn->node_db[i], rn->bw[i]);
                break;
            }
            node_msg nm;
            parse_msg(TAG, &nm, recv_buf, ret);
            msg_handler(rn, &nm, -1);
        }
        if(ret < 0){
            break;
        }
        // one last check to make sure all timeouts are handled
        ret = true;
        for(int i = 0; i < MAX_BRANCH_NODES; i++){
            if(rn->node_db[i].timeout_occurred){
                ret = false;
            }
        }
        vTaskResume(rn->xpoll_db);
        if(ret){
            ESP_LOGD(TAG, LOG_FMT("ending timeout handling"));
            vTaskSuspend(NULL);
        }
    }
    vTaskDelete(NULL);
}

void delete_branch(root_node * rn, branch_node * bn, branch_widget * bw){
    ESP_LOGI(TAG, LOG_FMT("deleting branch"));
    close(bn->ctrl_fd);
    for(int i = 0; i < rn->active_cnt; i++){
        if(rn->fd_db[i].fd == bn->msg_fd){
            for(int j = i; j < rn->active_cnt; j++){
                rn->fd_db[j].fd = rn->fd_db[j+1].fd;
            }
            i--;
            rn->active_cnt--;
        }
    }
    bn->ctrl_fd = -1;
    bn->msg_fd = -1;
    bn->timeout_occurred = false;
    rn->to_remove = -1;
    ESP_ERROR_CHECK(gptimer_stop(bn->timer));
    ESP_ERROR_CHECK(gptimer_disable(bn->timer));
    ESP_ERROR_CHECK(gptimer_del_timer(bn->timer));
    lvgl_branch_offline(bw);
}

void shutdown_root(root_node * rn){
    for(int i = 0; i < MAX_BRANCH_NODES; ++i){
        if(rn->node_db[i].ctrl_fd >= 0){
            char send_msg[20];
            sprintf(send_msg, MSG_FMT, ROOT_ID, SHUTDOWN, 0);
            int ret = send(rn->node_db[i].ctrl_fd, send_msg, sizeof(send_msg), 0);
            if(ret < 0){
                ESP_LOGE(TAG, LOG_FMT("error in send (%d)"), errno);
                break;
            }
            close(rn->node_db[i].ctrl_fd);
            rn->node_db[i].ctrl_fd = -1;
            ESP_ERROR_CHECK(gptimer_stop(rn->node_db[i].timer));
            ESP_ERROR_CHECK(gptimer_disable(rn->node_db[i].timer));
            ESP_ERROR_CHECK(gptimer_del_timer(rn->node_db[i].timer));
        }
        if(rn->node_db[i].msg_fd >= 0){
            close(rn->node_db[i].msg_fd);
            rn->node_db[i].msg_fd = -1;
        }
    }
    for(int i = 0; i < rn->active_cnt; i++){
        if(rn->fd_db[i].fd >= 0){
            close(rn->fd_db[i].fd);
        }
    }
    // send shutdown to branch socks, unregister timers
    vTaskDelete(rn->xtimeout);
    ESP_LOGI(TAG, "root shutdown complete");
}

static void msg_handler(root_node * rn, node_msg * nm, int fd){
    char send_msg[20];
    int ret = -1;
    switch(nm->msg_type){
        case NODE_INFO:
            ESP_LOGI(TAG, LOG_FMT("received %s %d from BranchID=%d"), mt_string((enum msg_type) nm->msg_type), nm->node_id, NODE_ID(nm->node_id));
            if((nm->node_id & 1) != 0){
                ESP_LOGI(TAG, LOG_FMT("fd %d is msg"), fd);
                rn->node_db[NODE_INDX(nm->node_id)].msg_fd = fd;
                lvgl_branch_online(rn->bw[NODE_INDX(nm->node_id)]);
                rn->to_remove = -1;
            }else{
                // if fd is a branch ctrl_fd we don't poll it in poll_fd
                // only in timeout handler
                ESP_LOGI(TAG, LOG_FMT("fd %d is ctrl"), fd);
                rn->node_db[NODE_INDX(nm->node_id)].ctrl_fd = fd;
                rn->to_remove = fd;
                rn->compress_db = true;
                timeout_info * ti = calloc(1, sizeof(struct timeout_info));
                ti->bn = & rn->node_db[NODE_INDX(nm->node_id)];
                ti->rn = rn;
                ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &rn->node_db[NODE_INDX(nm->node_id)].timer));
                ESP_ERROR_CHECK(gptimer_set_alarm_action(rn->node_db[NODE_INDX(nm->node_id)].timer, &alarm_config));
                gptimer_event_callbacks_t cbs = {
                    .on_alarm = timeout_cb,
                };
                ESP_ERROR_CHECK(gptimer_register_event_callbacks(rn->node_db[NODE_INDX(nm->node_id)].timer, &cbs, (void*)ti));
                ESP_ERROR_CHECK(gptimer_enable(rn->node_db[NODE_INDX(nm->node_id)].timer));
                ESP_ERROR_CHECK(gptimer_start(rn->node_db[NODE_INDX(nm->node_id)].timer));
            }
            ESP_LOGI(TAG, LOG_FMT("Branch%d: msg_fd=%d, ctrl_fd=%d"), NODE_ID(nm->node_id)
                                                                    , rn->node_db[NODE_INDX(nm->node_id)].msg_fd
                                                                    , rn->node_db[NODE_INDX(nm->node_id)].ctrl_fd);
            
            memset(send_msg, 0, sizeof(send_msg));
            sprintf(send_msg, MSG_FMT, ROOT_ID, RECV_COMPLETE, SUCCESS);
            ret = send(fd, send_msg, sizeof(send_msg), 0);
            if(ret < 0){
                ESP_LOGE(TAG, LOG_FMT("error in send (%d)"), errno);
                rn->close_conn = true;
                break;
            }
            break;
        case RECV_COMPLETE:
            ESP_LOGI(TAG, LOG_FMT("received %s %s from BranchID=%d"), mt_string((enum msg_type) nm->msg_type), rs_string((enum recv_status) nm->value), NODE_ID(nm->node_id));
            switch(nm->value){
                case SUCCESS:
                    break;
                default:
                    rn->close_conn = true;
                    break;
            }
            break;
        case POST:
            int signal_name = nm->value >> 4;
            int signal_value = nm->value & 15;
            ESP_LOGI(TAG, LOG_FMT("received %s %s %d from BranchID=%d"), mt_string((enum msg_type) nm->msg_type), sn_string(signal_name), signal_value,NODE_ID(nm->node_id));
            switch(signal_name){
                case BUTTON:
                    if((nm->value & 15) == 1){
                led_on(rn->bw[NODE_INDX(nm->node_id)]);
                }else{
                    led_off(rn->bw[NODE_INDX(nm->node_id)]);
                }
                break;
            }
            memset(send_msg, 0, sizeof(send_msg));
            gptimer_set_raw_count(rn->node_db[NODE_INDX(nm->node_id)].timer, 0);
            sprintf(send_msg, MSG_FMT, ROOT_ID, RECV_COMPLETE, SUCCESS);
            ret = send(fd, send_msg, sizeof(send_msg), 0);
            if(ret < 0){
                ESP_LOGE(TAG, LOG_FMT("error in send (%d)"), errno);
                rn->close_conn = true;
                break;
            }
            break;
    }
}

void poll_db(void * parameters){
    root_node * rn = (root_node *) parameters;
    int new_fd = -1;
    char buf[100];
    while(rn->end_server == false){
        ESP_LOGI(TAG, LOG_FMT("waiting on poll, active_cnt=%d, timeout=%d"), rn->active_cnt, POLL_TIMEOUT);
        int ret= poll(rn->fd_db, rn->active_cnt, POLL_TIMEOUT);
        if (ret < 0) {
            ESP_LOGE(TAG, LOG_FMT("error in poll (%d)"), errno);
            break;
        }else if (ret == 0){
            ESP_LOGI(TAG, LOG_FMT("poll timeout occured"));
            continue;
        }
        rn->current_size = rn->active_cnt;
        for (int i = 0; i < rn->current_size; i++){
            if(rn->fd_db[i].revents == 0){
                continue;
            }else if(rn->fd_db[i].revents != POLLIN){
                ESP_LOGE(TAG, LOG_FMT("%d revents=%d"), i, rn->fd_db[i].revents);
                rn->end_server = true;
                break;
            }
            if(rn->fd_db[i].fd == rn->listen_fd){
                do{
                    new_fd = accept(rn->listen_fd, NULL, NULL);
                    if(new_fd < 0){
                        if(errno != EWOULDBLOCK){
                            ESP_LOGE(TAG, LOG_FMT("error in accept (%d)"), errno);
                        }
                        break;
                    }
                    ESP_LOGI(TAG, LOG_FMT("new connection - %d"), new_fd);
                    fcntl(new_fd, F_SETFL, O_NONBLOCK);
                    rn->fd_db[rn->active_cnt].fd = new_fd;
                    rn->fd_db[rn->active_cnt].events = POLLIN;
                    rn->active_cnt++;

                }while(new_fd != -1);
            }else{
                ESP_LOGI(TAG, LOG_FMT("new message on %d"), rn->fd_db[i].fd);
                rn->close_conn = false;
                do {
                    ret = recv(rn->fd_db[i].fd, buf, sizeof(buf), 0);
                    if(ret < 0){
                        if(errno != EWOULDBLOCK){
                            ESP_LOGE(TAG, LOG_FMT("error in recv (%d)"), errno);
                            rn->close_conn = true;
                        }
                        if(rn->to_remove == rn->fd_db[i].fd){
                            ESP_LOGI(TAG, LOG_FMT("setting %d to -1"), rn->to_remove);
                            rn->fd_db[i].fd = -1;
                        }
                        break;
                    }
                    if(ret == 0){
                        ESP_LOGI(TAG, LOG_FMT("connection closed on %d"), rn->fd_db[i].fd);
                        rn->close_conn = true;
                        break;
                    }
                    ESP_LOGI(TAG, LOG_FMT("%d bytes received"), ret);
                    node_msg nm;
                    parse_msg(TAG, &nm, buf, ret);
                    msg_handler(rn, &nm, rn->fd_db[i].fd);
                }while(true);

                if(rn->close_conn){
                    close(rn->fd_db[i].fd);
                    rn->fd_db[i].fd = -1;
                    rn->compress_db = true;
                }
            }
        }
        if(rn->compress_db){
            ESP_LOGI(TAG, LOG_FMT("compressing fd_db"));
            rn->compress_db = false;
            for(int i = 0; i < rn->active_cnt; i++){
                if(rn->fd_db[i].fd == -1){
                    for(int j = i; j < rn->active_cnt; j++){
                        rn->fd_db[j].fd = rn->fd_db[j+1].fd;
                    }
                    i--;
                    rn->active_cnt--;
                }
            }
        }
    }
    shutdown_root(rn);
    vTaskDelete(NULL);
}

esp_err_t start_node(root_node * rn){
    int enable = 1;
    rn->active_cnt = 1;
    rn->current_size = 0;
    rn->end_server = false;
    rn->close_conn = false;
    rn->compress_db = false;
    rn->timeout_fd = -1;
    rn->to_remove = -1;
    rn->xpoll_db = NULL;
    rn->xtimeout = NULL;
    for(int i = 0; i < MAX_BRANCH_NODES; ++i){
        rn->node_db[i].msg_fd = -1;
        rn->node_db[i].ctrl_fd = -1;
        rn->node_db[i].timer = NULL;
    }
    rn->listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (rn->listen_fd < 0) {
        ESP_LOGE(TAG, LOG_FMT("error in socket (%d)"), errno);
        return ESP_FAIL;
    }
    struct sockaddr_in serv_addr = {
        .sin_family   = PF_INET,
        .sin_addr     = {
            .s_addr = htonl(INADDR_ANY)
        },
        .sin_port     = htons(NETWORK_PORT)
    };
    if (setsockopt(rn->listen_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(enable)) < 0) {
        ESP_LOGW(TAG, LOG_FMT("error enabling SO_REUSEADDR (%d)"), errno);
    }
    if(fcntl(rn->listen_fd, F_SETFL, O_NONBLOCK) < 0){
        ESP_LOGE(TAG, LOG_FMT("error in fcntl (%d)"), errno);
        close(rn->listen_fd);
        return ESP_FAIL;
    }
    if (bind(rn->listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        ESP_LOGE(TAG, LOG_FMT("error in bind (%d)"), errno);
        close(rn->listen_fd);
        return ESP_FAIL;
    }
    if (listen(rn->listen_fd, 1) < 0) {
        ESP_LOGE(TAG, LOG_FMT("error in listen (%d)"), errno);
        close(rn->listen_fd);
        return ESP_FAIL;
    }
    memset(rn->fd_db, 0 , sizeof(rn->fd_db));
    rn->fd_db[0].fd = rn->listen_fd;
    rn->fd_db[0].events = POLLIN;
    xTaskCreate(handle_timeout, "timeout", 10000, (void*)rn, 1, &rn->xtimeout);
    xTaskCreate(poll_db, "poll_db", 10000, (void*)rn, 1, &rn->xpoll_db);
    return ESP_OK;
}
