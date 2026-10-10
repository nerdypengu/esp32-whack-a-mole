# 🔨 ESP32 Arcade Whack-A-Mole Game

An arcade-style Whack-A-Mole game built for **ESP32** supporting both **ESP-IDF 5.x** and **Arduino IDE**. Features 4 illuminated push-buttons, an **SSD1306 OLED display**, a **buzzer/speaker** for retro audio feedback, and **2-Player Bluetooth Battle Mode**!

---

## 🎮 Game Features

- **Solo & 2-Player Bluetooth Battle Modes**:
  - **Solo Mode**: Single player high-score chase.
  - **2-Player Battle Mode**: Connect 2 ESP32 units wirelessly over Bluetooth Serial to battle live in real-time!
- **4 Active Moles**: Random mole pop-ups using illuminated push-buttons.
- **Dynamic Difficulty**: Moles pop up faster as your score increases (from 1.2s down to 0.45s per mole).
- **Retro Audio**: Custom tones for Hits (+100 pts), Misses (-30 pts), Victory fanfare, Defeat sound, and Game Over melodies.
- **OLED Arcade UI**:
  - **Ready Screen**: Mode selection (`[B1] Solo`, `[B2] Host Battle`, `[B3] Join Battle`).
  - **Battle Lobby & Synchronized Countdown**: Simultaneous `3..2..1..GO!` countdown start.
  - **Live Gameplay**: Real-time timer countdown, current score vs opponent score (`YOU: 900  P2: 750`), total hits & misses.
  - **Game Over Screen**: Final winner announcement (`YOU WIN!`, `YOU LOSE!`, or `DRAW GAME`) with detailed score comparison.
- **Flicker-Free OLED Engine**: Dual-pin architecture ensures zero LED flickering during I2C OLED display updates.

---

## 🔌 Hardware Setup & Pinout

### Component Summary
- **1x or 2x ESP32 Dev Boards** (one per player)
- **4x Illuminated Push-Buttons** (Mole 1 - Mole 4)
- **1x SSD1306 I2C OLED Display (128x64)**
- **1x Passive Buzzer or Speaker**

### 📍 Wiring Diagrams

#### 1. Standard 30-Pin / 28-Pin Board Mapping
| Component | ESP32 Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **OLED Display** | `GPIO 4` (`D4`) | I2C SDA | 128x64 OLED (Address `0x3C`) |
| **OLED Display** | `GPIO 5` (`D5`) | I2C SCL | Internal pull-up enabled |
| **Speaker / Buzzer** | `GPIO 17` (`TX2`) | PWM Tone Output | LEDC channel (`GPIO 17` silent idle) |
| **Mole 1 (B1)** | Input: `GPIO 21` (`D21`)<br>LED: `GPIO 19` (`D19`) | Button & LED | **Solo Start** (Internal Pull-Up) |
| **Mole 2 (B2)** | Input: `GPIO 22` (`D22`)<br>LED: `GPIO 23` (`D23`) | Button & LED | **Host Battle** (Internal Pull-Up) |
| **Mole 3 (B3)** | Input: `GPIO 32` (`D32`)<br>LED: `GPIO 33` (`D33`) | Button & LED | **Join Battle** (Internal Pull-Up) |
| **Mole 4 (B4)** | Input: `GPIO 25` (`D25`)<br>LED: `GPIO 26` (`D26`) | Button & LED | **Mole 4** (Internal Pull-Up) |

#### 2. ESP-WROOM-32 38-Pin Module Mapping
| Component | 38-Pin Board Location | ESP32 Pin | Function | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **OLED Display** | Right Side - Pin 26 | `GPIO 4` (`D4`) | I2C SDA | 128x64 OLED |
| **OLED Display** | Right Side - Pin 29 | `GPIO 5` (`D5`) | I2C SCL | I2C Clock |
| **Speaker / Buzzer** | Right Side - Pin 28 | `GPIO 17` (`D17`) | PWM Tone | Audio output |
| **Mole 1 (B1)** | Input: Right Pin 33<br>LED: Right Pin 31 | Input: `GPIO 21`<br>LED: `GPIO 19` | Button & LED | **Solo Start** |
| **Mole 2 (B2)** | Input: Right Pin 30<br>LED: Right Pin 37 | Input: `GPIO 18`<br>LED: `GPIO 23` | Button & LED | **Host Battle** |
| **Mole 3 (B3)** | Input: Left Pin 11<br>LED: Left Pin 12 | Input: `GPIO 27`<br>LED: `GPIO 14` | Button & LED | **Join Battle** |
| **Mole 4 (B4)** | Input: Left Pin 15<br>LED: Left Pin 13 | Input: `GPIO 13`<br>LED: `GPIO 12` | Button & LED | **Mole 4** |

> ℹ️ **Note on 4-Pin Illuminated Buttons**: Connect diagonally across opposite corners! 
> - **Input Pin + GND** on one diagonal pair.
> - **LED Output Pin + GND** on the other diagonal pair.

---

## ⚔️ 2-Player Bluetooth Battle Mode Setup

1. **Player 1 (Host)**:
   - On the READY screen, press **Button 2 (B2)**.
   - OLED displays: `ROLE: HOST` and `WAITING FOR P2...`
2. **Player 2 (Joiner)**:
   - On the READY screen, press **Button 3 (B3)**.
   - OLED displays: `ROLE: JOINING...` and automatically connects to Player 1's Bluetooth (`WhackMole-Host`).
3. **Battle Start!**:
   - Once connected, both displays synchronize and trigger a countdown: `3..2..1..GO!`.
   - Play 30 seconds of Whack-A-Mole while viewing your live score and Player 2's score side-by-side!
   - At 0 seconds, victory or defeat sound effects play and the higher score is declared the **WINNER**!

---

## 🛠️ How to Build & Flash

### Option A: Arduino IDE
- **For 30-Pin / 28-Pin Board**: Open [`arduino/esp32_whack_a_mole/esp32_whack_a_mole.ino`](file:///f:/test-esp/arduino/esp32_whack_a_mole/esp32_whack_a_mole.ino).
- **For 38-Pin Board**: Open [`arduino/esp32_whack_a_mole_38pin/esp32_whack_a_mole_38pin.ino`](file:///f:/test-esp/arduino/esp32_whack_a_mole_38pin/esp32_whack_a_mole_38pin.ino).
- **For 2-Player Bluetooth Battle**: Open [`arduino/esp32_whack_a_mole_battle.ino`](file:///f:/test-esp/arduino/esp32_whack_a_mole_battle.ino).

Select Board: **ESP32 Dev Module** and click **Upload**!

### Option B: ESP-IDF 5.x
1. **Build Firmware**:
   ```powershell
   .\build.bat
   ```
2. **Flash & Run Serial Monitor**:
   *(Replace `COM3` with your ESP32's COM port)*
   ```powershell
   .\flash_monitor.bat COM3
   ```

---

## 📂 Project Structure

```
.
├── arduino/
│   ├── esp32_whack_a_mole/       # 30-Pin / 28-Pin Arduino Sketch
│   │   └── esp32_whack_a_mole.ino
│   ├── esp32_whack_a_mole_38pin/ # Dedicated 38-Pin Module Sketch
│   │   └── esp32_whack_a_mole_38pin.ino
│   └── esp32_whack_a_mole_battle.ino # 2-Player Bluetooth Battle Sketch
├── main/
│   ├── main.c                    # ESP-IDF C version (Bluetooth Battle supported)
│   └── CMakeLists.txt            # Component build rules
├── CMakeLists.txt                # Main project CMake config
├── esp32_gpio_guide.md           # Detailed hardware & wiring reference
├── build.bat                     # ESP-IDF compile script
├── flash_monitor.bat             # Flash & launch serial monitor script
├── clear_screen.bat              # Erase OLED display script
└── README.md
```
