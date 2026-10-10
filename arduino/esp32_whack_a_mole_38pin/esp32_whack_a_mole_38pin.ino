/*
 * ESP32 Arcade Whack-A-Mole — Dedicated 38-Pin Board Sketch
 *
 * PHYSICAL 38-PIN BOARD WIRING DIAGRAM (Matches ESP32 38-Pin DevKit):
 *
 *                  LEFT SIDE                       RIGHT SIDE
 *         (3.3V) Pin  1 [ 3V3 ]              [ GND  ] Pin 38 (GND)
 *                Pin  2 [ EN  ]              [ D23  ] Pin 37 ---> Mole 2 LED
 *                Pin  3 [ D36 ]              [ D22  ] Pin 36
 *                Pin  4 [ D39 ]              [ TX0  ] Pin 35
 *                Pin  5 [ D34 ]              [ RX0  ] Pin 34
 *                Pin  6 [ D35 ]              [ D21  ] Pin 33 ---> Mole 1 Button
 *                Pin  7 [ D32 ]              [ GND  ] Pin 32
 *                Pin  8 [ D33 ]              [ D19  ] Pin 31 ---> Mole 1 LED
 *                Pin  9 [ D25 ]              [ D18  ] Pin 30 ---> Mole 2 Button
 *                Pin 10 [ D26 ]              [ D5   ] Pin 29 ---> OLED SCL
 * Mole 3 Button <--- Pin 11 [ D27 ]          [ D17  ] Pin 28 ---> Speaker / Buzzer
 *   Mole 3 LED <--- Pin 12 [ D14 ]          [ D16  ] Pin 27
 *   Mole 4 LED <--- Pin 13 [ D12 ]          [ D4   ] Pin 26 ---> OLED SDA
 *          (GND) Pin 14 [ GND ]              [ D0   ] Pin 25
 * Mole 4 Button <--- Pin 15 [ D13 ]          [ D2   ] Pin 24
 *      [FLASH]   Pin 16 [ D9  ]              [ D15  ] Pin 23
 *      [FLASH]   Pin 17 [ D10 ]              [ D8   ] Pin 22 [FLASH]
 *      [FLASH]   Pin 18 [ D11 ]              [ D7   ] Pin 21 [FLASH]
 *          (5V)  Pin 19 [ Vin ]              [ D6   ] Pin 20 [FLASH]
 */

#include <Wire.h>
#include "esp_random.h"

// ---- OLED Pin Configuration (SDA=Pin 26 / D4, SCL=Pin 29 / D5) ----
#define I2C_MASTER_SDA_IO    4   // Pin 26 (Right side)
#define I2C_MASTER_SCL_IO    5   // Pin 29 (Right side)
#define I2C_MASTER_FREQ_HZ   400000
#define OLED_I2C_ADDRESS     0x3C

// ---- Speaker / Buzzer Pin Configuration (Pin 28 / D17) ----
#define SPEAKER_PIN          17  // Pin 28 (Right side)
#define LEDC_RESOLUTION_BITS 10

#define GAME_DURATION_SEC    30

typedef enum {
  GAME_STATE_READY,
  GAME_STATE_PLAYING,
  GAME_STATE_GAMEOVER
} game_state_t;

typedef struct {
  uint8_t btn_pin;
  uint8_t led_pin;
  const char *label;
  uint32_t tone_freq;
} mole_cfg_t;

static const mole_cfg_t MOLES[4] = {
  { 21, 19, "M1", 523 }, // Mole 1: Btn Pin 33 (D21), LED Pin 31 (D19)
  { 18, 23, "M2", 659 }, // Mole 2: Btn Pin 30 (D18), LED Pin 37 (D23)
  { 27, 14, "M3", 784 }, // Mole 3: Btn Pin 11 (D27), LED Pin 12 (D14)
  { 13, 12, "M4", 880 }  // Mole 4: Btn Pin 15 (D13), LED Pin 13 (D12)
};

static uint8_t oled_buffer[1024];

// ---------------------------------------------------------------------------
// OLED (SSD1306) driver — Wire-based
// ---------------------------------------------------------------------------

static void oled_send_cmd(uint8_t cmd) {
  Wire.beginTransmission(OLED_I2C_ADDRESS);
  Wire.write(0x00); // control byte: command
  Wire.write(cmd);
  Wire.endTransmission();
}

static void oled_send_data(const uint8_t *data, size_t len) {
  Wire.beginTransmission(OLED_I2C_ADDRESS);
  Wire.write(0x40); // control byte: data
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

static void get_font_5x7(char c, uint8_t out[5]) {
  if (c >= 'a' && c <= 'z') {
    c -= 32; // Convert lowercase to uppercase
  }
  switch (c) {
    case ' ': out[0]=0x00; out[1]=0x00; out[2]=0x00; out[3]=0x00; out[4]=0x00; break;
    case '!': out[0]=0x00; out[1]=0x00; out[2]=0x5f; out[3]=0x00; out[4]=0x00; break;
    case '#': out[0]=0x14; out[1]=0x7f; out[2]=0x14; out[3]=0x7f; out[4]=0x14; break;
    case '*': out[0]=0x14; out[1]=0x08; out[2]=0x3e; out[3]=0x08; out[4]=0x14; break;
    case '-': out[0]=0x08; out[1]=0x08; out[2]=0x08; out[3]=0x08; out[4]=0x08; break;
    case ':': out[0]=0x00; out[1]=0x36; out[2]=0x36; out[3]=0x00; out[4]=0x00; break;
    case '0': out[0]=0x3e; out[1]=0x51; out[2]=0x49; out[3]=0x45; out[4]=0x3e; break;
    case '1': out[0]=0x00; out[1]=0x42; out[2]=0x7f; out[3]=0x40; out[4]=0x00; break;
    case '2': out[0]=0x42; out[1]=0x61; out[2]=0x51; out[3]=0x49; out[4]=0x46; break;
    case '3': out[0]=0x21; out[1]=0x41; out[2]=0x45; out[3]=0x4b; out[4]=0x31; break;
    case '4': out[0]=0x18; out[1]=0x14; out[2]=0x12; out[3]=0x7f; out[4]=0x10; break;
    case '5': out[0]=0x27; out[1]=0x45; out[2]=0x45; out[3]=0x45; out[4]=0x39; break;
    case '6': out[0]=0x3c; out[1]=0x4a; out[2]=0x49; out[3]=0x49; out[4]=0x30; break;
    case '7': out[0]=0x01; out[1]=0x71; out[2]=0x09; out[3]=0x05; out[4]=0x03; break;
    case '8': out[0]=0x36; out[1]=0x49; out[2]=0x49; out[3]=0x49; out[4]=0x36; break;
    case '9': out[0]=0x06; out[1]=0x49; out[2]=0x49; out[3]=0x29; out[4]=0x1e; break;
    case 'A': out[0]=0x7c; out[1]=0x12; out[2]=0x11; out[3]=0x12; out[4]=0x7c; break;
    case 'B': out[0]=0x7f; out[1]=0x49; out[2]=0x49; out[3]=0x49; out[4]=0x36; break;
    case 'C': out[0]=0x3e; out[1]=0x41; out[2]=0x41; out[3]=0x41; out[4]=0x22; break;
    case 'D': out[0]=0x7f; out[1]=0x41; out[2]=0x41; out[3]=0x22; out[4]=0x1c; break;
    case 'E': out[0]=0x7f; out[1]=0x49; out[2]=0x49; out[3]=0x49; out[4]=0x41; break;
    case 'F': out[0]=0x7f; out[1]=0x09; out[2]=0x09; out[3]=0x09; out[4]=0x01; break;
    case 'G': out[0]=0x3e; out[1]=0x41; out[2]=0x49; out[3]=0x49; out[4]=0x7a; break;
    case 'H': out[0]=0x7f; out[1]=0x08; out[2]=0x08; out[3]=0x08; out[4]=0x7f; break;
    case 'I': out[0]=0x00; out[1]=0x41; out[2]=0x7f; out[3]=0x41; out[4]=0x00; break;
    case 'J': out[0]=0x20; out[1]=0x40; out[2]=0x41; out[3]=0x3f; out[4]=0x01; break;
    case 'K': out[0]=0x7f; out[1]=0x08; out[2]=0x14; out[3]=0x22; out[4]=0x41; break;
    case 'L': out[0]=0x7f; out[1]=0x40; out[2]=0x40; out[3]=0x40; out[4]=0x40; break;
    case 'M': out[0]=0x7f; out[1]=0x02; out[2]=0x0c; out[3]=0x02; out[4]=0x7f; break;
    case 'N': out[0]=0x7f; out[1]=0x04; out[2]=0x08; out[3]=0x10; out[4]=0x7f; break;
    case 'O': out[0]=0x3e; out[1]=0x41; out[2]=0x41; out[3]=0x41; out[4]=0x3e; break;
    case 'P': out[0]=0x7f; out[1]=0x09; out[2]=0x09; out[3]=0x09; out[4]=0x06; break;
    case 'Q': out[0]=0x3e; out[1]=0x41; out[2]=0x51; out[3]=0x21; out[4]=0x5e; break;
    case 'R': out[0]=0x7f; out[1]=0x09; out[2]=0x19; out[3]=0x29; out[4]=0x46; break;
    case 'S': out[0]=0x46; out[1]=0x49; out[2]=0x49; out[3]=0x49; out[4]=0x31; break;
    case 'T': out[0]=0x01; out[1]=0x01; out[2]=0x7f; out[3]=0x01; out[4]=0x01; break;
    case 'U': out[0]=0x3f; out[1]=0x40; out[2]=0x40; out[3]=0x40; out[4]=0x3f; break;
    case 'V': out[0]=0x1f; out[1]=0x20; out[2]=0x40; out[3]=0x20; out[4]=0x1f; break;
    case 'W': out[0]=0x3f; out[1]=0x40; out[2]=0x38; out[3]=0x40; out[4]=0x3f; break;
    case 'X': out[0]=0x63; out[1]=0x14; out[2]=0x08; out[3]=0x14; out[4]=0x63; break;
    case 'Y': out[0]=0x07; out[1]=0x08; out[2]=0x70; out[3]=0x08; out[4]=0x07; break;
    case 'Z': out[0]=0x61; out[1]=0x51; out[2]=0x49; out[3]=0x45; out[4]=0x43; break;
    case '[': out[0]=0x00; out[1]=0x7f; out[2]=0x41; out[3]=0x41; out[4]=0x00; break;
    case ']': out[0]=0x00; out[1]=0x41; out[2]=0x41; out[3]=0x7f; out[4]=0x00; break;
    default: memset(out, 0, 5); break;
  }
}

static void oled_draw_char(int x, int page, char c) {
  if (c < 32 || c > 126) return;
  if (x > 122 || page > 7) return;
  uint8_t f[5];
  get_font_5x7(c, f);
  for (int i = 0; i < 5; i++) {
    oled_buffer[page * 128 + x + i] = f[i];
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
// Speaker / buzzer — Arduino LEDC tone API
// ---------------------------------------------------------------------------

static void speaker_init(void) {
  ledcAttach(SPEAKER_PIN, 1000, LEDC_RESOLUTION_BITS); // initial freq is irrelevant, overwritten per tone
}

static void play_tone(uint32_t freq_hz, uint32_t duration_ms) {
  if (freq_hz == 0) {
    ledcWrite(SPEAKER_PIN, 0); // silence
  } else {
    ledcWriteTone(SPEAKER_PIN, freq_hz); // sets frequency + 50% duty
    delay(duration_ms);
    ledcWrite(SPEAKER_PIN, 0); // silence
  }
}

static void sound_hit(uint32_t base_freq) {
  play_tone(base_freq, 50);
  play_tone(base_freq * 3 / 2, 80);
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

// ---------------------------------------------------------------------------
// GPIO — buttons (pull-up, active-low) + illuminated LEDs
// ---------------------------------------------------------------------------

static void gpio_init_all(void) {
  for (int i = 0; i < 4; i++) {
    pinMode(MOLES[i].btn_pin, INPUT_PULLUP);
    pinMode(MOLES[i].led_pin, OUTPUT);
    digitalWrite(MOLES[i].led_pin, LOW); // Default: LED OFF
  }
}

static void set_all_leds(int state) {
  for (int i = 0; i < 4; i++) {
    digitalWrite(MOLES[i].led_pin, state);
  }
}

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------

static game_state_t state = GAME_STATE_READY;
static int score = 0;
static int high_score = 0;
static int current_mole = -1;
static int total_hits = 0;
static int total_misses = 0;

static unsigned long game_start_time = 0;
static unsigned long mole_spawn_time = 0;
static uint32_t mole_active_duration_ms = 1200; // Starts at 1.2s per mole

static int last_btn_states[4] = {1, 1, 1, 1};

static char buf[32];

void setup() {
  Serial.begin(115200);
  Serial.println("Starting ESP32 Arcade Whack-A-Mole Game (38-Pin Module)...");

  Wire.setBufferSize(132);
  Wire.begin(I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, I2C_MASTER_FREQ_HZ);

  randomSeed(esp_random());

  oled_init();
  speaker_init();
  gpio_init_all();
}

void loop() {
  oled_clear();

  if (state == GAME_STATE_READY) {
    set_all_leds((millis() / 250) % 2);

    oled_draw_string(14, 0, "WHACK-A-MOLE!");
    oled_draw_string(4, 2, "ARCADE ESP32 GAME");
    oled_draw_string(4, 4, "PRESS ANY BUTTON");
    oled_draw_string(14, 5, "TO START GAME");

    snprintf(buf, sizeof(buf), "HIGH SCORE: %d", high_score);
    oled_draw_string(10, 7, buf);

    for (int i = 0; i < 4; i++) {
      int btn = digitalRead(MOLES[i].btn_pin);
      if (btn != last_btn_states[i]) {
        Serial.printf("[READY MODE] Mole %d (%s) GPIO %d changed to %d\n", i + 1, MOLES[i].label, MOLES[i].btn_pin, btn);
      }
      if (btn == 0 && last_btn_states[i] == 1) {
        state = GAME_STATE_PLAYING;
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
        last_btn_states[i] = btn;
        break;
      }
      last_btn_states[i] = btn;
    }
  }
  else if (state == GAME_STATE_PLAYING) {
    unsigned long now = millis();
    uint32_t elapsed_sec = (now - game_start_time) / 1000;
    int time_remaining = GAME_DURATION_SEC - elapsed_sec;

    if (time_remaining <= 0) {
      state = GAME_STATE_GAMEOVER;
      set_all_leds(0);
      if (score > high_score) {
        high_score = score;
      }
      Serial.printf("GAME OVER! Final Score: %d (Hits: %d, Misses: %d)\n", score, total_hits, total_misses);
      sound_gameover();
      return;
    }

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

    for (int i = 0; i < 4; i++) {
      int btn = digitalRead(MOLES[i].btn_pin);
      if (btn != last_btn_states[i]) {
        Serial.printf("[GAME MODE] Mole %d (%s) GPIO %d changed to %d\n", i + 1, MOLES[i].label, MOLES[i].btn_pin, btn);
      }
      if (btn == 0 && last_btn_states[i] == 1) {
        if (i == current_mole) {
          score += 100;
          total_hits++;
          Serial.printf("HIT Mole %s! (+100) Score: %d\n", MOLES[i].label, score);

          digitalWrite(MOLES[i].led_pin, LOW);
          sound_hit(MOLES[i].tone_freq);

          delay(100);
          current_mole = random(4);
          mole_spawn_time = millis();
          digitalWrite(MOLES[current_mole].led_pin, HIGH);

          if (mole_active_duration_ms > 450) {
            mole_active_duration_ms -= 25;
          }
        } else {
          score -= 30;
          if (score < 0) score = 0;
          total_misses++;
          Serial.printf("MISS! Pressed wrong button %s. (-30)\n", MOLES[i].label);
          sound_miss();
        }
      }
      last_btn_states[i] = btn;
    }

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
