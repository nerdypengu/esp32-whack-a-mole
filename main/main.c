#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

static const char *TAG = "WHACK_A_MOLE";

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

#define GAME_DURATION_SEC           30

typedef enum {
    GAME_STATE_READY,
    GAME_STATE_PLAYING,
    GAME_STATE_GAMEOVER
} game_state_t;

typedef struct {
    gpio_num_t btn_pin;
    gpio_num_t led_pin;
    const char *label;
    uint32_t tone_freq;
} mole_cfg_t;

static const mole_cfg_t MOLES[4] = {
    { GPIO_NUM_21, GPIO_NUM_19, "M1", 523 }, // Mole 1: Input D21, LED D19
    { GPIO_NUM_22, GPIO_NUM_23, "M2", 659 }, // Mole 2: Input D22, LED D23
    { GPIO_NUM_32, GPIO_NUM_33, "M3", 784 }, // Mole 3: Input D32, LED D33
    { GPIO_NUM_25, GPIO_NUM_26, "M4", 880 }  // Mole 4: Input D25, LED D26
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

static void play_tone(uint32_t freq_hz, uint32_t duration_ms) {
    if (freq_hz == 0) {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    } else {
        ledc_set_freq(LEDC_MODE, LEDC_TIMER, freq_hz);
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 512);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    }
}

static void sound_hit(void) {
    play_tone(880, 50);
    play_tone(1175, 80);
}

static void sound_miss(void) {
    play_tone(200, 150);
}

static void sound_gameover(void) {
    play_tone(523, 100);
    play_tone(440, 100);
    play_tone(349, 100);
    play_tone(261, 250);
}

static void gpio_init_all(void) {
    for (int i = 0; i < 4; i++) {
        gpio_reset_pin(MOLES[i].btn_pin);
        gpio_reset_pin(MOLES[i].led_pin);

        // Button Input with Internal Pull-Up
        gpio_config_t btn_conf = {
            .pin_bit_mask = (1ULL << MOLES[i].btn_pin),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&btn_conf);

        // LED Output with Max Drive Capability (38mA)
        gpio_config_t led_conf = {
            .pin_bit_mask = (1ULL << MOLES[i].led_pin),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&led_conf);
        gpio_set_drive_capability(MOLES[i].led_pin, GPIO_DRIVE_CAP_3);

        // Default: LED OFF (LOW = 0)
        gpio_set_level(MOLES[i].led_pin, 0);
    }
}

static void set_all_leds(int state) {
    for (int i = 0; i < 4; i++) {
        gpio_set_level(MOLES[i].led_pin, state);
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
    ESP_LOGI(TAG, "Starting ESP32 Arcade Whack-A-Mole Game...");

    ESP_ERROR_CHECK(i2c_master_init());
    oled_init();
    speaker_init();
    gpio_init_all();

    game_state_t state = GAME_STATE_READY;
    int score = 0;
    int high_score = 0;
    int current_mole = -1;
    int total_hits = 0;
    int total_misses = 0;

    TickType_t game_start_time = 0;
    TickType_t mole_spawn_time = 0;
    uint32_t mole_active_duration_ms = 1200; // Starts at 1.2s per mole

    int last_btn_states[4] = {1, 1, 1, 1};

    char buf[32];

    while (1) {
        oled_clear();

        if (state == GAME_STATE_READY) {
            // Flash LEDs to invite player
            set_all_leds((xTaskGetTickCount() / 25) % 2);

            oled_draw_string(14, 0, "WHACK-A-MOLE!");
            oled_draw_string(4, 2, "ARCADE ESP32 GAME");
            oled_draw_string(4, 4, "PRESS ANY BUTTON");
            oled_draw_string(14, 5, "TO START GAME");

            snprintf(buf, sizeof(buf), "HIGH SCORE: %d", high_score);
            oled_draw_string(10, 7, buf);

            // Check any button press to start game
            for (int i = 0; i < 4; i++) {
                int btn = gpio_get_level(MOLES[i].btn_pin);
                if (btn == 0 && last_btn_states[i] == 1) {
                    // Start Game!
                    state = GAME_STATE_PLAYING;
                    score = 0;
                    total_hits = 0;
                    total_misses = 0;
                    game_start_time = xTaskGetTickCount();
                    mole_active_duration_ms = 1200;
                    set_all_leds(0);

                    // Spawn first mole
                    current_mole = esp_random() % 4;
                    mole_spawn_time = xTaskGetTickCount();
                    gpio_set_level(MOLES[current_mole].led_pin, 1);

                    play_tone(1000, 100);
                    break;
                }
                last_btn_states[i] = btn;
            }
        }
        else if (state == GAME_STATE_PLAYING) {
            TickType_t now = xTaskGetTickCount();
            uint32_t elapsed_sec = (now - game_start_time) * portTICK_PERIOD_MS / 1000;
            int time_remaining = GAME_DURATION_SEC - elapsed_sec;

            if (time_remaining <= 0) {
                // Game Over!
                state = GAME_STATE_GAMEOVER;
                set_all_leds(0);
                if (score > high_score) {
                    high_score = score;
                }
                ESP_LOGI(TAG, "GAME OVER! Final Score: %d (Hits: %d, Misses: %d)", score, total_hits, total_misses);
                sound_gameover();
                continue;
            }

            // Check if mole timed out (escaped)
            uint32_t mole_elapsed_ms = (now - mole_spawn_time) * portTICK_PERIOD_MS;
            if (current_mole >= 0 && mole_elapsed_ms >= mole_active_duration_ms) {
                // Mole escaped!
                gpio_set_level(MOLES[current_mole].led_pin, 0); // Turn off mole LED
                total_misses++;
                play_tone(300, 60);

                // Spawn next mole after short delay
                vTaskDelay(pdMS_TO_TICKS(150));
                current_mole = esp_random() % 4;
                mole_spawn_time = xTaskGetTickCount();
                gpio_set_level(MOLES[current_mole].led_pin, 1); // Turn ON new mole LED

                // Increase difficulty (speed up mole as score grows)
                if (mole_active_duration_ms > 450) {
                    mole_active_duration_ms -= 15;
                }
            }

            // Check button presses for Hits or Misses
            for (int i = 0; i < 4; i++) {
                int btn = gpio_get_level(MOLES[i].btn_pin);
                if (btn == 0 && last_btn_states[i] == 1) {
                    if (i == current_mole) {
                        // HIT! 🎉
                        score += 100;
                        total_hits++;
                        ESP_LOGI(TAG, "HIT Mole %s! (+100) Score: %d", MOLES[i].label, score);

                        gpio_set_level(MOLES[i].led_pin, 0); // Whacked! Turn OFF LED
                        sound_hit();

                        // Spawn next mole
                        vTaskDelay(pdMS_TO_TICKS(100));
                        current_mole = esp_random() % 4;
                        mole_spawn_time = xTaskGetTickCount();
                        gpio_set_level(MOLES[current_mole].led_pin, 1);

                        // Speed up mole timer
                        if (mole_active_duration_ms > 450) {
                            mole_active_duration_ms -= 25;
                        }
                    } else {
                        // MISS! ❌ (Pressed wrong button)
                        score -= 30;
                        if (score < 0) score = 0;
                        total_misses++;
                        ESP_LOGI(TAG, "MISS! Pressed wrong button %s. (-30)", MOLES[i].label);
                        sound_miss();
                    }
                }
                last_btn_states[i] = btn;
            }

            // OLED UI Rendering during Game
            oled_draw_string(10, 0, "WHACK-A-MOLE!");

            snprintf(buf, sizeof(buf), "TIME REMAINING: %2ds", time_remaining);
            oled_draw_string(2, 2, buf);

            snprintf(buf, sizeof(buf), "SCORE: %d", score);
            oled_draw_string(2, 4, buf);

            snprintf(buf, sizeof(buf), "HITS:%d  MISS:%d", total_hits, total_misses);
            oled_draw_string(2, 6, buf);
        }
        else if (state == GAME_STATE_GAMEOVER) {
            oled_draw_string(20, 0, "GAME OVER!");

            snprintf(buf, sizeof(buf), "FINAL SCORE: %d", score);
            oled_draw_string(2, 2, buf);

            snprintf(buf, sizeof(buf), "HIGH SCORE:  %d", high_score);
            oled_draw_string(2, 4, buf);

            oled_draw_string(4, 6, "PRESS ANY BUTTON");
            oled_draw_string(14, 7, "TO RESTART");

            // Check button to return to READY state
            for (int i = 0; i < 4; i++) {
                int btn = gpio_get_level(MOLES[i].btn_pin);
                if (btn == 0 && last_btn_states[i] == 1) {
                    state = GAME_STATE_READY;
                    play_tone(600, 100);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    break;
                }
                last_btn_states[i] = btn;
            }
        }

        oled_update();
        vTaskDelay(pdMS_TO_TICKS(30)); // 33Hz UI update rate
    }
}
