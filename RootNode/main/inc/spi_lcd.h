#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "nvs_flash.h"

#include "esp_lcd_ili9341.h"

#ifndef SPI_LCD
#define SPI_LCD

// Using SPI2 in the example
#define LCD_HOST  SPI2_HOST

#define LCD_PIXEL_CLOCK_HZ     (20 * 1000 * 1000)
#define LCD_BK_LIGHT_ON_LEVEL  0
#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL
#define PIN_NUM_SCLK           19
#define PIN_NUM_MOSI           23
#define PIN_NUM_MISO           25
#define PIN_NUM_LCD_DC         21
#define PIN_NUM_LCD_RST        18
#define PIN_NUM_LCD_CS         22
#define PIN_NUM_BK_LIGHT       5
#define PIN_NUM_TOUCH_CS       15

// The pixel number in horizontal and vertical
#define LCD_H_RES              320
#define LCD_V_RES              240
// Bit number used to represent command and parameter
#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8

#define LVGL_TICK_PERIOD_MS    2

#define LED_OFF_COLOR          0x128C24
#define LED_ON_COLOR           0x1FED3E
#define LED_ON_LGHT_COLOR      0x15A12A

#define BRANCH_NO              3

typedef struct branch_widget {
    int branch_no;
    lv_style_t led_style;
    lv_obj_t * led;
    lv_obj_t * base;
    lv_style_t base_style;
    lv_obj_t * label_base;
    lv_obj_t * label_led;
    lv_obj_t * label_offline;
}branch_widget;

lv_disp_t * spi_lcd_init(TaskHandle_t xlv_timer);
void lvgl_branch_base(lv_disp_t * disp, branch_widget * bw, int offset);
void lvgl_branch_online(branch_widget * bw);
void lvgl_branch_offline(branch_widget * bw);
void led_off(branch_widget * bw);
void led_on(branch_widget * bw);
void lv_timer(void * parameters);

#endif