/*
  ============================================================
  KypruinoArcade — Kypruino Mini Arcade Machine
  Part of the Kypruino STEM Projects by ROBO (robo.com.cy)
  Guide: https://robo.com.cy/blogs/blog/kypruino-mini-arcade-machine
  ============================================================

  A pocket arcade built around the Kypruino held portrait (USB-C down), so
  the four onboard buttons form a cross:

    LCD   : 1.8" ST7735 128x160 SPI colour screen, mounted landscape
    OLED  : 128x32 SSD1306 I2C, mounted portrait, shows live game stats,
            gauges and fun messages next to the main screen
    Sound : onboard buzzer on D9 (Timer1), music + effects
    Light : 3 onboard NeoPixels on D8, reacting to the game
    Input : onboard buttons C = UP, A = DOWN, B = LEFT, D = RIGHT

  No external libraries are needed: the LCD, OLED, NeoPixel and I2C drivers
  are written in this sketch so that everything fits in 32 KB.

  Controls
    Menu     : UP/DOWN choose, RIGHT start
    In game  : hold LEFT + RIGHT together = pause (UP resumes, DOWN quits)

  Files
    Config.h        pins, screen orientation, which games are compiled in
    Engine.*        menu, game loop, pause, high scores, OLED helpers
    Lcd.*  Oled.*   display drivers        Pixels.*  Sound.*  Input.*
    Game*.cpp       one file per game — copy one to write your own
*/

#include "Games.h"

const Game* const GAMES[] PROGMEM = {
#if GAME_SIMON
  &gameSimon,
#endif
#if GAME_SNAKE
  &gameSnake,
#endif
#if GAME_TETRIS
  &gameTetris,
#endif
#if GAME_2048
  &game2048,
#endif
#if GAME_BREAKOUT
  &gameBreakout,
#endif
#if GAME_SHOOTER
  &gameShooter,
#endif
#if GAME_RACER
  &gameRacer,
#endif
#if GAME_FROGGER
  &gameFrogger,
#endif
#if GAME_FLAPPY
  &gameFlappy,
#endif
#if GAME_WHACK
  &gameWhack,
#endif
#if GAME_BEAT
  &gameBeat,
#endif
#if GAME_PET
  &gamePet,
#endif
};

const uint8_t GAME_COUNT = sizeof(GAMES) / sizeof(GAMES[0]);

void setup() {
  engineInit();
}

void loop() {
  engineMenu();
}
