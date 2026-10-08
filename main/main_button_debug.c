#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

static const char *TAG = "BTN_DEBUG";

// OLED Pin Configuration (SDA=D4, SCL=D5)
#define I2C_MASTER_SDA_IO           4
#define I2C_MASTER_SCL_IO           5
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          400000
#define OLED_I2C_ADDRESS            0x3C

// Speaker / Buzzer Pin Configuration on TX2 (GPIO 17)
#define SPEAKER_PIN                 GPIO_NUM_17
#define LEDC_TIMER                  LEDC_TIMER_0
#define LEDC_MODE                   LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL                LEDC_CHANNEL_0
#define LEDC_DUTY_RES               LEDC_TIMER_10_BIT

typedef struct {
    gpio_num_t btn_pin;
    gpio_num_t led_pin;
    const char *name;
    uint32_t freq;
} btn_debug_cfg_t;

static const btn_debug_cfg_t BUTTONS[4] = {
    { GPIO_NUM_21, GPIO_NUM_19, "B1 (D21->D19)", 523 },
    { GPIO_NUM_22, GPIO_NUM_23, "B2 (D22->D23)", 659 },
    { GPIO_NUM_32, GPIO_NUM_33, "B3 (D32->D33)", 784 },
    { GPIO_NUM_25, GPIO_NUM_26, "B4 (D25->D26)", 880 }
};

static uint8_t oled_buffer[1024];

static esp_err_t oled_send_cmd(uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};
    return i2c_master_write_to_device(I2C_MASTER_NUM, OLED_I2C_ADDRESS, buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t oled_send_data(const uint8_t *data, size_t len) {
    uint8_t buf[1025];
    buf[0] = 0x40;
    memcpy(buf + 1, data, len);
    return i2c_master_write_to_device(I2C_MASTER_NUM, OLED_I2C_ADDRESS, buf, len + 1, pdMS_TO_TICKS(500));
}

static void oled_init(void) {
    uint8_t init_cmds[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
        0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
    };
    for (size_t i = 0; i < sizeof(init_cmds); i++) {
        oled_send_cmd(init_cmds[i]);
    }
}

static void oled_update(void) {
    for (uint8_t page = 0; page < 8; page++) {
        oled_send_cmd(0xB0 + page);
        oled_send_cmd(0x00);
        oled_send_cmd(0x10);
        oled_send_data(&oled_buffer[page * 128], 128);
    }
}

static void oled_clear(void) {
    memset(oled_buffer, 0x00, sizeof(oled_buffer));
}

// 5x7 ASCII Font Bitmap
static const uint8_t font5x7[][5] = {
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00},
    ['!'] = {0x00, 0x00, 0x5f, 0x00, 0x00},
    ['#'] = {0x14, 0x7f, 0x14, 0x7f, 0x14},
    ['-'] = {0x08, 0x08, 0x08, 0x08, 0x08},
    [':'] = {0x00, 0x36, 0x36, 0x00, 0x00},
    ['0'] = {0x3e, 0x51, 0x49, 0x45, 0x3e},
    ['1'] = {0x00, 0x42, 0x7f, 0x40, 0x00},
    ['2'] = {0x42, 0x61, 0x51, 0x49, 0x46},
    ['3'] = {0x21, 0x41, 0x45, 0x4b, 0x31},
    ['4'] = {0x18, 0x14, 0x12, 0x7f, 0x10},
    ['5'] = {0x27, 0x45, 0x45, 0x45, 0x39},
    ['6'] = {0x3c, 0x4a, 0x49, 0x49, 0x30},
    ['7'] = {0x01, 0x71, 0x09, 0x05, 0x03},
    ['8'] = {0x36, 0x49, 0x49, 0x49, 0x36},
    ['9'] = {0x06, 0x49, 0x49, 0x29, 0x1e},
    ['A'] = {0x7c, 0x12, 0x11, 0x12, 0x7c},
    ['B'] = {0x7f, 0x49, 0x49, 0x49, 0x36},
    ['C'] = {0x3e, 0x41, 0x41, 0x41, 0x22},
    ['D'] = {0x7f, 0x41, 0x41, 0x22, 0x1c},
    ['E'] = {0x7f, 0x49, 0x49, 0x49, 0x41},
    ['F'] = {0x7f, 0x09, 0x09, 0x09, 0x01},
    ['G'] = {0x3e, 0x41, 0x49, 0x49, 0x7a},
    ['H'] = {0x7f, 0x08, 0x08, 0x08, 0x7f},
    ['I'] = {0x00, 0x41, 0x7f, 0x41, 0x00},
    ['J'] = {0x20, 0x40, 0x41, 0x3f, 0x01},
    ['K'] = {0x7f, 0x08, 0x14, 0x22, 0x41},
    ['L'] = {0x7f, 0x40, 0x40, 0x40, 0x40},
    ['M'] = {0x7f, 0x02, 0x0c, 0x02, 0x7f},
    ['N'] = {0x7f, 0x04, 0x08, 0x10, 0x7f},
    ['O'] = {0x3e, 0x41, 0x41, 0x41, 0x3e},
    ['P'] = {0x7f, 0x09, 0x09, 0x09, 0x06},
    ['Q'] = {0x3e, 0x41, 0x51, 0x21, 0x5e},
    ['R'] = {0x7f, 0x09, 0x19, 0x29, 0x46},
    ['S'] = {0x46, 0x49, 0x49, 0x49, 0x31},
    ['T'] = {0x01, 0x01, 0x7f, 0x01, 0x01},
    ['U'] = {0x3f, 0x40, 0x40, 0x40, 0x3f},
    ['V'] = {0x1f, 0x20, 0x40, 0x20, 0x1f},
    ['W'] = {0x3f, 0x40, 0x38, 0x40, 0x3f},
    ['X'] = {0x63, 0x14, 0x08, 0x14, 0x63},
    ['Y'] = {0x07, 0x08, 0x70, 0x08, 0x07},
    ['Z'] = {0x61, 0x51, 0x49, 0x45, 0x43},
    ['['] = {0x00, 0x7f, 0x41, 0x41, 0x00},
    [']'] = {0x00, 0x41, 0x41, 0x7f, 0x00}
};

static void oled_draw_char(int x, int page, char c) {
    if (c < 32 || c > 126) return;
    if (x > 122 || page > 7) return;
    for (int i = 0; i < 5; i++) {
        oled_buffer[page * 128 + x + i] = font5x7[(uint8_t)c][i];
    }
    oled_buffer[page * 128 + x + 5] = 0x00;
}

static void oled_draw_string(int x, int page, const char *str) {
    while (*str && x <= 122) {
        oled_draw_char(x, page, *str);
        x += 6;
        str++;
    }
}

static void speaker_init(void) {
    gpio_reset_pin(SPEAKER_PIN);
    ledc_timer_config_t timer_conf = {
        .speed_mode      = LEDC_MODE,
        .timer_num       = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz         = 1000,
        .clk_cfg         = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t channel_conf = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = SPEAKER_PIN,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&channel_conf);
}

static void play_tone_async(uint32_t freq_hz) {
    if (freq_hz == 0) {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    } else {
        ledc_set_freq(LEDC_MODE, LEDC_TIMER, freq_hz);
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 512);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    }
}

static void gpio_init_all(void) {
    for (int i = 0; i < 4; i++) {
        gpio_reset_pin(BUTTONS[i].btn_pin);
        gpio_reset_pin(BUTTONS[i].led_pin);

        // Input Button with Internal Pull-Up
        gpio_config_t btn_conf = {
            .pin_bit_mask = (1ULL << BUTTONS[i].btn_pin),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&btn_conf);

        // Output LED
        gpio_config_t led_conf = {
            .pin_bit_mask = (1ULL << BUTTONS[i].led_pin),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&led_conf);
        gpio_set_drive_capability(BUTTONS[i].led_pin, GPIO_DRIVE_CAP_3);

        gpio_set_level(BUTTONS[i].led_pin, 0);
    }
}

static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

void app_main(void) {
    ESP_LOGI(TAG, "=== ESP32 BUTTON & HARDWARE DIAGNOSTIC TOOL ===");

    ESP_ERROR_CHECK(i2c_master_init());
    oled_init();
    speaker_init();
    gpio_init_all();

    int last_btn_states[4] = {1, 1, 1, 1};
    char buf[32];

    while (1) {
        oled_clear();

        oled_draw_string(2, 0, "=== BUTTON DEBUG ===");

        int active_tone = 0;

        for (int i = 0; i < 4; i++) {
            int level = gpio_get_level(BUTTONS[i].btn_pin);
            int is_pressed = (level == 0); // Active-low button

            // Turn ON corresponding LED while pressed, OFF when released
            gpio_set_level(BUTTONS[i].led_pin, is_pressed ? 1 : 0);

            if (is_pressed) {
                active_tone = BUTTONS[i].freq;
            }

            // Log state changes on Serial Monitor
            if (level != last_btn_states[i]) {
                ESP_LOGI(TAG, "%s: %s (Raw Pin State: %d, LED: %s)",
                         BUTTONS[i].name,
                         is_pressed ? "PRESSED [ACTIVE-LOW]" : "RELEASED",
                         level,
                         is_pressed ? "ON" : "OFF");
                last_btn_states[i] = level;
            }

            // Display live status on OLED screen (Page 2..5)
            snprintf(buf, sizeof(buf), "B%d(D%02d): %s [LED:%s]",
                     i + 1,
                     BUTTONS[i].btn_pin,
                     is_pressed ? "PRESS" : " IDLE",
                     is_pressed ? "ON " : "OFF");
            oled_draw_string(2, 2 + i, buf);
        }

        // Play sound for currently pressed button
        play_tone_async(active_tone);

        oled_draw_string(2, 7, "PRESS BTN TO TEST!");
        oled_update();

        vTaskDelay(pdMS_TO_TICKS(20)); // Fast 50Hz responsiveness
    }
}
