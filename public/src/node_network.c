#include "../inc/node_network.h"

int hex_to_int(char c){
    if(c < '0' || (c > '9' && c < 'a') || c > 'f'){
        return -1;
    }else if(c <= '9'){
        return (c - '0');
    }else if(c >= 'a'){
        return (c - 'a' + 10);
    }
    return -1;
}

void parse_msg(const char * TAG, struct node_msg * nm, char * buf, int len){
    int field_indx = 0;
    bool msg_done = false;
    nm->value = -1;
    char c;
    for(int i = 0; i < len; ++i){
        c = buf[i];
        switch(c){
            case 'X':
                break;
            case ' ':
                field_indx += 1;
                break;
            case ';':
                msg_done = true;
                break;
            default:
                switch(field_indx){
                    case NODE_ID:
                        ESP_LOGD(TAG, "ID=%c", c);
                        nm->node_id = hex_to_int(c);
                        if(nm->node_id < 0){
                            ESP_LOGI(TAG, "invalid node id %c", c);
                            return;
                        }
                        break;
                    case MSG_TYPE:
                        ESP_LOGD(TAG, "MSG_TYPE=%c", c);
                        nm->msg_type = hex_to_int(c);
                        if(nm->msg_type < 0){
                            ESP_LOGI(TAG, "invalid msg id %c", c);
                        }
                        break;
                    case MSG_VALUE:
                        ESP_LOGD(TAG, "MSG_VALUE=%c", c);
                        if(nm->value == -1){
                            nm->value = hex_to_int(c);
                        }else{
                            nm->value = nm->value << 4;
                            nm->value += hex_to_int(c);
                        }
                        if(nm->value < 0){
                            ESP_LOGI(TAG, "invalid msg value %c", c);
                        }
                        break;
                }
        }
        if(msg_done){
            break;
        }
    }
}

char * mt_string(enum msg_type mt){
    switch(mt){
        case RECV_COMPLETE:
            return "RECV_COMPLETE";
        case NODE_INFO:
            return "NODE_INFO";
        case KEEP_ALIVE:
            return "KEEP_ALIVE";
        case POST:
            return "POST";
        case SHUTDOWN:
            return "SHUTDOWN";
        default:
            return "INVALID";
    }
}

char * rs_string(enum recv_status rs){
    switch(rs){
        case SUCCESS:
            return "SUCCESS";
        case FAILURE:
            return "FAILURE";
        default:
            return "INVALID";
    }
}

char * sn_string(enum signal_name sn){
    switch(sn){
        case BUTTON:
            return "BUTTON";
        case TEMP:
            return "TEMP";
        default:
            return "INVALID";
    }
}