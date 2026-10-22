/*
  Flappy Kypruino — a little yellow bird flies right through gaps in green pipes.
  Any button flaps (UP is the natural one). Hit a pipe, the ground or the top and
  it is over. Score = pipes passed. The OLED shows the score and an altitude gauge.
*/
#include "Games.h"
#if GAME_FLAPPY

namespace {

#define BIRD_X   32
#define GROUND   124            // y of the brown ground strip (play area is 0..123)
#define BIRD_MAX 116            // highest bird y that still clears the ground
#define GAP      44
#define PIPE_W   16
#define GRAV     3              // 1/16 px per frame^2
#define JUMP     40             // 2.5 px per frame
#define VMAX     64             // 4 px per frame

// Events returned by movePipe()
#define EV_PASS 1
#define EV_HIT  2

struct State {
  int16_t yf, vy;      // bird y and velocity, 4 fraction bits
  uint8_t y;           // bird y in pixels this frame
  uint8_t by;          // bird y as last drawn
  uint8_t px[2];       // pipe position: (left edge + 16) / 2, so 0..132 fits a byte
  uint8_t gap[2];      // top of each pipe's gap
  uint8_t alt;         // altitude level shown on the OLED (0..29)
  uint8_t msg;         // index into MSGS
  bool started;
};
State& s = *(State*)gameRam;
static_assert(sizeof(State) <= GAME_RAM_SIZE, "Flappy state too big");

const uint8_t BIRD[2][8] PROGMEM = {
  { 0x3C, 0x7E, 0x77, 0xBF, 0x9F, 0xFF, 0x7E, 0x3C },   // wing up
  { 0x3C, 0x7E, 0x77, 0xFF, 0xFF, 0x9F, 0x5E, 0x3C },   // wing down
};

#define C_PIPE RGB(0, 190, 0)

const char S_TITLE[] PROGMEM = "FLAPY";
const char S_ALT[]   PROGMEM = "ALT";
const char S_PRESS[] PROGMEM = "PRESS\nUP";
const char S_FLAP[]  PROGMEM = "FLAP!";
const char S_NICE[]  PROGMEM = "NICE";
const char S_WOW[]   PROGMEM = "WOW!";
const char S_OUCH[]  PROGMEM = "OUCH!";
const char* const MSGS[] PROGMEM = { S_PRESS, S_FLAP, S_NICE, S_WOW, S_OUCH };
enum { M_PRESS, M_FLAP, M_NICE, M_WOW, M_OUCH };

void setMsg(uint8_t i) {
  s.msg = i;
  hudMessage((const char*)pgm_read_ptr(&MSGS[i]));
}

void drawAlt() {
  oledVBar(10, 63, 12, 31, (s.alt * 7) >> 1);   // 29 levels → 0..100 %
}

void hud() {
  oledClear();
  oledTextCenterP(2, S_TITLE, 1);
  hudScoreBlock(12);
  oledTextCenterP(53, S_ALT, 1);
  drawAlt();
  setMsg(s.msg);
}

uint8_t newGap() { return 8 + rnd(GROUND - GAP - 16 + 1); }

// Scroll pipe i 2 px left, redrawing only its two edge columns.
// Returns EV_PASS when it has just cleared the bird, EV_HIT when it overlaps it.
__attribute__((noinline)) uint8_t movePipe(uint8_t i) {
  uint8_t p = s.px[i] - 1;
  uint8_t g = s.gap[i];
  s.px[i] = p;
  int16_t x = (int16_t)(p << 1) - PIPE_W;                    // left edge
  lcdFillRect(x + PIPE_W, 0, 2, GROUND, C_BLACK);            // old right edge
  if (x >= 0) {                                              // new left edge
    lcdFillRect(x, 0, 2, g, C_PIPE);                         // top pipe
    lcdFillRect(x, (uint8_t)(g + GAP), 2, (uint8_t)(GROUND - GAP - g), C_PIPE);  // bottom pipe
  }
  if (p == 0) { s.px[i] = (LCD_W + PIPE_W) / 2; s.gap[i] = newGap(); }
  if (p == BIRD_X / 2) return EV_PASS;
  if (p > BIRD_X / 2 && p < (BIRD_X + 8 + PIPE_W) / 2 && (s.y < g || s.y + 8 > g + GAP)) return EV_HIT;
  return 0;
}

void drawBird() {
  uint8_t y = s.y, o = s.by;
  if (y > o) lcdFillRect(BIRD_X, o, 8, y - o, C_BLACK);        // uncover rows above
  else if (y < o) lcdFillRect(BIRD_X, y + 8, 8, o - y, C_BLACK); // uncover rows below
  s.by = y;
  lcdBitmap(BIRD_X, y, BIRD[(frameNo >> 2) & 1], 8, 8, C_YELLOW, C_BLACK);
}

void start() {
  lcdClear(C_BLACK);
  lcdFillRect(0, GROUND, LCD_W, LCD_H - GROUND, C_BROWN);
  s.yf = 56 << 4;
  s.y = s.by = 56;
  s.px[0] = (LCD_W + PIPE_W) / 2;
  s.px[1] = (LCD_W + PIPE_W + 88) / 2;
  s.gap[0] = newGap();
  s.gap[1] = newGap();
  s.alt = (BIRD_MAX - 56) >> 2;
  pixAllHue(0);
  drawBird();
  hud();
}

void update() {
  uint8_t ev = 0;
  if (btnPressed) {
    s.vy = -JUMP;
    sfx(SFX_JUMP);
    if (!s.started) { s.started = true; setMsg(M_FLAP); }
  }

  if (s.started) {
    s.vy += GRAV;
    if (s.vy > VMAX) s.vy = VMAX;
    s.yf += s.vy;
    if ((uint16_t)s.yf > (BIRD_MAX << 4)) {       // above the top (negative) or into the ground
      s.yf = s.yf < 0 ? 0 : BIRD_MAX << 4;
      ev = EV_HIT;
    }
  } else {
    s.yf += (frameNo & 16) ? 2 : -2;      // hover and bob until the first flap
  }
  s.y = s.yf >> 4;

  if (s.started) ev |= movePipe(0) | movePipe(1);
  drawBird();
  uint8_t lvl = (BIRD_MAX - s.y) >> 2;
  if (lvl != s.alt) { s.alt = lvl; drawAlt(); }

  if (ev & EV_PASS) {
    score++;
    sfx(SFX_COIN);
    fxFlash(0, 255, 0, 6);
    pixAllHue(score << 4);
    setMsg((score & 7) ? M_NICE : M_WOW);
    hudScoreBlock(12);
  }
  if (ev & EV_HIT) {
    fxFlash(255, 0, 0, 20);
    sfx(SFX_HIT);
    setMsg(M_OUCH);
    gameOver();
  }
}

const char NAME[] PROGMEM = "Flappy Kypruino";
const char TAG[]  PROGMEM = "FLAPY\nTAP\nTO\nFLY";

} // namespace

const Game gameFlappy PROGMEM = { ID_FLAPPY, NAME, TAG, start, update, hud };

#endif
