#include "display.h"
#include "display_orient.h"
#include "board_pins.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lvgl_port.h"
#include "esp_heap_caps.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "robot_settings.h"

#include <stdlib.h>

#if CONFIG_ROBOT_DISPLAY_BGR
#define PANEL_ORDER LCD_RGB_ELEMENT_ORDER_BGR
#else
#define PANEL_ORDER LCD_RGB_ELEMENT_ORDER_RGB
#endif
#if CONFIG_ROBOT_LCD_INVERT
#define PANEL_INVERT true
#else
#define PANEL_INVERT false
#endif

#define LCD_HOST   SPI2_HOST
#define DRAW_LINES 48   // LVGL partial buffer: 240*48*2 B = 23 KB, x2 (double buffer), internal DMA RAM

static const char *TAG = "display";

esp_err_t display_check_pins(void)
{
    struct { const char *name; int gpio; bool optional; } pins[] = {
        { "LCD_MOSI", CONFIG_ROBOT_PIN_LCD_MOSI, false },
        { "LCD_SCLK", CONFIG_ROBOT_PIN_LCD_SCLK, false },
        { "LCD_CS",   CONFIG_ROBOT_PIN_LCD_CS,   true  },
        { "LCD_DC",   CONFIG_ROBOT_PIN_LCD_DC,   false },
        { "LCD_RST",  CONFIG_ROBOT_PIN_LCD_RST,  true  },
        { "LCD_BL",   CONFIG_ROBOT_PIN_LCD_BL,   true  },
#if CONFIG_ROBOT_ENABLE_SENSOR
        { "DHT11",    CONFIG_ROBOT_PIN_DHT11,    false },
#endif
    };
    const size_t n = sizeof(pins) / sizeof(pins[0]);
    int gpios[sizeof(pins) / sizeof(pins[0])];
    esp_err_t ret = ESP_OK;

    for (size_t i = 0; i < n; i++) {
        gpios[i] = pins[i].gpio;
        pin_status_t s = board_pin_classify(pins[i].gpio);
        if (s == PIN_OK || (s == PIN_UNUSED && pins[i].optional)) continue;
        bool fatal = s >= PIN_ERR_INVALID || s == PIN_UNUSED;
        ESP_LOG_LEVEL(fatal ? ESP_LOG_ERROR : ESP_LOG_WARN, TAG, "%s = GPIO%d: %s",
                      pins[i].name, pins[i].gpio, board_pin_status_str(s));
        if (fatal) ret = ESP_ERR_INVALID_ARG;
    }
    int dup = board_pins_find_duplicate(gpios, n);
    if (dup >= 0) {
        ESP_LOGE(TAG, "GPIO%d assigned to more than one signal", dup);
        ret = ESP_ERR_INVALID_ARG;
    }
    return ret;
}

/* Phase-2 bring-up check: red/green/blue/white bars straight to the panel (no LVGL).
 * Wrong colours / shifted bars tell you which Kconfig knob to change (see README). */
static void draw_test_pattern(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t panel, int w, int h)
{
    // RGB565, byte-swapped because the panel expects big-endian pixels.
    static const uint16_t colors[] = { 0x00F8 /*red*/, 0xE007 /*green*/, 0x1F00 /*blue*/, 0xFFFF /*white*/ };
    const int band = h / 4;
    uint16_t *buf = heap_caps_malloc(w * DRAW_LINES * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf) { ESP_LOGW(TAG, "no DMA RAM for test pattern"); return; }

    for (int b = 0; b < 4; b++) {
        for (int i = 0; i < w * DRAW_LINES; i++) buf[i] = colors[b];
        int y_end = (b == 3) ? h : (b + 1) * band;
        for (int y = b * band; y < y_end; y += DRAW_LINES) {
            int y2 = y + DRAW_LINES < y_end ? y + DRAW_LINES : y_end;
            esp_lcd_panel_draw_bitmap(panel, 0, y, w, y2, buf);
        }
        // ponytail: a NOP command waits for queued colour DMA to finish before the buffer is reused.
        esp_lcd_panel_io_tx_param(io, 0x00, NULL, 0);
    }
    free(buf);
}

esp_err_t display_init(lv_display_t **out_disp)
{
    const int w = CONFIG_ROBOT_DISPLAY_WIDTH, h = CONFIG_ROBOT_DISPLAY_HEIGHT;
    robot_settings_t settings;
    settings_get(&settings);

    display_orient_t o = display_orient_get(settings.display_rotation, w, h, w,
                                            CONFIG_ROBOT_LCD_RAM_HEIGHT,
                                            settings.display_offset_x, settings.display_offset_y);
    ESP_RETURN_ON_FALSE(o.valid, ESP_ERR_INVALID_ARG, TAG, "rotation must be 0/90/180/270");

#if CONFIG_ROBOT_PIN_LCD_BL >= 0
    gpio_config_t bl = { .pin_bit_mask = 1ULL << CONFIG_ROBOT_PIN_LCD_BL, .mode = GPIO_MODE_OUTPUT };
    ESP_RETURN_ON_ERROR(gpio_config(&bl), TAG, "backlight gpio");
    gpio_set_level(CONFIG_ROBOT_PIN_LCD_BL, 0);   // off until the panel has valid content
#endif

    spi_bus_config_t bus = {
        .sclk_io_num = CONFIG_ROBOT_PIN_LCD_SCLK,
        .mosi_io_num = CONFIG_ROBOT_PIN_LCD_MOSI,
        .miso_io_num = -1, .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = w * DRAW_LINES * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "spi bus");

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = CONFIG_ROBOT_PIN_LCD_CS,
        .dc_gpio_num = CONFIG_ROBOT_PIN_LCD_DC,
        .spi_mode = CONFIG_ROBOT_LCD_SPI_MODE,
        .pclk_hz = CONFIG_ROBOT_DISPLAY_SPI_CLOCK_MHZ * 1000 * 1000,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io), TAG, "panel io");

    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = CONFIG_ROBOT_PIN_LCD_RST,
        .rgb_ele_order = settings.display_bgr ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(io, &panel_cfg, &panel), TAG, "st7789");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(panel, settings.display_invert), TAG, "invert");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(panel, o.swap_xy), TAG, "swap");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(panel, o.mirror_x, o.mirror_y), TAG, "mirror");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(panel, o.gap_x, o.gap_y), TAG, "gap");
    ESP_LOGI(TAG, "ST7789 %dx%d rot=%d swap=%d mirror=%d/%d gap=%d,%d @%d MHz",
             w, h, settings.display_rotation, o.swap_xy, o.mirror_x, o.mirror_y,
             o.gap_x, o.gap_y, CONFIG_ROBOT_DISPLAY_SPI_CLOCK_MHZ);

    if (CONFIG_ROBOT_DISPLAY_TEST_PATTERN_MS > 0) draw_test_pattern(io, panel, w, h);
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel, true), TAG, "display on");
#if CONFIG_ROBOT_PIN_LCD_BL >= 0
    gpio_set_level(CONFIG_ROBOT_PIN_LCD_BL, 1);
#endif
    if (CONFIG_ROBOT_DISPLAY_TEST_PATTERN_MS > 0) vTaskDelay(pdMS_TO_TICKS(CONFIG_ROBOT_DISPLAY_TEST_PATTERN_MS));

    // LVGL task on core 1; Wi-Fi/lwIP stay on core 0.
    lvgl_port_cfg_t lcfg = ESP_LVGL_PORT_INIT_CONFIG();
    lcfg.task_affinity = 1;
    lcfg.task_stack = 8192;
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lcfg), TAG, "lvgl port");

    lvgl_port_display_cfg_t dcfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = w * DRAW_LINES,
        .double_buffer = true,
        .hres = w,
        .vres = h,
        .rotation = { .swap_xy = o.swap_xy, .mirror_x = o.mirror_x, .mirror_y = o.mirror_y },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = { .buff_dma = 1, .swap_bytes = 1 },
    };
    lv_display_t *disp = lvgl_port_add_disp(&dcfg);
    ESP_RETURN_ON_FALSE(disp, ESP_FAIL, TAG, "lvgl_port_add_disp failed");

    lvgl_port_lock(0);
    lv_timer_set_period(lv_display_get_refr_timer(disp), 1000 / settings.animation_fps);
    lvgl_port_unlock();

    *out_disp = disp;
    return ESP_OK;
}
