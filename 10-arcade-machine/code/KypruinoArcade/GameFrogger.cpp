/*
  Frogger — hop the frog across five lanes of traffic and four rows of drifting
  logs into one of the five empty homes on the far bank. Fill all five homes to
  go up a level (faster traffic). UP / DOWN / LEFT / RIGHT hop one cell. On a log
  you drift with it; drifting off the edge, landing in the water or touching a
  car costs a life (3 lives). +10 for every new row reached, +50 per home.
*/
#include "Games.h"
#if GAME_FROGGER

namespace {

#define CELL      10
#define LANES     9
#define START_X   76
#define START_ROW 11
#define MAX_X     (LCD_W - 8)
#define NOINLINE  __attribute__((noinline))

#define C_WATER   RGB(0, 40, 160)
#define C_ROAD    RGB(32, 32, 32)
#define C_MEDIAN  RGB(120, 0, 160)

struct State {
  int16_t phase[LANES];   // lane scroll position, 1/4 px units, 0 .. period*4-1
  int16_t fx;             // frog x in pixels
  uint8_t row;            // frog row 0..11
  uint8_t lives;
  uint8_t level;
  uint8_t homes;          // bit k set = home slot k filled
  uint8_t nHome;          // homes filled this level
  uint8_t maxRow;         // furthest row reached this run (smallest number)
  uint8_t wait;           // pause frames after a death
  uint8_t fxT;            // frames left of the level-up rainbow
  const char* msg;        // PROGMEM message shown on the OLED
};
State& s = *(State*)gameRam;
static_assert(sizeof(State) <= GAME_RAM_SIZE, "Frogger state too big");

// One entry per moving lane: objects of len px repeat every period px and
// move speed quarter-pixels per frame (sign = direction). Lanes 0-3 are the
// river (rows 1-4, logs), lanes 4-8 the road (rows 6-10, cars).
struct Lane { uint8_t period, len; int8_t speed; uint16_t colour; };
const Lane LANE[LANES] PROGMEM = {
  { 64, 24, -3, C_BROWN },
  { 80, 40,  2, C_BROWN },
  { 56, 32, -4, C_BROWN },
  { 96, 48,  3, C_BROWN },
  { 64, 16, -3, C_YELLOW },
  { 48, 12,  4, C_CYAN },
  { 40, 12, -2, C_MAGENTA },
  { 72, 16,  5, C_WHITE },
  { 56, 12, -3, C_RED },
};

const char S_TITLE[] PROGMEM = "FROG";
const char S_HOMES[] PROGMEM = "HOME";
const char S_LVL[]   PROGMEM = "LV";
const char S_HOP[]   PROGMEM = "HOP!";
const char S_SPLAT[] PROGMEM = "SPLAT";
const char S_SPLSH[] PROGMEM = "SPLSH";
const char S_SAFE[]  PROGMEM = "SAFE!";
const char S_HOME[]  PROGMEM = "HOME!";

uint8_t laneRow(uint8_t i)   { return i < 4 ? i + 1 : i + 2; }
uint8_t rowLane(uint8_t r)   { return r < 5 ? r - 1 : r - 2; }
bool    isLaneRow(uint8_t r) { return (uint8_t)(r - 1) < 4 || (uint8_t)(r - 6) < 5; }
uint8_t lP(uint8_t i)        { return pgm_read_byte(&LANE[i].period); }
uint8_t lL(uint8_t i)        { return pgm_read_byte(&LANE[i].len); }
uint16_t laneBg(uint8_t i)   { return i < 4 ? C_WATER : C_ROAD; }
int16_t laneOff(uint8_t i)   { return s.phase[i] >> 2; }

// Is pixel column x inside one of the objects of lane i?
NOINLINE bool onObj(uint8_t i, int16_t x) {
  uint8_t P = lP(i);
  int16_t r = x - laneOff(i);
  if (r < 0) r += P;
  while (r >= P) r -= P;
  return r < lL(i);
}

// Repaint columns [x, x+w) of lane i: background, then every object touching
// the strip (drawn whole — the frog is redrawn on top every frame anyway).
NOINLINE void lanePaint(uint8_t i, int16_t x, uint8_t w) {
  int16_t y = laneRow(i) * CELL;
  uint8_t P = lP(i), L = lL(i);
  uint16_t c = pgm_read_word(&LANE[i].colour);
  lcdFillRect(x, y, w, CELL, laneBg(i));
  for (int16_t a = laneOff(i) - P; a < x + w; a += P)
    if (a + L > x) lcdFillRect(a, y, L, CELL, c);
}

// Advance lane i one frame and repaint only the columns that changed.
// Returns the pixel delta (signed).
int8_t laneStep(uint8_t i) {
  uint8_t P = lP(i), L = lL(i);
  int8_t v = (int8_t)pgm_read_byte(&LANE[i].speed);
  v += v < 0 ? -s.level : s.level;
  int16_t ph = s.phase[i] + v;
  int8_t d = (ph >> 2) - (s.phase[i] >> 2);
  int16_t P4 = (int16_t)P << 2;
  if (ph < 0) ph += P4; else if (ph >= P4) ph -= P4;
  s.phase[i] = ph;
  if (!d) return 0;
  int16_t y = laneRow(i) * CELL;
  uint16_t c = pgm_read_word(&LANE[i].colour), bg = laneBg(i);
  uint8_t w = d > 0 ? d : -d;
  for (int16_t a = laneOff(i) - P; a < LCD_W + 8; a += P) {
    // the object now covers [a, a+L): one w-wide strip became object, one background
    lcdFillRect(d > 0 ? a - w : a + L, y, w, CELL, bg);
    lcdFillRect(d > 0 ? a + L - w : a, y, w, CELL, c);
  }
  return d;
}

NOINLINE void drawFrog(int16_t x, int16_t y, uint16_t c) {
  lcdFillRect(x, y + 1, 8, 8, c);
  lcdPixel(x + 2, y + 2, C_WHITE);
  lcdPixel(x + 5, y + 2, C_WHITE);
}

NOINLINE void eraseFrog() {
  uint8_t r = s.row;
  if (isLaneRow(r)) lanePaint(rowLane(r), s.fx, 8);
  else lcdFillRect(s.fx, r * CELL, 8, CELL, r == 5 ? C_MEDIAN : C_GREY);
}

NOINLINE void drawHomes() {
  lcdFillRect(0, 0, LCD_W, CELL, C_DGREEN);
  for (uint8_t k = 0; k < 5; k++) {
    uint8_t x = 16 + k * 32;
    lcdFillRect(x, 0, 8, CELL, C_WATER);
    if (s.homes & (1 << k)) drawFrog(x, 0, C_GREEN);
  }
}

NOINLINE void drawStatus() {   // bottom 8 px strip: lives as tiny frogs
  lcdFillRect(0, 120, LCD_W, 8, C_BLACK);
  for (uint8_t i = 0; i < s.lives; i++) lcdFillRect(2 + i * 8, 121, 6, 6, C_GREEN);
}

void hud() {
  oledClear();
  oledTextCenterP(2, S_TITLE, 1);
  hudScoreBlock(12);
  oledHearts(54, s.lives, 3);
  hudLabel(64, S_HOMES, s.nHome);
  oledTextP(2, 86, S_LVL, 1);
  oledNumberAt(30, 86, s.level + 1, 1);
  hudMessage(s.msg);
}

NOINLINE void pixRow() {
  uint8_t r = s.row;
  if (r == 5) pixAll(255, 200, 0);
  else if ((uint8_t)(r - 1) < 4) pixAll(0, 0, 255);
  else pixAll(0, 255, 0);
}

NOINLINE void spawn() {
  s.fx = START_X;
  s.row = s.maxRow = START_ROW;
  pixRow();
}

NOINLINE void die(const char* m) {
  s.wait = 24;
  s.lives--;
  fxFlash(255, 0, 0, 12);
  sfx(SFX_HIT);
  drawFrog(s.fx, s.row * CELL, C_RED);
  drawStatus();
  s.msg = m;
  hud();
}

void home(uint8_t k) {
  s.homes |= 1 << k;
  score += 50;
  sfx(SFX_COIN);
  fxFlash(255, 255, 255, 8);
  s.msg = S_HOME;
  if (++s.nHome == 5) {
    s.nHome = s.homes = 0;
    if (s.level < 12) s.level++;
    sfx(SFX_LEVELUP);
    fxMode(FX_RAINBOW, 0);
    s.fxT = 20;
  }
  drawHomes();
  spawn();
  hud();
}

void hop() {
  uint8_t b = btnPressed;
  eraseFrog();
  if (b & BTN_UP) {
    if (s.row == 1) {                       // only an empty home slot accepts the frog
      int16_t t = s.fx - 12;                // slots at x = 16 + 32k, tolerance +-4
      uint8_t k = t >> 5;
      if (t >= 0 && (t & 31) <= 8 && !(s.homes & (1 << k))) { home(k); return; }
    } else s.row--;
  }
  else if (b & BTN_DOWN)  { if (s.row < START_ROW) s.row++; }
  else if (b & BTN_LEFT)  s.fx -= 8;
  else if (b & BTN_RIGHT) s.fx += 8;
  if (!isLaneRow(s.row)) {                  // land: stay on screen
    if (s.fx < 0) s.fx = 0;
    if (s.fx > MAX_X) s.fx = MAX_X;
  }
  sfx(SFX_JUMP);
  pixRow();
  s.msg = S_HOP;
  if (s.row < s.maxRow) {
    s.maxRow = s.row;
    score += 10;
    if (s.row == 5) s.msg = S_SAFE;
  }
  hud();
}

void start() {
  s.lives = 3;
  s.msg = S_HOP;
  drawHomes();
  for (uint8_t i = 0; i < LANES; i++) {
    s.phase[i] = rnd(lP(i)) << 2;
    lanePaint(i, 0, LCD_W);
  }
  lcdFillRect(0, 5 * CELL, LCD_W, CELL, C_MEDIAN);
  lcdFillRect(0, START_ROW * CELL, LCD_W, CELL, C_GREY);
  drawStatus();
  spawn();
  hud();
}

void update() {
  if (s.fxT && !--s.fxT) fxMode(FX_HOLD, 0);
  if (s.wait) {                             // everything freezes after a death
    if (--s.wait) return;
    eraseFrog();
    if (!s.lives) { gameOver(); return; }
    spawn();
    return;
  }
  int8_t fd = 0;                            // how far the lane under the frog moved
  for (uint8_t i = 0; i < LANES; i++) {
    int8_t d = laneStep(i);
    if (laneRow(i) == s.row) fd = d;
  }
  uint8_t r = s.row;
  if (isLaneRow(r)) {
    uint8_t i = rowLane(r);
    if (i < 4) {                            // river: ride a log or drown
      if (!onObj(i, s.fx + fd + 4)) { die(S_SPLSH); return; }
      if (fd) { eraseFrog(); s.fx += fd; }
      if (s.fx < 0 || s.fx > MAX_X) { die(S_SPLSH); return; }
    } else if (onObj(i, s.fx + 2) || onObj(i, s.fx + 5)) { die(S_SPLAT); return; }
  }
  if (btnPressed) hop();
  drawFrog(s.fx, s.row * CELL, C_GREEN);
}

const char NAME[] PROGMEM = "Frogger";
const char TAG[]  PROGMEM = "FROG\nCROSS\nROAD\nAND\nRIVER";

} // namespace

const Game gameFrogger PROGMEM = { ID_FROGGER, NAME, TAG, start, update, hud };

#endif
