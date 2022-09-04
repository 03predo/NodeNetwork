
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <lwip/ip4_addr.h>
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "esp_mac.h"

#ifndef WIFI_H
#define WIFI_H

#define WIFI_STA_SSID       "unit501"
#define WIFI_STA_PASS       "Waterloo2022"
#define MAXIMUM_RETRY       5
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define WIFI_AP_SSID        "RootNode"      // SSID is the name of the network 
#define WIFI_AP_PASS        "RootPass"      // password for wifi
#define WIFI_CHANNEL        1               // Wifi Channel 1: 2401–2423 MHz
#define MAX_STA_CONN        5               // Max numbers of station that can connected at one time

void wifi_init_sta(EventGroupHandle_t s_wifi_event_group, unsigned short int *status);
void wifi_sta_event_handler(void* s_wifi_event_group, esp_event_base_t event_base, int32_t event_id, void* event_data);
void wifi_ap_event_handler(void* s_wifi_event_group, esp_event_base_t event_base, int32_t event_id, void* event_data);
#endif
