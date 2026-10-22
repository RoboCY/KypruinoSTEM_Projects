/*
  Simon Says — watch the four pads light up, then repeat the sequence.
  UP / DOWN / LEFT / RIGHT press the matching pad. Score = rounds completed.
*/
#include "Games.h"
#if GAME_SIMON

namespace {

struct State {
  uint8_t seq[64];
  uint8_t len;       // current sequence length
  uint8_t step;      // position while showing / entering
  uint8_t phase;     // 0 = showing, 1 = your turn, 2 = round done
  uint8_t timer;     // frames left in the current sub-step
  uint8_t lit;       // pad currently lit, 0xFF = none
  bool padOn;        // showing: pad is in its lit part
};
State& s = *(State*)gameRam;
static_assert(sizeof(State) <= GAME_RAM_SIZE, "Simon state too big");

const uint8_t  PX[4] PROGMEM = { 60, 60, 12, 108 };   // UP, DOWN, LEFT, RIGHT
const uint8_t  PY[4] PROGMEM = { 4, 84, 44, 44 };
const uint16_t COL_DIM[4] PROGMEM = { RGB(120, 0, 0), RGB(0, 100, 0), RGB(120, 120, 0), RGB(0, 0, 140) };
const uint16_t COL_LIT[4] PROGMEM = { C_RED, C_GREEN, C_YELLOW, C_BLUE };
const uint8_t  NOTE[4] PROGMEM = { N_C5, N_E5, N_G5, N_C6 };
const uint8_t  HUE[4] PROGMEM = { 0, 85, 42, 170 };

const char S_WATCH[] PROGMEM = "WATCH";
const char S_TURN[]  PROGMEM = "YOUR\nTURN";
const char S_GOOD[]  PROGMEM = "GOOD!";
const char S_ROUND[] PROGMEM = "ROUND";

void drawPad(uint8_t i, bool lit) {
  lcdFillRect(pgm_read_byte(&PX[i]), pgm_read_byte(&PY[i]), 40, 40,
              pgm_read_word(lit ? &COL_LIT[i] : &COL_DIM[i]));
}

void drawRound() {
  lcdFillRect(60, 44, 40, 40, C_BLACK);
  lcdNumber(s.len < 10 ? 86 : 92, 56, s.len, C_WHITE, C_BLACK, 2);
}

void light(uint8_t i) {
  s.lit = i;
  drawPad(i, true);
  soundNote(pgm_read_byte(&NOTE[i]));
  pixAllHue(pgm_read_byte(&HUE[i]));
}

void unlight() {
  if (s.lit == 0xFF) return;
  drawPad(s.lit, false);
  s.lit = 0xFF;
  soundOff();
  pixAll(0, 0, 0);
}

void hud() {
  oledClear();
  static const char T[] PROGMEM = "SIMON";
  oledTextCenterP(2, T, 1);
  hudLabel(16, S_ROUND, s.len);
  hudScoreBlock(40);
  hudMessage(s.phase == 0 ? S_WATCH : s.phase == 1 ? S_TURN : S_GOOD);
}

void startShow() {
  s.phase = 0;
  s.step = 0;
  s.timer = 20;
  s.padOn = false;
  hud();
}

void start() {
  s.len = 1;
  s.seq[0] = rnd(4);
  s.lit = 0xFF;
  for (uint8_t i = 0; i < 4; i++) drawPad(i, false);
  drawRound();
  startShow();
}

void update() {
  if (s.phase == 0) {                       // ---- showing the sequence
    if (s.timer) { s.timer--; return; }
    if (s.padOn) {
      unlight();
      s.padOn = false;
      s.step++;
      s.timer = 5;
      if (s.step >= s.len) { s.phase = 1; s.step = 0; hud(); }
    } else {
      light(s.seq[s.step]);
      s.padOn = true;
      s.timer = s.len > 8 ? 8 : 16 - s.len;  // faster as the sequence grows
    }
    return;
  }

  if (s.phase == 2) {                       // ---- short pause after a good round
    if (s.timer) { s.timer--; return; }
    s.seq[s.len] = rnd(4);
    if (s.len < 63) s.len++;
    drawRound();
    startShow();
    return;
  }

  // ---- phase 1: the player's turn
  if (s.lit != 0xFF && (s.timer == 0 || !(btnHeld & (1 << s.lit)))) unlight();
  if (s.timer) s.timer--;
  if (!btnPressed) return;
  uint8_t pad = btnPressed & BTN_UP ? 0 : btnPressed & BTN_DOWN ? 1 : btnPressed & BTN_LEFT ? 2 : 3;
  unlight();
  light(pad);
  s.timer = 10;
  if (pad != s.seq[s.step]) {
    fxFlash(255, 0, 0, 20);
    gameOver();
    return;
  }
  s.step++;
  if (s.step >= s.len) {
    score = s.len;
    s.phase = 2;
    s.timer = 24;
    fxFlash(0, 255, 0, 12);
    hud();
  }
}

const char NAME[] PROGMEM = "Simon Says";
const char TAG[]  PROGMEM = "SIMON\nWATCH\nTHEN\nREPEA\nT IT!";

} // namespace

const Game gameSimon PROGMEM = { ID_SIMON, NAME, TAG, start, update, hud };

#endif
