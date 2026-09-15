/*
 * ESP32 Arcade Whack-A-Mole — Arduino IDE port with 2-Player Bluetooth Battle Mode
 *
 * Supports:
 *  1. SOLO MODE (Press B1): Classic single-player game.
 *  2. BATTLE MODE (Press B2 for HOST, Press B3 to JOIN):
 *     Two ESP32s connect over Bluetooth Serial, synchronize game start (3..2..1..GO!),
 *     display live opponent scores during 30s gameplay, and declare a Winner!
 */

#include <Wire.h>
#include "esp_random.h"
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif

BluetoothSerial SerialBT;

// ---- OLED Pin Configuration (SDA=D4, SCL=D5) ----
#define I2C_MASTER_SDA_IO    4
#define I2C_MASTER_SCL_IO    5
#define I2C_MASTER_FREQ_HZ   400000
#define OLED_I2C_ADDRESS     0x3C

// ---- Speaker / Buzzer Pin Configuration on TX2 (GPIO 17) ----
#define SPEAKER_PIN          17
#define LEDC_RESOLUTION_BITS 10   // 1024 duty levels

#define GAME_DURATION_SEC    30

typedef enum {
  GAME_STATE_READY,
  GAME_STATE_BT_CONNECTING,
  GAME_STATE_COUNTDOWN,
  GAME_STATE_PLAYING_SOLO,
  GAME_STATE_PLAYING_BATTLE,
  GAME_STATE_GAMEOVER_SOLO,
  GAME_STATE_GAMEOVER_BATTLE
} game_state_t;

typedef enum {
  BT_ROLE_NONE,
  BT_ROLE_HOST,
  BT_ROLE_CLIENT
} bt_role_t;

typedef struct {
  uint8_t btn_pin;
  uint8_t led_pin;
  const char *label;
  uint32_t tone_freq;
} mole_cfg_t;

static const mole_cfg_t MOLES[4] = {
  { 21, 19, "M1", 523 }, // Mole 1: Input D21, LED D19 (B1 = Solo / General Start)
  { 22, 23, "M2", 659 }, // Mole 2: Input D22, LED D23 (B2 = Host Battle)
  { 32, 33, "M3", 784 }, // Mole 3: Input D32, LED D33 (B3 = Join Battle)
  { 25, 26, "M4", 880 }  // Mole 4: Input D25, LED D26
};

static uint8_t oled_buffer[1024];

// ---------------------------------------------------------------------------
// OLED Driver (SSD1306)
// ---------------------------------------------------------------------------

static void oled_send_cmd(uint8_t cmd) {
  Wire.beginTransmission(OLED_I2C_ADDRESS);
  Wire.write(0x00);
  Wire.write(cmd);
  Wire.endTransmission();
}

static void oled_send_data(const uint8_t *data, size_t len) {
  Wire.beginTransmission(OLED_I2C_ADDRESS);
  Wire.write(0x40);
  Wire.write(data, len);
  Wire.endTransmission();
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

// ---------------------------------------------------------------------------
// Speaker & Audio Tones
// ---------------------------------------------------------------------------

static void speaker_init(void) {
  ledcAttach(SPEAKER_PIN, 1000, LEDC_RESOLUTION_BITS);
}

static void play_tone(uint32_t freq_hz, uint32_t duration_ms) {
  if (freq_hz == 0) {
    ledcWrite(SPEAKER_PIN, 0);
  } else {
    ledcWriteTone(SPEAKER_PIN, freq_hz);
    delay(duration_ms);
    ledcWrite(SPEAKER_PIN, 0);
  }
}

static void sound_hit(uint32_t base_freq) {
  play_tone(base_freq, 50);
  play_tone(base_freq * 3 / 2, 80);
}

static void sound_miss(void) {
  play_tone(200, 150);
}

static void sound_victory(void) {
  play_tone(523, 100);
  play_tone(659, 100);
  play_tone(784, 150);
  play_tone(1046, 300);
}

static void sound_defeat(void) {
  play_tone(400, 150);
  play_tone(350, 150);
  play_tone(300, 150);
  play_tone(250, 350);
}

static void sound_gameover(void) {
  play_tone(523, 100);
  play_tone(440, 100);
  play_tone(349, 100);
  play_tone(261, 250);
}

// ---------------------------------------------------------------------------
// GPIO Initialization
// ---------------------------------------------------------------------------

static void gpio_init_all(void) {
  for (int i = 0; i < 4; i++) {
    pinMode(MOLES[i].btn_pin, INPUT_PULLUP);
    pinMode(MOLES[i].led_pin, OUTPUT);
    digitalWrite(MOLES[i].led_pin, LOW);
  }
}

static void set_all_leds(int state) {
  for (int i = 0; i < 4; i++) {
    digitalWrite(MOLES[i].led_pin, state);
  }
}

// ---------------------------------------------------------------------------
// Game Globals & Bluetooth State
// ---------------------------------------------------------------------------

static game_state_t state = GAME_STATE_READY;
static bt_role_t bt_role = BT_ROLE_NONE;

static int score = 0;
static int opp_score = 0;
static int high_score = 0;
static int current_mole = -1;
static int total_hits = 0;
static int total_misses = 0;

static unsigned long game_start_time = 0;
static unsigned long mole_spawn_time = 0;
static uint32_t mole_active_duration_ms = 1200;

static int last_btn_states[4] = {1, 1, 1, 1};
static char buf[32];
static String bt_rx_buffer = "";

// ---------------------------------------------------------------------------
// Bluetooth Messaging Helper Functions
// ---------------------------------------------------------------------------

static void send_bt_msg(const String &msg) {
  if (SerialBT.hasClient() || bt_role == BT_ROLE_CLIENT) {
    SerialBT.println(msg);
  }
}

static void process_bt_incoming(void) {
  while (SerialBT.available()) {
    char c = SerialBT.read();
    if (c == '\n' || c == '\r') {
      if (bt_rx_buffer.length() > 0) {
        bt_rx_buffer.trim();
        
        if (bt_rx_buffer.startsWith("SCORE:")) {
          opp_score = bt_rx_buffer.substring(6).toInt();
        } 
        else if (bt_rx_buffer == "START_GAME") {
          if (state == GAME_STATE_BT_CONNECTING || state == GAME_STATE_COUNTDOWN) {
            state = GAME_STATE_COUNTDOWN;
          }
        }
        else if (bt_rx_buffer.startsWith("FINAL:")) {
          opp_score = bt_rx_buffer.substring(6).toInt();
        }

        bt_rx_buffer = "";
      }
    } else {
      bt_rx_buffer += c;
    }
  }
}

// ---------------------------------------------------------------------------
// Arduino setup() & loop()
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println("Starting ESP32 Arcade Whack-A-Mole (Bluetooth Battle Ready)...");

  Wire.setBufferSize(132);
  Wire.begin(I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, I2C_MASTER_FREQ_HZ);

  randomSeed(esp_random());

  oled_init();
  speaker_init();
  gpio_init_all();
}

void loop() {
  oled_clear();
  process_bt_incoming();

  if (state == GAME_STATE_READY) {
    set_all_leds((millis() / 250) % 2);

    oled_draw_string(14, 0, "WHACK-A-MOLE!");
    oled_draw_string(2, 2, "[B1] SOLO PLAY");
    oled_draw_string(2, 4, "[B2] BATTLE HOST");
    oled_draw_string(2, 5, "[B3] BATTLE JOIN");

    snprintf(buf, sizeof(buf), "HIGH SCORE: %d", high_score);
    oled_draw_string(10, 7, buf);

    // Read buttons for Mode Selection
    int b1 = digitalRead(MOLES[0].btn_pin);
    int b2 = digitalRead(MOLES[1].btn_pin);
    int b3 = digitalRead(MOLES[2].btn_pin);

    if (b1 == 0 && last_btn_states[0] == 1) {
      // SOLO MODE
      bt_role = BT_ROLE_NONE;
      state = GAME_STATE_PLAYING_SOLO;
      score = 0;
      total_hits = 0;
      total_misses = 0;
      game_start_time = millis();
      mole_active_duration_ms = 1200;
      set_all_leds(0);

      current_mole = random(4);
      mole_spawn_time = millis();
      digitalWrite(MOLES[current_mole].led_pin, HIGH);
      play_tone(1000, 100);
    }
    else if (b2 == 0 && last_btn_states[1] == 1) {
      // BATTLE MODE - HOST
      bt_role = BT_ROLE_HOST;
      SerialBT.begin("WhackMole-Host"); // Bluetooth device name
      state = GAME_STATE_BT_CONNECTING;
      set_all_leds(0);
      play_tone(800, 100);
    }
    else if (b3 == 0 && last_btn_states[2] == 1) {
      // BATTLE MODE - JOIN CLIENT
      bt_role = BT_ROLE_CLIENT;
      SerialBT.begin("WhackMole-Client", true); // Master mode to connect
      state = GAME_STATE_BT_CONNECTING;
      set_all_leds(0);
      play_tone(800, 100);

      // Attempt auto-connect to Host
      SerialBT.connect("WhackMole-Host");
    }

    for (int i = 0; i < 4; i++) {
      last_btn_states[i] = digitalRead(MOLES[i].btn_pin);
    }
  }
  else if (state == GAME_STATE_BT_CONNECTING) {
    oled_draw_string(14, 0, "BATTLE LOBBY");

    if (bt_role == BT_ROLE_HOST) {
      oled_draw_string(2, 2, "ROLE: HOST");
      oled_draw_string(2, 4, "WAITING FOR P2...");
      oled_draw_string(2, 6, "BT: WhackMole-Host");

      if (SerialBT.hasClient()) {
        send_bt_msg("START_GAME");
        state = GAME_STATE_COUNTDOWN;
      }
    } else {
      oled_draw_string(2, 2, "ROLE: JOINING...");
      oled_draw_string(2, 4, "CONNECTING BT...");
      oled_draw_string(2, 6, "TARGET: Host");

      if (SerialBT.connected()) {
        send_bt_msg("START_GAME");
        state = GAME_STATE_COUNTDOWN;
      }
    }
  }
  else if (state == GAME_STATE_COUNTDOWN) {
    oled_draw_string(14, 0, "BATTLE CONNECTED!");
    oled_draw_string(20, 3, "GET READY...");

    set_all_leds(1);
    play_tone(523, 100);
    oled_draw_string(60, 5, "3"); oled_update(); delay(600);
    play_tone(659, 100);
    oled_draw_string(60, 5, "2"); oled_update(); delay(600);
    play_tone(784, 100);
    oled_draw_string(60, 5, "1"); oled_update(); delay(600);
    play_tone(1046, 250);

    state = GAME_STATE_PLAYING_BATTLE;
    score = 0;
    opp_score = 0;
    total_hits = 0;
    total_misses = 0;
    game_start_time = millis();
    mole_active_duration_ms = 1200;
    set_all_leds(0);

    current_mole = random(4);
    mole_spawn_time = millis();
    digitalWrite(MOLES[current_mole].led_pin, HIGH);
  }
  else if (state == GAME_STATE_PLAYING_SOLO || state == GAME_STATE_PLAYING_BATTLE) {
    unsigned long now = millis();
    uint32_t elapsed_sec = (now - game_start_time) / 1000;
    int time_remaining = GAME_DURATION_SEC - elapsed_sec;

    if (time_remaining <= 0) {
      set_all_leds(0);
      if (state == GAME_STATE_PLAYING_BATTLE) {
        send_bt_msg("FINAL:" + String(score));
        state = GAME_STATE_GAMEOVER_BATTLE;
        delay(300);
        process_bt_incoming();

        if (score > opp_score) {
          sound_victory();
        } else if (score < opp_score) {
          sound_defeat();
        } else {
          sound_gameover();
        }
      } else {
        state = GAME_STATE_GAMEOVER_SOLO;
        if (score > high_score) high_score = score;
        sound_gameover();
      }
      return;
    }

    // Check mole escape
    uint32_t mole_elapsed_ms = now - mole_spawn_time;
    if (current_mole >= 0 && mole_elapsed_ms >= mole_active_duration_ms) {
      digitalWrite(MOLES[current_mole].led_pin, LOW);
      total_misses++;
      play_tone(300, 60);

      delay(150);
      current_mole = random(4);
      mole_spawn_time = millis();
      digitalWrite(MOLES[current_mole].led_pin, HIGH);

      if (mole_active_duration_ms > 450) {
        mole_active_duration_ms -= 15;
      }
    }

    // Check button hits/misses
    for (int i = 0; i < 4; i++) {
      int btn = digitalRead(MOLES[i].btn_pin);
      if (btn == 0 && last_btn_states[i] == 1) {
        if (i == current_mole) {
          // HIT!
          score += 100;
          total_hits++;
          digitalWrite(MOLES[i].led_pin, LOW);
          sound_hit(MOLES[i].tone_freq);

          if (state == GAME_STATE_PLAYING_BATTLE) {
            send_bt_msg("SCORE:" + String(score));
          }

          delay(100);
          current_mole = random(4);
          mole_spawn_time = millis();
          digitalWrite(MOLES[current_mole].led_pin, HIGH);

          if (mole_active_duration_ms > 450) {
            mole_active_duration_ms -= 25;
          }
        } else {
          // MISS!
          score -= 30;
          if (score < 0) score = 0;
          total_misses++;
          sound_miss();

          if (state == GAME_STATE_PLAYING_BATTLE) {
            send_bt_msg("SCORE:" + String(score));
          }
        }
      }
      last_btn_states[i] = btn;
    }

    // Render OLED UI
    if (state == GAME_STATE_PLAYING_BATTLE) {
      snprintf(buf, sizeof(buf), "TIME:%2ds   [BATTLE]", time_remaining);
      oled_draw_string(2, 0, buf);

      snprintf(buf, sizeof(buf), "YOU: %-4d   P2: %-4d", score, opp_score);
      oled_draw_string(2, 3, buf);

      snprintf(buf, sizeof(buf), "HITS:%-2d     MISS:%-2d", total_hits, total_misses);
      oled_draw_string(2, 6, buf);
    } else {
      oled_draw_string(10, 0, "WHACK-A-MOLE!");

      snprintf(buf, sizeof(buf), "TIME REMAINING: %2ds", time_remaining);
      oled_draw_string(2, 2, buf);

      snprintf(buf, sizeof(buf), "SCORE: %d", score);
      oled_draw_string(2, 4, buf);

      snprintf(buf, sizeof(buf), "HITS:%d  MISS:%d", total_hits, total_misses);
      oled_draw_string(2, 6, buf);
    }
  }
  else if (state == GAME_STATE_GAMEOVER_BATTLE) {
    if (score > opp_score) {
      oled_draw_string(14, 0, "* YOU WIN! *");
    } else if (score < opp_score) {
      oled_draw_string(14, 0, "* YOU LOSE *");
    } else {
      oled_draw_string(14, 0, "* DRAW GAME *");
    }

    snprintf(buf, sizeof(buf), "YOUR SCORE: %d", score);
    oled_draw_string(2, 3, buf);

    snprintf(buf, sizeof(buf), "OPP  SCORE: %d", opp_score);
    oled_draw_string(2, 5, buf);

    oled_draw_string(2, 7, "PRESS ANY BTN RESTART");

    for (int i = 0; i < 4; i++) {
      int btn = digitalRead(MOLES[i].btn_pin);
      if (btn == 0 && last_btn_states[i] == 1) {
        state = GAME_STATE_READY;
        if (SerialBT.hasClient()) SerialBT.disconnect();
        play_tone(600, 100);
        delay(300);
        break;
      }
      last_btn_states[i] = btn;
    }
  }
  else if (state == GAME_STATE_GAMEOVER_SOLO) {
    oled_draw_string(20, 0, "GAME OVER!");

    snprintf(buf, sizeof(buf), "FINAL SCORE: %d", score);
    oled_draw_string(2, 2, buf);

    snprintf(buf, sizeof(buf), "HIGH SCORE:  %d", high_score);
    oled_draw_string(2, 4, buf);

    oled_draw_string(4, 6, "PRESS ANY BUTTON");
    oled_draw_string(14, 7, "TO RESTART");

    for (int i = 0; i < 4; i++) {
      int btn = digitalRead(MOLES[i].btn_pin);
      if (btn == 0 && last_btn_states[i] == 1) {
        state = GAME_STATE_READY;
        play_tone(600, 100);
        delay(300);
        break;
      }
      last_btn_states[i] = btn;
    }
  }

  oled_update();
  delay(30);
}
