#include "wifi.h"
#include "branch_node.h"

static const char *TAG = "branch_main";

EventGroupHandle_t wifi_event_group;

void app_main(void)
{
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);

    unsigned short int status = 0;
    char branch_ip[50];
    wifi_init_sta(wifi_event_group, &status, branch_ip);
    ESP_LOGI(TAG, LOG_FMT("ip=%s"), branch_ip);
    branch_node * bn = calloc(1, sizeof(branch_node));

    if(start_node(bn, branch_ip) != ESP_OK){
        ESP_LOGI(TAG, "failed to start node");
        return;
    }
}
