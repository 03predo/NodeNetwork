#include "spi_lcd.h"
#include "node_network.h"

#define RADIUS(X) X*(LCD_H_RES/MAX_BRANCH_NODES)
static const char *TAG = "spi_lcd";

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

static void lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

void lv_timer(void * parameters){
    while(1){
        // raise the task priority of LVGL and/or reduce the handler period can improve the performance
        vTaskDelay(pdMS_TO_TICKS(100/portTICK_PERIOD_MS));
        // The task running lv_timer_handler should have lower priority than that running `lv_tick_inc`
        lv_timer_handler();
    }
    vTaskDelete(NULL);
    
}

lv_disp_t * spi_lcd_init(TaskHandle_t xlv_timer){
    static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv;      // contains callback functions

    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_LCD_DC,
        .cs_gpio_num = PIN_NUM_LCD_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = notify_lvgl_flush_ready,
        .user_ctx = &disp_drv,
    };
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install ILI9341 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_LCD_RST,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    // user can flush pre-defined pattern to the screen before we turn on the screen or backlight
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(PIN_NUM_BK_LIGHT, LCD_BK_LIGHT_ON_LEVEL);

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    lv_color_t *buf1 = heap_caps_malloc(LCD_H_RES * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = heap_caps_malloc(LCD_H_RES * 20 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2);
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, LCD_H_RES * 20);

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.drv_update_cb = NULL;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));
    xTaskCreate(lv_timer, "lv_timer", 4000, (void*)0, 1, &xlv_timer);
    return lv_disp_drv_register(&disp_drv);
}

void led_off(branch_widget * bw){
    lv_style_set_bg_color(&bw->led_style, lv_color_hex(LED_OFF_COLOR));
    lv_led_off(bw->led);
    lv_label_set_text(bw->label_led, "Off");
}

void led_on(branch_widget * bw){
    lv_style_set_bg_color(&bw->led_style, lv_color_hex(LED_ON_COLOR));
    lv_led_on(bw->led);
    lv_label_set_text(bw->label_led, "On");
}

void lvgl_branch_base(lv_disp_t * disp, branch_widget * bw, int offset){
    lv_obj_t *scr = lv_disp_get_scr_act(disp);
    // create base object
    bw->base = lv_obj_create(scr);
    lv_obj_set_size(bw->base, LCD_H_RES/MAX_BRANCH_NODES, LCD_V_RES);
    lv_obj_align(bw->base, LV_ALIGN_CENTER, offset, 0);
    lv_style_init(&bw->base_style);
    lv_style_set_bg_color(&bw->base_style, lv_color_hex(0xFFFFFF));
    lv_style_set_border_color(&bw->base_style, lv_color_hex(0xCCCBCB));
    lv_obj_add_style(bw->base, &bw->base_style, LV_PART_MAIN);
    // create base label
    bw->label_base = lv_label_create(bw->base);
    char buf[20];
    sprintf(buf, "Branch %d", bw->branch_no);
    lv_label_set_text(bw->label_base, buf);
    lv_obj_align(bw->label_base, LV_ALIGN_TOP_MID, 0, 0);;
    // create led
    bw->led = lv_led_create(bw->base);
    lv_obj_align(bw->led, LV_ALIGN_CENTER, 0, -20);
    lv_led_set_color(bw->led, lv_color_hex(LED_ON_LGHT_COLOR));
    lv_style_init(&bw->led_style);
    lv_obj_add_style(bw->led, &bw->led_style, LV_PART_MAIN);
    lv_obj_set_size(bw->led, RADIUS(.6), RADIUS(.6));
    lv_obj_add_flag(bw->led, LV_OBJ_FLAG_HIDDEN);
    // create led label
    bw->label_led = lv_label_create(bw->led);
    lv_obj_align(bw->label_led, LV_ALIGN_CENTER, 0, 0);
    led_off(bw);

    bw->label_temp = lv_label_create(bw->base);
    lv_label_set_text(bw->label_temp, "Temp: NA");
    lv_obj_align(bw->label_temp, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_add_flag(bw->label_temp, LV_OBJ_FLAG_HIDDEN);

    // create offline label
    bw->label_offline = lv_label_create(bw->base);
    lv_label_set_text(bw->label_offline, "Offline");
    lv_obj_align(bw->label_offline, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(bw->label_offline, LV_OBJ_FLAG_HIDDEN);
}

void lvgl_branch_online(branch_widget * bw){
    lv_obj_clear_flag(bw->led, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(bw->label_temp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(bw->label_offline, LV_OBJ_FLAG_HIDDEN);
}

void lvgl_branch_offline(branch_widget * bw){
    lv_obj_add_flag(bw->led, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(bw->label_temp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(bw->label_offline, LV_OBJ_FLAG_HIDDEN);
}
