#include "wifi.h"


static const char *TAG = "wifi_funcs";
static unsigned int s_retry_num = 0;

wifi_config_t wifi_config_sta = {
    .sta = {
        .ssid = WIFI_STA_SSID,
        .password = WIFI_STA_PASS,
        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        .pmf_cfg = {
            .capable = true,
            .required = false
        },
    },
};

wifi_config_t wifi_config_ap = {
    .ap = {
        .ssid = WIFI_AP_SSID,
        .ssid_len = strlen(WIFI_AP_SSID),
        .channel = WIFI_CHANNEL,
        .password = WIFI_AP_PASS,
        .max_connection = MAX_STA_CONN,
        .authmode = WIFI_AUTH_WPA_WPA2_PSK,
    },
};

void wifi_sta_event_handler(void* s_wifi_event_group, esp_event_base_t event_base, int32_t event_id, void* event_data){
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "EVENT BASE: %s, EVENT ID: %ld, EVENT BITS %lu", event_base, event_id, xEventGroupGetBits(s_wifi_event_group));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGI(TAG, "EVENT BASE: %s, EVENT ID: %ld, EVENT BITS %lu", event_base, event_id, xEventGroupGetBits(s_wifi_event_group));
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "EVENT BASE: %s, EVENT ID: %ld, EVENT BITS %lu", event_base, event_id, xEventGroupGetBits(s_wifi_event_group));
    }
}

void wifi_ap_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        /* if our wifi event is a station connected we cast event data to the wifi_event_ap_staconnected_t struct 
        which has three members: MAC address of the station connected, the aid of the station connected, and a bool to determine of the 
        station connected is a mesh child */
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station="MACSTR" join, AID=%d, isMeshChild=%d",MAC2STR(event->mac), event->aid, event->is_mesh_child);
        
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        //wifi_even_apstadisconnected_t struct has same members as wifi_event_ap_staconnected_t
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, isMeshChild=%d",MAC2STR(event->mac), event->aid, event->is_mesh_child);
    }
}

void wifi_init_ap(EventGroupHandle_t s_wifi_event_group, unsigned short int *status){
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_ap_event_handler, NULL,NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap) );
    ESP_ERROR_CHECK(esp_wifi_start());
    vEventGroupDelete(s_wifi_event_group);
}

void wifi_init_sta(EventGroupHandle_t s_wifi_event_group, unsigned short int *status){
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_t* wifiAP = esp_netif_create_default_wifi_ap();
    esp_netif_ip_info_t ipInfo;
    IP4_ADDR(&ipInfo.ip, 192,168,2,1);
	IP4_ADDR(&ipInfo.gw, 192,168,2,1);
	IP4_ADDR(&ipInfo.netmask, 255,255,255,0);
	esp_netif_dhcps_stop(wifiAP);
	esp_netif_set_ip_info(wifiAP, &ipInfo);
	esp_netif_dhcps_start(wifiAP);
    

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_sta_event_handler,s_wifi_event_group,&instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,&wifi_sta_event_handler,s_wifi_event_group,&instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_ap_event_handler, NULL,NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap) );
    ESP_ERROR_CHECK(esp_wifi_start());
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",WIFI_STA_SSID, WIFI_STA_PASS);
        *status = 0;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",WIFI_STA_SSID, WIFI_STA_PASS);
        *status = 1;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        *status = 2;
    }
    
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

