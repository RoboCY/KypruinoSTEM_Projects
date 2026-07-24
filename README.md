# Kypruino STEM Projects

Open build files for the STEM projects published on the [ROBO CY blog](https://robo.com.cy/blogs/blog). Every project folder contains the Arduino sketch, the 3D-printable parts, wiring notes and a link to the full write-up.

All projects run on the **[Kypruino UNO+](https://robo.com.cy/products/kypruino-uno-v0-6)** (ATmega328P, Arduino UNO compatible) with onboard NeoPixels, buzzer, buttons and a dedicated I2C/OLED port. A plain Arduino UNO works too, with extra manual wiring for the onboard parts.

> **Free for hobbyists and makers. Attribution required. Commercial use is not permitted without a licence.** See [Licence](#licence).

---

## Repository layout

```
kypruino-stem-projects/
├── 01-spooky-pumpkin/
│   ├── README.md
│   ├── code/          Arduino sketch (.ino)
│   ├── stl/           3D printable parts (.stl / .3mf)
│   └── images/
├── 02-easter-egg/
├── 03-magicmotion-mood-lamp/
├── 04-smart-street-light/
├── 05-mini-nfc-banking-terminal/
├── 06-mini-theremin/
├── 07-smart-plant-monitor/
├── LICENSE-CODE
├── LICENSE-HARDWARE
└── README.md
```

Each project folder follows the same pattern: `README.md`, `code/`, `stl/`, `images/`.

---

## Projects

### `01-spooky-pumpkin/`
**Interactive 3D Printed Spooky Pumpkin** — A motion-activated Halloween prop. A PIR sensor detects passers-by, the onboard NeoPixels flicker orange/red like a candle (random brightness 75–255) and the buzzer plays the opening of the Halloween theme, then cools down and re-arms. Printed in transparent PLA/PETG so the shell glows.

| | |
|---|---|
| **Hardware** | Kypruino, PIR motion sensor, 3× male–female DuPont wires, USB-C cable |
| **Wiring** | PIR VCC → 5V · GND → GND · OUT → D7 |
| **Libraries** | Adafruit NeoPixel |
| **Print** | Transparent filament; flat internal base for board mounting, rear hole for USB-C |
| **Guide** | [Read the build](https://robo.com.cy/blogs/blog/create-your-own-kypruino-arduino-spooky-3d-printed-pumpkin) |

*Model attribution: based on "Jackie Jack-o-Lantern" by BirdBott on [Thingiverse](https://www.thingiverse.com/thing:4613024), modified with a flat base and cable pass-through.*

---

### `02-easter-egg/`
**Interactive 3D Printed Easter Egg** — An egg that celebrates when it is found. PIR motion detection triggers a quick LED flash, a smooth pastel colour animation and a short reward sound, then resets. Two-part translucent print for easy access to the electronics.

| | |
|---|---|
| **Hardware** | Kypruino, PIR motion sensor, 3× male–female DuPont wires, USB cable |
| **Wiring** | PIR VCC → 5V · GND → GND · OUT → D7 |
| **Libraries** | Adafruit NeoPixel |
| **Print** | Translucent filament, hollow two-part shell, USB opening, stable base |
| **Guide** | [Read the build](https://robo.com.cy/blogs/blog/create-your-own-interactive-easter-egg-with-kypruino-arduino) |

---

### `03-magicmotion-mood-lamp/`
**MagicMotion DIY Mood Lamp** — A gesture-controlled lamp with no buttons or app. Tilt to change brightness, shake to cycle colour, flip upside down to switch off. Colours cross-fade smoothly and each has its own idle animation. Startup calibration records a reference gravity vector.

| | |
|---|---|
| **Hardware** | Kypruino, MPU6050 accelerometer/gyro, NeoPixel ring or onboard NeoPixels, USB cable |
| **Wiring** | MPU6050 SDA → A4 · SCL → A5 · VCC → 5V · GND → GND · NeoPixel DIN → D8 |
| **Libraries** | Adafruit NeoPixel, MPU6050 (I2C) |
| **Print** | Cylindrical multi-part enclosure, fuzzy-skin outer shell for light diffusion, USB opening |
| **Guide** | [Read the build](https://robo.com.cy/blogs/blog/magicmotion-diy-mood-lamp-kypruino-neopixel) |

---

### `04-smart-street-light/`
**Smart Street Light Model for Dark Skies** — A planetarium demonstration model for light-pollution awareness. An LDR module reads ambient light; below a threshold the model switches on a downward-facing warm white street lamp while the onboard NeoPixels light the house interior in a cooler white. Lamp wiring is routed underground into the house for a clean presentation.

| | |
|---|---|
| **Hardware** | Kypruino, LDR light sensor module, warm white LED + series resistor, USB cable |
| **Wiring** | LDR VCC → 5V · GND → GND · SIG → A0 · LED anode → D10 via resistor · cathode → GND |
| **Libraries** | Adafruit NeoPixel |
| **Print** | House enclosure (roof slot for LDR), street lamp, base section with underground wire channel |
| **Tuning** | Adjust `int darkThreshold = 500;` and the comparison direction to suit your LDR module |
| **Guide** | [Read the build](https://robo.com.cy/blogs/blog/how-to-build-a-smart-street-light-model-for-dark-skies) |

---

### `05-mini-nfc-banking-terminal/`
**Kypruino Mini Banking System** — A simulated card-based banking terminal. Tap an RFID card to open an account, then deposit, withdraw or transfer virtual money to a second card. Balances are stored in EEPROM as cents so they survive power loss. Onboard buttons drive the menu and digit editor; NeoPixels and buzzer give accept/decline feedback.

| | |
|---|---|
| **Hardware** | Kypruino, RC522 RFID reader, 128×32 SSD1306 I2C OLED, 13.56 MHz MIFARE-style cards/tags, USB-C cable |
| **Wiring** | RC522 SDA → D10 · SCK → D13 · MOSI → D11 · MISO → D12 · RST → D5 · **3.3V (not 5V)** · GND → GND<br>OLED → dedicated I2C port (VCC/GND/SDA/SCL) |
| **Onboard** | Buttons A/B/C/D → D7/D6/D4/D2 · NeoPixels → D8 · Buzzer → D9 |
| **Libraries** | MFRC522, Adafruit SSD1306, Adafruit GFX, Adafruit NeoPixel, Adafruit BusIO (SPI/Wire/EEPROM built in) |
| **Config** | `accountNames[]`, `STARTING_BALANCE_CENTS`, `RESET_ALL_ACCOUNTS` |
| **Print** | Optional terminal-style enclosure |
| **Guide** | [Read the build](https://robo.com.cy/blogs/blog/kypruino-mini-nfc-banking-terminal) |

> ⚠️ Educational simulation only. No connection to real banks or payment networks. Do not use real bank cards.

---

### `06-mini-theremin/`
**Kypruino Mini Theremin** — A gesture-played instrument in a spherical shell. Hand distance from an ultrasonic sensor maps to chromatic notes from C4 to C6, with stability filtering so notes do not jump. A potentiometer sets vibrato depth. Sound comes from the onboard buzzer via `tone()`.

| | |
|---|---|
| **Hardware** | Kypruino, HC-SR04 ultrasonic sensor, potentiometer, USB-C cable |
| **Wiring** | HC-SR04 VCC → 5V · GND → GND · TRIG → D4 · ECHO → D5<br>Pot S → A0 · V → 5V · G → GND · Buzzer → D9 (onboard) |
| **Libraries** | None — standard Arduino + `math.h` |
| **Config** | `CLOSE_IS_HIGH`, `MIN_DISTANCE_CM = 5.0`, `MAX_DISTANCE_CM = 50.0` |
| **Print** | Two-half spherical enclosure with front sensor openings, pot slot and internal board supports |
| **Guide** | [Read the build](https://robo.com.cy/blogs/blog/kypruino-mini-theremin-ultrasonic-musical-instrument) |

---

### `07-smart-plant-monitor/`
**Smart Plant Monitor** — A plant pot that tells you how the plant feels. Reads soil moisture, ambient light, temperature and humidity, derives VPD (vapour pressure deficit) and a weighted comfort score, then cycles three OLED screens: an animated mood face, live sensor values and a suggested action. Ambient light is judged on a rolling 24-hour average so it does not complain every night.

| | |
|---|---|
| **Hardware** | Kypruino, 0.96" SSD1306 128×64 I2C OLED, capacitive soil moisture sensor, analog light sensor module, AHT10 temp/humidity sensor, USB-C cable |
| **Wiring** | OLED + AHT10 → shared I2C (VCC/GND/SDA/SCL) · Soil AOUT → A0 · Light AO → A1 (DO unused) |
| **Libraries** | Adafruit SSD1306, Adafruit GFX, Adafruit AHTX0, Adafruit BusIO |
| **Config** | Uncomment one care profile: `PROFILE_DRY_LOVING`, `PROFILE_BRIGHT_HERB`, `PROFILE_BALANCED_HOUSEPLANT`, `PROFILE_FLOWERING`, `PROFILE_HUMID_LOVING`, `PROFILE_ORCHID_AIRY`, `PROFILE_CUSTOM` |
| **Print** | Pot with a flat side for external board/OLED mounting, plus slots for the light and AHT10 sensors |
| **Guide** | [Read the build](https://robo.com.cy/blogs/blog/smart-plant-monitor-kypruino-oled) |

---

## Getting started

1. **Install the Arduino IDE** — [arduino.cc/en/software](https://www.arduino.cc/en/software)
2. **Select the board** — Tools → Board → *Arduino UNO* (the Kypruino is UNO-compatible), then pick the correct port.
3. **Install the libraries** listed for your chosen project via Sketch → Include Library → Manage Libraries…
4. **Open the sketch** from that project's `code/` folder and upload.
5. **Print the parts** from `stl/`. Unless noted, 0.2 mm layer height, 15–20 % infill, no supports needed on most parts. Light-diffusing builds (pumpkin, egg, lamp) want transparent or translucent filament.
6. **Wire it up** using the table in the project folder, then assemble.

Prefer to work in the browser? [Kypruino Studio](https://robo.com.cy/pages/apps) offers a block-based alternative to the Arduino IDE.

---

## Contributing

Issues and pull requests are welcome — improved sketches, better print geometry, bug fixes, translations. Please keep the folder structure consistent and include a photo of your build in `images/` if you can. By contributing you agree your contribution is released under the same licences below.

---

## Licence

This repository is **free for personal, hobbyist, maker and classroom use, with attribution. Commercial use is not permitted without a separate licence.**

| Content | Licence |
|---|---|
| 3D models, STL/3MF files, documentation, images | [Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)](https://creativecommons.org/licenses/by-nc/4.0/) |
| Arduino sketches and all other source code | [PolyForm Noncommercial License 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0/) |

**You may:** build these projects at home, in a makerspace or in a classroom; print the parts for yourself, your students or your friends; modify the code and models; share your modified versions under the same terms.

**You may not, without written permission:** sell printed parts, kits, assembled units or firmware derived from this repository; use these files in a paid product, paid course, paid workshop or any other commercial offering; remove or obscure attribution.

**Attribution format:**

> Based on Kypruino STEM Projects by ROBO (Robo Educational & Research Robotics Ltd, Cyprus) — https://robo.com.cy — licensed CC BY-NC 4.0 / PolyForm Noncommercial 1.0.0.

Attribution must appear in the README or documentation of any redistributed version, and on any public posting, video or listing of a build.

**Third-party models:** where a project is derived from someone else's design, the original creator and their licence are named in that project's folder. Those terms apply in addition to the terms above.

**Commercial licensing:** paid workshops, kit production, resale, classroom products or any commercial use — contact **support@robo.com.cy**.

No warranty. Provided "as is". You are responsible for the safety of anything you build, wire or power.

---

## About

Built by [ROBO](https://robo.com.cy) — Robo Educational & Research Robotics Ltd, Cyprus. Makers of the Kypruino UNO+, Cyprus's first Arduino-compatible board, deployed in over 100 schools.

[Blog](https://robo.com.cy/blogs/blog) · [Shop](https://robo.com.cy/collections) · [Apps](https://robo.com.cy/pages/apps) · [Educators](https://robo.com.cy/pages/educators) · [YouTube](https://www.youtube.com/@robo4601) · [Instagram](https://www.instagram.com/robo.com.cy/)
