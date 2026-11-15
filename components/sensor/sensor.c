#include <string.h>
#include "sensor.h"
#include "dht11_proto.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sensor";
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static sensor_data_t s_data;

/* Busy-wait while the line is at `level`. Returns elapsed us, or -1 on timeout. */
static int wait_while(gpio_num_t pin, int level, int timeout_us)
{
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(pin) == level)
        if (esp_timer_get_time() - start > timeout_us) return -1;
    return (int)(esp_timer_get_time() - start);
}

/* ponytail: bit-banged inside a ~5 ms critical section on core 1 (the LVGL core:
 * worst case one late frame). Move to RMT RX if that ever shows up as jitter. */
static dht11_status_t dht11_read(gpio_num_t pin, float *t, float *h)
{
    static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    uint16_t high_us[DHT11_BITS];
    uint8_t bytes[5];
    bool ok = true;

    gpio_set_level(pin, 0);                 // start signal: >= 18 ms low
    vTaskDelay(pdMS_TO_TICKS(20));

    portENTER_CRITICAL(&mux);
    gpio_set_level(pin, 1);                 // release (open-drain, pulled up)
    ok = wait_while(pin, 1, 100) >= 0       // sensor pulls low after 20-40 us
      && wait_while(pin, 0, 100) >= 0       // 80 us low
      && wait_while(pin, 1, 100) >= 0;      // 80 us high
    for (int i = 0; ok && i < DHT11_BITS; i++) {
        int hi = 0;
        ok = wait_while(pin, 0, 80) >= 0 && (hi = wait_while(pin, 1, 100)) >= 0;
        if (ok) high_us[i] = (uint16_t)hi;
    }
    portEXIT_CRITICAL(&mux);

    if (!ok) return DHT11_ERR_TIMEOUT;
    dht11_bits_to_bytes(high_us, bytes);
    return dht11_decode(bytes, t, h);
}

static void sensor_task(void *arg)
{
    const gpio_num_t pin = (gpio_num_t)CONFIG_ROBOT_PIN_DHT11;
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,   // weak; most DHT11 modules also have a 10k pull-up
    };
    gpio_config(&cfg);
    gpio_set_level(pin, 1);
    vTaskDelay(pdMS_TO_TICKS(1500));        // DHT11 needs ~1 s after power-up

    for (;;) {
        float t, h;
        dht11_status_t st = dht11_read(pin, &t, &h);
        portENTER_CRITICAL(&s_lock);
        if (st == DHT11_OK) {
            s_data.temperature = t;
            s_data.humidity = h;
            s_data.timestamp_ms = esp_timer_get_time() / 1000;
            s_data.fail_count = 0;
        } else {
            s_data.fail_count++;
        }
        uint32_t fails = s_data.fail_count;
        portEXIT_CRITICAL(&s_lock);

        if (st == DHT11_OK) ESP_LOGD(TAG, "%.1f C %.0f %%RH", t, h);
        else if (fails == 1 || fails % 10 == 0) ESP_LOGW(TAG, "read failed: %s (x%lu)", dht11_status_str(st), (unsigned long)fails);
        vTaskDelay(pdMS_TO_TICKS(CONFIG_ROBOT_SENSOR_INTERVAL_MS));
    }
}

esp_err_t sensor_start(void)
{
#if CONFIG_ROBOT_ENABLE_SENSOR
    BaseType_t ok = xTaskCreatePinnedToCore(sensor_task, "dht11", 3072, NULL, 5, NULL, 1);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
#else
    return ESP_OK;
#endif
}

void sensor_get(sensor_data_t *out)
{
    portENTER_CRITICAL(&s_lock);
    *out = s_data;
    portEXIT_CRITICAL(&s_lock);
    int64_t age_ms = esp_timer_get_time() / 1000 - out->timestamp_ms;
    out->valid = out->timestamp_ms > 0 && age_ms < CONFIG_ROBOT_SENSOR_STALE_S * 1000LL;
}
