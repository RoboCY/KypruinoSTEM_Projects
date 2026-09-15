/*
  2048 — slide the tiles with UP / DOWN / LEFT / RIGHT. Tiles with the same
  number merge into one when they touch. Reach the 2048 tile to win; when the
  board is full and nothing can merge, it is game over.
  Score = sum of the merged tile values (classic scoring).
*/
#include "Games.h"
#if GAME_2048

namespace {

struct State {
  uint8_t b[16];       // board as exponents: 0 = empty, 1 = 2, 2 = 4 ... 11 = 2048
  uint8_t prev[16];    // what is currently drawn on the LCD
  uint8_t big;         // biggest merge of the last move (exponent, 0 = none)
  uint8_t maxExp;      // highest tile on the board
  uint8_t msg;         // reaction shown on the OLED
};
State& s = *(State*)gameRam;
static_assert(sizeof(State) <= GAME_RAM_SIZE, "2048 state too big");

// Per direction: first cell, step to the next line, stride along a line.
// UP, DOWN, LEFT, RIGHT
const int8_t DIR[4][3] PROGMEM = { { 0, 1, 4 }, { 12, 1, -4 }, { 0, 4, 1 }, { 3, 4, -1 } };

const char S_TITLE[] PROGMEM = "2048";
const char S_MAX[]   PROGMEM = "MAX";
const char S_MSG[]   PROGMEM = "SLIDE\0MERGE\0BIG!\0\0WOW!!";   // 6 bytes per message

void hud() {
  oledClear();
  oledTextCenterP(2, S_TITLE, 1);
  hudScoreBlock(14);
  hudLabel(56, S_MAX, s.maxExp ? 1 << s.maxExp : 0);
  hudMessage(S_MSG + s.msg * 6);
}

void drawTile(uint8_t i) {
  uint8_t v = s.b[i];
  int16_t x = 21 + (i & 3) * 30, y = 5 + (i >> 2) * 30;
  uint16_t c = v ? lcdHsv(v * 22, 255, 255) : C_DGREY;
  lcdFillRect(x, y, 28, 28, c);
  if (!v) return;
  uint8_t digits = (v + 2) / 3;            // 1..4 digits
  uint8_t sz = digits <= 2 ? 2 : 1;
  lcdNumber(x + 14 + digits * 3 * sz, y + 14 - 4 * sz, 1 << v, C_BLACK, c, sz);
}

// Redraw the cells that changed, refresh the highest tile and the pixels.
void redraw(bool all) {
  s.maxExp = 0;
  for (uint8_t i = 0; i < 16; i++) {
    if (all || s.b[i] != s.prev[i]) { drawTile(i); s.prev[i] = s.b[i]; }
    if (s.b[i] > s.maxExp) s.maxExp = s.b[i];
  }
  pixAllHue(s.maxExp * 22);
}

void spawn() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < 16; i++) if (!s.b[i]) n++;
  if (!n) return;
  uint8_t k = rnd(n);
  for (uint8_t i = 0; i < 16; i++)
    if (!s.b[i] && !k--) { s.b[i] = rnd(10) ? 1 : 2; return; }
}

// Slide one line of 4 cells towards its first cell; true if anything moved.
bool slideLine(int8_t base, int8_t stride) {
  uint8_t line[4] = { 0, 0, 0, 0 };
  uint8_t n = 0;
  bool merged = false, moved = false;
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t v = s.b[base + i * stride];
    if (!v) continue;
    if (n && !merged && line[n - 1] == v) {
      line[n - 1] = ++v;
      merged = true;
      uint16_t add = 1 << v;
      score = score > 0xFFFF - add ? 0xFFFF : score + add;
      if (v > s.big) s.big = v;
    } else {
      line[n++] = v;
      merged = false;
    }
  }
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t& cell = s.b[base + i * stride];
    if (cell != line[i]) { cell = line[i]; moved = true; }
  }
  return moved;
}

bool stuck() {
  for (uint8_t i = 0; i < 16; i++) {
    uint8_t v = s.b[i];
    if (!v) return false;
    if ((i & 3) != 3 && v == s.b[i + 1]) return false;
    if (i < 12 && v == s.b[i + 4]) return false;
  }
  return true;
}

void start() {
  lcdFillRect(20, 4, 120, 120, C_GREY);
  spawn();
  spawn();
  redraw(true);
}

void update() {
  if (!(btnPressed & BTN_ANY)) return;
  uint8_t d = btnPressed & BTN_UP ? 0 : btnPressed & BTN_DOWN ? 1 : btnPressed & BTN_LEFT ? 2 : 3;
  const int8_t* p = DIR[d];
  int8_t base = pgm_read_byte(p), step = pgm_read_byte(p + 1), stride = pgm_read_byte(p + 2);
  s.big = 0;
  bool moved = false;
  for (uint8_t l = 0; l < 4; l++)
    if (slideLine(base + l * step, stride)) moved = true;
  if (!moved) return;

  spawn();
  redraw(false);
  uint8_t big = s.big;
  if (big) {
    sfx(SFX_COIN);
    if (big >= 7) fxFlash(255, 255, 255, 8);
  } else {
    sfx(SFX_BLIP);
  }
  s.msg = big == 0 ? 0 : big < 7 ? 1 : big < 10 ? 2 : 3;
  hud();
  if (s.maxExp >= 11) { sfx(SFX_WIN); gameWon(); return; }
  if (stuck()) gameOver();
}

const char NAME[] PROGMEM = "2048";
const char TAG[]  PROGMEM = "2048\nSLIDE\nAND\nMERGE";

} // namespace

const Game game2048 PROGMEM = { ID_2048, NAME, TAG, start, update, hud };

#endif
