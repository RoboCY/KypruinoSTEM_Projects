/*
  Engine.h — what every game gets from the arcade:
  the LCD, the portrait OLED, the NeoPixels, the buzzer, the buttons, a shared
  RAM arena, a random generator, high scores in EEPROM and the game-over flow.

  A game is a PROGMEM Game record with three functions:
    start()  — set up state in gameRam and draw the first LCD frame
    update() — called every FRAME_MS (40 times a second)
    hud()    — redraw the whole OLED from the game's state
  When the player loses, call gameOver(); to end with a win, call gameWon().
*/
#pragma once
#include "Config.h"
#include "Lcd.h"
#include "Oled.h"
#include "Pixels.h"
#include "Sound.h"
#include "Input.h"
#include <avr/eeprom.h>

struct Game {
  uint8_t id;              // fixed id → EEPROM high-score slot (never reuse a number)
  const char* name;        // PROGMEM, shown in the LCD menu
  const char* tagline;     // PROGMEM, '\n'-separated lines of max 5 chars, shown on the OLED
  void (*init)();
  void (*update)();
  void (*hud)();
};

// Game ids (EEPROM slots)
enum {
  ID_SIMON = 0, ID_SNAKE, ID_TETRIS, ID_2048, ID_BREAKOUT, ID_SHOOTER,
  ID_RACER, ID_FROGGER, ID_FLAPPY, ID_WHACK, ID_BEAT, ID_PET
};

#define EEPROM_MAGIC_ADDR   ((uint8_t*)0)
#define EEPROM_SOUND_ADDR   ((uint8_t*)1)
#define EEPROM_BEST_ADDR    ((uint16_t*)8)    // 16 slots × 2 bytes → 8..39
#define EEPROM_PET_ADDR     ((uint8_t*)64)    // Kypruino Pet save game

extern const Game* const GAMES[] PROGMEM;   // defined in the .ino
extern const uint8_t GAME_COUNT;

extern uint8_t gameRam[GAME_RAM_SIZE];
extern uint16_t score;
extern uint16_t best;        // best score of the running game (read from EEPROM)
extern uint16_t frameNo;     // frames since the game started

void engineInit();
void engineMenu();           // runs the menu and the selected game; returns when back at the menu
void gameOver();             // end the game after this update (loss)
void gameWon();              // end the game after this update (win)
uint8_t rnd(uint8_t n);      // 0 .. n-1
uint16_t rnd16();

// OLED building blocks shared by the games
void hudScoreBlock(int16_t y);         // "SCORE" + score + "BEST" + best, 38 px tall
void hudMessage(const char* linesP);   // clears the bottom 32 px and centres the lines there
void hudLabel(int16_t y, const char* labelP, uint16_t value);  // label row + number row (18 px)
