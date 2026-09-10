# 🔨 ESP32 Arcade Whack-A-Mole Game

An arcade-style Whack-A-Mole game built on ESP32 using **ESP-IDF 5.x**, featuring 4 illuminated push-buttons, an **SSD1306 OLED display**, and a **buzzer/speaker** for retro audio feedback.

---

## 🎮 Game Features

- **4 Active Moles**: Random mole pop-ups using illuminated push-buttons.
- **Dynamic Difficulty**: Moles pop up faster as your score increases (from 1.2s down to 0.45s per mole).
- **Retro Audio**: Custom tones for Hits (+100 pts), Misses (-30 pts), Mole escapes, and Game Over melodies.
- **OLED Arcade UI**:
  - **Ready Screen**: Flashing LEDs + High Score tracking.
  - **Live Gameplay**: Real-time timer countdown, current score, total hits & misses.
  - **Game Over Screen**: Final stats & instant restart trigger.
- **Flicker-Free OLED Engine**: Dual-pin architecture ensures zero LED flickering during I2C OLED display updates.

---

## 🔌 Hardware Setup & Pinout

### Component Summary
- **1x ESP32 Dev Board**
- **4x Illuminated Push-Buttons** (Mole 1 - Mole 4)
- **1x SSD1306 I2C OLED Display (128x64)**
- **1x Passive Buzzer or Speaker**

### 📍 Wiring Diagram

| Component | ESP32 Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **OLED Display** | `GPIO 4` (`D4`) | I2C SDA | 128x64 OLED (Address `0x3C`) |
| **OLED Display** | `GPIO 5` (`D5`) | I2C SCL | Internal pull-up enabled |
| **Speaker / Buzzer** | `GPIO 17` (`TX2`) | PWM Tone Output | LEDC channel (`GPIO 17` stays silent idle) |
| **Mole 1 (B1)** | Input: `GPIO 21` (`D21`)<br>LED: `GPIO 19` (`D19`) | Button & LED | Internal Pull-Up enabled |
| **Mole 2 (B2)** | Input: `GPIO 22` (`D22`)<br>LED: `GPIO 23` (`D23`) | Button & LED | Internal Pull-Up enabled |
| **Mole 3 (B3)** | Input: `GPIO 32` (`D32`)<br>LED: `GPIO 33` (`D33`) | Button & LED | Internal Pull-Up enabled |
| **Mole 4 (B4)** | Input: `GPIO 25` (`D25`)<br>LED: `GPIO 26` (`D26`) | Button & LED | Internal Pull-Up enabled |

> ℹ️ **Note on 4-Pin Illuminated Buttons**: Connect diagonally across opposite corners! 
> - **Input Pin + GND** on one diagonal pair.
> - **LED Output Pin + GND** on the other diagonal pair.

---

## 🛠️ How to Build & Flash

### Prerequisites
- Install **ESP-IDF v5.x** ([Official Setup Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)).

### Quick Start (Windows Helper Scripts)

1. **Build Firmware**:
   ```powershell
   .\build.bat
   ```

2. **Flash & Run Serial Monitor**:
   *(Replace `COM3` with your ESP32's COM port)*
   ```powershell
   .\flash_monitor.bat COM3
   ```

3. **Other Utility Scripts**:
   - Clean OLED screen on exit: `.\clear_screen.bat COM3`
   - Full Flash Erase: `.\erase_flash.bat COM3`

---

## 📂 Project Structure

```
.
├── main/
│   ├── main.c              # Core game loop, OLED driver, LEDC sound engine
│   └── CMakeLists.txt      # Component build rules
├── CMakeLists.txt          # Main project CMake config
├── esp32_gpio_guide.md     # Detailed hardware & wiring reference
├── build.bat               # ESP-IDF compile script
├── flash_monitor.bat       # Flash & launch serial monitor script
├── clear_screen.bat        # Erase OLED display script
└── README.md
```

---

## 🕹️ Controls & Gameplay Rules

1. **Start Game**: Press any of the 4 mole buttons on the **READY** screen.
2. **Whack a Mole**: When a mole lights up, press its button to score **+100 points**.
3. **Avoid Misses**: Pressing an unlit button deducts **-30 points**.
4. **Beat the Clock**: You have 30 seconds to score as high as possible!
