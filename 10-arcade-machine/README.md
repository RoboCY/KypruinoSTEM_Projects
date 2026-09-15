# Kypruino Mini Arcade Machine

A pocket arcade with twelve games on a single ATmega328P. The Kypruino is held portrait with the USB-C connector facing down, so its four onboard buttons form a cross that works as a D-pad. A 1.8" colour LCD mounted landscape is the main screen, and a narrow 128×32 OLED mounted portrait beside it is a live "second screen" that shows score, lives, gauges, combo counters and cheeky messages while you play. The onboard buzzer plays music and effects and the three NeoPixels light up with the action.

No external libraries are needed. The LCD, OLED, I2C, NeoPixel and sound drivers are written inside the sketch so that everything fits in 32 KB of flash.

## Games

| Game | How to play | Second screen |
|---|---|---|
| **Simon Says** | Watch the four pads light up, repeat with the matching buttons | Round, score, WATCH / YOUR TURN |
| **Snake** | Steer with the cross, eat fruit, do not bite yourself | Length gauge, score, YUM! |
| **Tetris** | LEFT/RIGHT move, UP rotates, DOWN drops; Korobeiniki plays along | Level, lines, reactions |
| **2048** | Slide the tiles with the cross, merge to 2048 | Highest tile, score |
| **Breakout** | LEFT/RIGHT paddle, UP launches; 5 rows of bricks, 3 lives | Lives, level, score |
| **Space Shooter** | LEFT/RIGHT move, UP fires, DOWN smart bomb; waves of aliens | Lives, wave, bomb status |
| **Lane Racer** | LEFT/RIGHT change lane, UP boost, DOWN brake | Speedometer, score |
| **Frogger** | Hop across the road and the river to the 5 homes | Lives, homes, HOP! |
| **Flappy** | Press UP to flap through the pipes | Altitude gauge, score |
| **Whack-a-Mole** | Hit the button of the hole where the mole pops up; avoid bombs | Reaction time in ms, lives |
| **Beat Lanes** | Rhythm game: press the lane's button as the note reaches the line | Combo, health, PERFECT / GOOD / MISS |
| **Kypruino Pet** | Feed, play, clean and put your pet to sleep; it is saved in EEPROM | Four stat gauges, age, mood |

Menu: UP/DOWN choose a game, RIGHT starts it. In a game, hold LEFT + RIGHT together to pause (UP resumes, DOWN quits). Best scores are stored in EEPROM per game. The menu also has a button test and a sound on/off switch, and after 40 s of no input the machine goes into an attract mode.

## Build

| | |
|---|---|
| **Code** | [`code/KypruinoArcade/`](code/KypruinoArcade/) (open `KypruinoArcade.ino`, the other files load with it) |
| **3D parts** | [`stl/`](stl/) |
| **Hardware** | Kypruino, 1.8" ST7735 128×160 SPI TFT LCD, 0.91" 128×32 SSD1306 I2C OLED, jumper wires, USB-C cable |
| **LCD wiring** | VCC → 5V (or 3.3V, check your module) · GND → GND · SCK/CLK → D13 · SDA/MOSI → D11 · CS → D10 · A0/DC → D5 · RESET → D3 · LED → 5V (through the module's resistor, or 3.3V) |
| **OLED wiring** | Kypruino I2C/OLED port (VCC / GND / SDA / SCL), address 0x3C |
| **Onboard** | Buttons C = UP (D4) · A = DOWN (D7) · B = LEFT (D6) · D = RIGHT (D2) · NeoPixels → D8 · Buzzer → D9 |
| **Libraries** | None |
| **Config** | [`Config.h`](code/KypruinoArcade/Config.h): button-to-direction mapping, LCD rotation and colour order, OLED flip, brightness, and the list of games compiled into the build |
| **Guide** | [Read the full build](https://robo.com.cy/blogs/blog/kypruino-mini-arcade-machine) |

### Getting the screens right

- **Buttons** feel wrong? Run *Button test* from the menu and swap the pin numbers of `BTN_UP_PIN`, `BTN_DOWN_PIN`, `BTN_LEFT_PIN`, `BTN_RIGHT_PIN` in `Config.h` until the cross matches.
- **LCD upside down?** Change `LCD_ROTATION` from 1 to 3. Red and blue swapped? Set `LCD_BGR` to 0. A noisy strip along one edge means your module needs the "green tab" offset: try `LCD_COL_OFFSET 2` and `LCD_ROW_OFFSET 1`.
- **OLED text upside down?** Toggle `OLED_FLIP` between 1 and 0. The default suits the OLED plugged into the Kypruino I2C port with its pins at the bottom. The arcade runs fine without the OLED plugged in.

### Fitting games into 32 KB

Every game costs between 1 and 3 KB of flash. The full set of twelve compiles to about 29.9 KB of the 32 KB available (Arduino IDE reports 92 %), with 903 bytes of RAM used by globals. If you add your own game or the compiler reports the sketch is too big, set the games you do not need to 0 at the bottom of `Config.h`. Best scores keep their EEPROM slot even when a game is left out of a build.

### Writing your own game

Copy `GameSimon.cpp`, give it a new `GAME_...` switch in `Config.h`, an id in `Engine.h`, add the record to `Games.h` and the list in `KypruinoArcade.ino`. A game is three functions: `start()` draws the first frame, `update()` runs 40 times a second, and `hud()` redraws the OLED. Keep all state in the shared `gameRam` arena (300 bytes), keep tables and strings in PROGMEM, and draw only what changed on the LCD because there is no framebuffer.

## Notes

The LCD SPI runs at 8 MHz and the whole screen can be filled in about 40 ms, so games redraw only the sprites that moved. The OLED has a 512-byte framebuffer that is sent over I2C only when something changed. Sound comes from Timer1 toggling D9 in hardware, so notes cost no CPU time, and the NeoPixels are driven by a small bit-bang routine on D8.
