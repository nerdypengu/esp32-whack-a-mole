# ESP32 Hardware Wiring & GPIO Reference Guide

---

## 1. 4-Pin Push Button & Illuminated Switch Wiring

### Internal Structure of a 4-Pin Switch
Inside a standard 4-pin push button or illuminated switch, the 4 pins are connected as **two internal pairs**:

```
        Pin A1 [--- Metal Bar ---] Pin A2
                        |
                  [ Push Switch ]
                        |
        Pin B1 [--- Metal Bar ---] Pin B2
```

- **Pin A1** and **Pin A2** are permanently connected inside the switch.
- **Pin B1** and **Pin B2** are permanently connected inside the switch.
- Pressing the button pushes the internal switch down to connect Pair A to Pair B.

### Why Wiring Side-by-Side Failed (Constant "PRESSED" State)
If you connect your ESP32 GPIO pin to **Pin A1** and GND to **Pin A2**, current flows through the permanent internal metal bar **24/7**, regardless of whether the button is pressed!
- The ESP32 pin is shorted to `GND` (0V) constantly.
- The code interprets `0V` as `PRESSED` all the time.
- Disconnecting the wire lets the ESP32 internal pull-up return the pin to `3.3V` (`RELEASED`).

### The Solution: Diagonal Pin Wiring
To guarantee that the circuit only completes when the button is pushed, connect your wires to **DIAGONAL opposite pins**:

```
        (ESP32 GPIO) ---> Pin A1 ------- Pin A2
                                           |  (Switch Opens/Closes)
                          Pin B1 ------- Pin B2 <--- (GND)
```

- **Wire 1 (GPIO Input)**: Connect to **Top-Left** (Pin A1)
- **Wire 2 (GND)**: Connect to **Bottom-Right** (Pin B2)

By using diagonal pins, current **MUST** pass through the push-activated switch contact.

---

## 2. ESP32 GPIO Pin Classification & Restrictions

Not all GPIO pins on the ESP32 are created equal. Below is the complete breakdown of which pins can be used and why.

```
+-----------------------------------------------------------------------------+
|                               ESP32 GPIO MAP                                |
+------------------+------------------+------------------+--------------------+
|  SAFE I/O PINS   |   INPUT ONLY     |  BOOT STRAPPING  |   FLASH MEMORY     |
| (Input & Output) |   (No Output)    |  (Use Caution)   |    (DO NOT USE)    |
+------------------+------------------+------------------+--------------------+
| GPIO 18, 19, 21  | GPIO 34          | GPIO 0           | GPIO 6             |
| GPIO 22, 23      | GPIO 35          | GPIO 2           | GPIO 7             |
| GPIO 25, 26, 27  | GPIO 36 (VP)     | GPIO 5           | GPIO 8             |
| GPIO 32, 33      | GPIO 39 (VN)     | GPIO 12, 15      | GPIO 9, 10, 11     |
+------------------+------------------+------------------+--------------------+
```

---

### Category A: Safe General-Purpose Pins (RECOMMENDED)
**Pins**: `GPIO 18`, `GPIO 19`, `GPIO 21`, `GPIO 22`, `GPIO 23`, `GPIO 25`, `GPIO 26`, `GPIO 27`, `GPIO 32`, `GPIO 33` (also `GPIO 4`, `GPIO 5` if not used for I2C).

- ✅ **Full Input & Output**: Can drive LEDs, relays, speakers, or read buttons and digital sensors.
- ✅ **Internal Pull-Up / Pull-Down**: Built-in resistors selectable in code.
- ✅ **No Boot Side-Effects**: Safe to connect during power-on.

---

### Category B: Input-Only Pins (GPI Pins)
**Pins**: `GPIO 34`, `GPIO 35`, `GPIO 36` (VP), `GPIO 39` (VN).

- ❌ **NO Output Capability**: These pins lack output drivers. Configuring them as `GPIO_MODE_OUTPUT` will fail or do nothing. You **cannot** drive an LED on D34/D35/D36/D39.
- ❌ **NO Internal Pull-Up/Pull-Down**: They do not have built-in pull-up resistors. If used for buttons, you **must** connect a physical external 10kΩ resistor to 3.3V.
- ✅ **Best Use**: Analog Sensor Inputs (ADC), battery level monitor, or external active sensors.

---

### Category C: Flash Memory Pins (DO NOT USE)
**Pins**: `GPIO 6`, `GPIO 7`, `GPIO 8`, `GPIO 9`, `GPIO 10`, `GPIO 11`.

- ❌ **RESTRICTED**: These 6 pins are connected internally to the onboard SPI Flash memory chip containing your program code.
- ⚠️ Connecting components or reconfiguring these pins will **crash the ESP32 or prevent it from booting**.

---

### Category D: Boot Strapping Pins (Use with Caution)
**Pins**: `GPIO 0`, `GPIO 2`, `GPIO 5`, `GPIO 12`, `GPIO 15`.

These pins control the hardware boot mode when the ESP32 powers on:

| Pin | Boot Behavior | Risk / Warning |
|---|---|---|
| **GPIO 0** | Pulled **LOW** enters Serial Flashing Mode. | If pulled LOW by a button at boot, ESP32 won't run your main code. |
| **GPIO 2** | Connected to Onboard LED on many DevKits. Must be LOW/floating at boot. | External circuitry may prevent bootloader mode. |
| **GPIO 5** | Outputs PWM signal at boot. Controls SDIO slave timing. | May flicker attached LEDs during startup. |
| **GPIO 12** | Controls Flash Voltage (1.8V vs 3.3V). | If pulled HIGH at boot, flash voltage fails and ESP32 boot-loops. |
| **GPIO 15** | Controls SILENT boot log output. | Pulling LOW enables debug logging at boot. |

---

## 3. Recommended Pin Assignment Summary

| Application | Recommended Pins | Notes |
|---|---|---|
| **OLED I2C Display** | **SDA**: `D21` or `D4`<br>**SCL**: `D19` or `D5` | ESP32 software I2C allows re-mapping to any safe I/O. |
| **Push Buttons** | `D23`, `D21`, `D18`, `D32`, `D33` | Enables `.pull_up_en = GPIO_PULLUP_ENABLE` without external resistors. |
| **LED Outputs** | `D22`, `D19`, `D4`, `D33`, `D25`, `D26` | Standard output driving pins. |
| **Analog Sensors** | `D34`, `D35`, `D36`, `D39` | High-precision ADC inputs. |
