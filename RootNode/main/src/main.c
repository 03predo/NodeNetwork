
#include "wifi.h"
#include "root_node.h"

EventGroupHandle_t wifi_event_group;

void app_main(void)
{
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);

    root_node * rn = calloc(1, sizeof(struct root_node));
    rn->xlv_timer = NULL;
    lv_disp_t * disp = calloc(1, sizeof(lv_disp_t));
    disp = spi_lcd_init(rn->xlv_timer);
    for(int i = 0; i < MAX_BRANCH_NODES; ++i){
        rn->bw[i] = calloc(1, sizeof(branch_widget));
        rn->bw[i]->branch_no = i + 1;
        rn->bw[i]->base = NULL;
        rn->bw[i]->label_base = NULL;
        rn->bw[i]->led = NULL;
        rn->bw[i]->label_led = NULL;
        rn->bw[i]->label_offline = NULL;
        ESP_LOGI(TAG, "i(%d), offset=%d", i, OFFSET(i));
        lvgl_branch_base(disp, rn->bw[i], OFFSET(i));
        lvgl_branch_offline(rn->bw[i]);
    }
    

    unsigned short int status = 0;
    wifi_init_sta(wifi_event_group, &status);

    if(start_node(rn) != ESP_OK){
        ESP_LOGI(TAG, "failed to start node");
        return;
    }
}
