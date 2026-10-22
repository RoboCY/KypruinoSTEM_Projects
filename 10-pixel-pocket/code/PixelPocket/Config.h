/*
  Config.h — Kypruino Arcade
  Pins, screen orientation and the list of games compiled into this build.

  The ATmega328P has 32 KB of flash. Every game costs roughly 1.5–3 KB.
  If the compiler reports the sketch is too big, set some games to 0 below.
*/
#pragma once
#include <Arduino.h>

// Direct port access for a constant pin number (folds to a single instruction)
#define PIN_PORT(p) ((p) < 8 ? &PORTD : (p) < 14 ? &PORTB : &PORTC)
#define PIN_DDR(p)  ((p) < 8 ? &DDRD  : (p) < 14 ? &DDRB  : &DDRC)
#define PIN_IN(p)   ((p) < 8 ? &PIND  : (p) < 14 ? &PINB  : &PINC)
#define PIN_MASK(p) ((uint8_t)(1 << ((p) < 8 ? (p) : (p) < 14 ? (p) - 8 : (p) - 14)))
#define PIN_HIGH(p)   (*PIN_PORT(p) |= PIN_MASK(p))
#define PIN_LOW(p)    (*PIN_PORT(p) &= (uint8_t)~PIN_MASK(p))
#define PIN_OUTPUT(p) (*PIN_DDR(p) |= PIN_MASK(p))
#define PIN_PULLUP(p) (*PIN_DDR(p) &= (uint8_t)~PIN_MASK(p), *PIN_PORT(p) |= PIN_MASK(p))
#define PIN_READ(p)   (*PIN_IN(p) & PIN_MASK(p))

void waitMs(uint16_t ms);   // millis()-based delay, smaller than delay()

// -------------------- Buttons (onboard, INPUT_PULLUP, active LOW) --------------------
// Kypruino held portrait with the USB-C connector facing DOWN, so the four
// buttons form a cross: C = UP, A = DOWN, B = LEFT, D = RIGHT.
// If a direction feels wrong, swap the pin numbers here.
// (Menu → "Button test" shows which button is which.)
#define BTN_UP_PIN     4      // button C
#define BTN_DOWN_PIN   7      // button A
#define BTN_LEFT_PIN   6      // button B
#define BTN_RIGHT_PIN  2      // button D

// -------------------- Onboard NeoPixels + buzzer --------------------
#define PIXEL_COUNT    3
#define PIXEL_PIN      8      // must stay on D8 (PORTB bit 0) for the bit-bang driver
#define BUZZER_PIN     9      // must stay on D9 (OC1A): driven by Timer1 hardware
#define PIXEL_BRIGHTNESS 64   // 0–255 global scale for the NeoPixels

// -------------------- 1.8" ST7735 128x160 SPI LCD --------------------
// SCK → D13, MOSI (SDA) → D11 (hardware SPI). The rest are configurable:
#define LCD_CS_PIN     10
#define LCD_DC_PIN     5
#define LCD_RST_PIN    3
// The LCD is mounted landscape (160 wide, 128 tall). If the picture is upside
// down, change 1 → 3. If red and blue are swapped, change LCD_BGR to 0.
#define LCD_ROTATION   1
#define LCD_BGR        1
// Some 1.8" modules ("green tab") need a small pixel offset. Try 2 and 1 if
// you see a noisy edge on the screen; most red-tab modules use 0 and 0.
#define LCD_COL_OFFSET 0
#define LCD_ROW_OFFSET 0

// -------------------- 128x32 SSD1306 OLED (I2C 0x3C, portrait) --------------------
// Plug it into the Kypruino I2C/OLED port. It is mounted portrait (32 wide,
// 128 tall), header pins at the bottom. If its text is upside down, toggle OLED_FLIP.
#define OLED_ADDR      0x3C
#define OLED_FLIP      1

// -------------------- Engine --------------------
#define FRAME_MS       25     // 40 frames per second
#define GAME_RAM_SIZE  300    // shared RAM arena for the running game (bytes)

// -------------------- Games in this build --------------------
// 1 = included, 0 = left out.
#ifndef GAME_SIMON
#define GAME_SIMON 1
#endif
#ifndef GAME_SNAKE
#define GAME_SNAKE 1
#endif
#ifndef GAME_TETRIS
#define GAME_TETRIS 1
#endif
#ifndef GAME_2048
#define GAME_2048 1
#endif
#ifndef GAME_BREAKOUT
#define GAME_BREAKOUT 1
#endif
#ifndef GAME_SHOOTER
#define GAME_SHOOTER 1
#endif
#ifndef GAME_RACER
#define GAME_RACER 1
#endif
#ifndef GAME_FROGGER
#define GAME_FROGGER 1
#endif
#ifndef GAME_FLAPPY
#define GAME_FLAPPY 1
#endif
#ifndef GAME_WHACK
#define GAME_WHACK 1
#endif
#ifndef GAME_BEAT
#define GAME_BEAT 1
#endif
#ifndef GAME_PET
#define GAME_PET 1
#endif
