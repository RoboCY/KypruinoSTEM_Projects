/*
  Tetris — falling blocks on a 10x16 board. Fill a row to clear it; every
  10 lines the level rises and the pieces fall faster. Clear 4 rows at once
  for a rainbow "tetra". LEFT / RIGHT move (hold to slide), UP rotates,
  DOWN soft-drops. Score = 10 / 30 / 60 / 100 x level for 1 / 2 / 3 / 4 rows.
*/
#include "Games.h"
#if GAME_TETRIS

namespace {

#define BW 10                 // board width in cells
#define BH 16                 // board height in cells
#define BX 40                 // board left edge on the LCD (8 px cells)

struct State {
  uint8_t board[BW * BH];     // colour index per cell (piece + 1), 0 = empty
  uint8_t piece, rot, next;
  int8_t  px, py;             // top-left of the 4x4 piece box in cells
  uint8_t level;
  uint16_t lines;
  uint16_t full;              // bit mask of full rows while they flash
  uint8_t gtimer;             // frames since the last gravity step
  uint8_t timer;              // flash frames left
  uint8_t phase;              // 0 = piece falling, 1 = rows flashing
  uint8_t msg;                // hudMessage index (rows cleared last time)
  uint8_t rainbow;            // frames of rainbow left after a 4-row clear
  bool noDrop;                // DOWN must be released before the next soft drop
};
State& s = *(State*)gameRam;
static_assert(sizeof(State) <= GAME_RAM_SIZE, "Tetris state too big");

// 4x4 piece masks: one nibble per row from the top, MSB = left column.
const uint16_t SHAPES[7][4] PROGMEM = {
  { 0x0F00, 0x2222, 0x0F00, 0x2222 },   // I
  { 0x6600, 0x6600, 0x6600, 0x6600 },   // O
  { 0x4E00, 0x4C40, 0x0E40, 0x4640 },   // T
  { 0x6C00, 0x4620, 0x6C00, 0x4620 },   // S
  { 0xC600, 0x2640, 0xC600, 0x2640 },   // Z
  { 0x8E00, 0x6440, 0x0E20, 0x44C0 },   // J
  { 0x2E00, 0x4460, 0x0E80, 0xC440 },   // L
};
const uint16_t COLOUR[9] PROGMEM = {
  C_BLACK, C_CYAN, C_YELLOW, C_MAGENTA, C_GREEN, C_RED, C_BLUE, C_ORANGE, C_WHITE
};
const uint8_t HUE[7]      PROGMEM = { 128, 42, 213, 85, 0, 170, 21 };
const uint8_t GRAVITY[10] PROGMEM = { 20, 17, 14, 12, 10, 8, 6, 5, 4, 3 };
const uint8_t POINTS[4]   PROGMEM = { 10, 30, 60, 100 };

const char S_TITLE[] PROGMEM = "TETRS";
const char S_LEVEL[] PROGMEM = "LEVEL";
const char S_LINES[] PROGMEM = "LINES";
const char S_NEXT[]  PROGMEM = "NEXT";
const char S_LVL[]   PROGMEM = "LVL";
const char M0[] PROGMEM = "GO!";
const char M1[] PROGMEM = "GOOD";
const char M2[] PROGMEM = "GREAT";
const char M3[] PROGMEM = "WOW";
const char M4[] PROGMEM = "TETRA\n!!!";
const char* const MSG[5] PROGMEM = { M0, M1, M2, M3, M4 };

// Korobeiniki, 8 bars; one tick = an eighth note at 120 ms.
const uint8_t MUSIC[] PROGMEM = {
  N_E5, 2, N_B4, 1, N_C5, 1, N_D5, 2, N_C5, 1, N_B4, 1,
  N_A4, 2, N_A4, 1, N_C5, 1, N_E5, 2, N_D5, 1, N_C5, 1,
  N_B4, 3, N_C5, 1, N_D5, 2, N_E5, 2,
  N_C5, 2, N_A4, 2, N_A4, 2, REST, 2,
  REST, 1, N_D5, 2, N_F5, 1, N_A5, 2, N_G5, 1, N_F5, 1,
  N_E5, 3, N_C5, 1, N_E5, 2, N_D5, 1, N_C5, 1,
  N_B4, 2, N_B4, 1, N_C5, 1, N_D5, 2, N_E5, 2,
  N_C5, 2, N_A4, 2, N_A4, 2, REST, 2,
  END
};

// ---------------- drawing ----------------

void drawCell(int8_t x, int8_t y, uint8_t c) {
  lcdFillRect(BX + x * 8, y * 8, 7, 7, pgm_read_word(&COLOUR[c]));
}

void drawRow(uint8_t y) {
  for (uint8_t x = 0; x < BW; x++) drawCell(x, y, s.board[y * BW + x]);
}

uint16_t shape(uint8_t rot) { return pgm_read_word(&SHAPES[s.piece][rot]); }

void drawPiece(uint8_t c) {
  uint16_t m = shape(s.rot);
  for (uint8_t i = 0; i < 16; i++, m <<= 1)
    if (m & 0x8000) drawCell(s.px + (i & 3), s.py + (i >> 2), c);
}

void drawNext() {
  lcdFillRect(11, 18, 16, 16, C_BLACK);
  uint16_t m = pgm_read_word(&SHAPES[s.next][0]);
  uint16_t col = pgm_read_word(&COLOUR[s.next + 1]);
  for (uint8_t i = 0; i < 16; i++, m <<= 1)
    if (m & 0x8000) lcdFillRect(11 + (i & 3) * 4, 18 + (i >> 2) * 4, 3, 3, col);
}

void drawPanel() {
  lcdNumber(156, 16, s.level, C_WHITE, C_BLACK, 1);
  lcdNumber(156, 42, s.lines, C_WHITE, C_BLACK, 1);
}

void hud() {
  oledClear();
  oledTextCenterP(2, S_TITLE, 1);
  hudScoreBlock(12);
  hudLabel(52, S_LEVEL, s.level);
  hudLabel(72, S_LINES, s.lines);
  hudMessage((const char*)pgm_read_ptr(&MSG[s.msg]));
}

// ---------------- rules ----------------

bool fits(uint8_t rot, int8_t px, int8_t py) {
  uint16_t m = shape(rot);
  for (uint8_t i = 0; i < 16; i++, m <<= 1) {
    if (!(m & 0x8000)) continue;
    int8_t x = px + (i & 3), y = py + (i >> 2);
    if (x < 0 || x >= BW || y >= BH || s.board[y * BW + x]) return false;
  }
  return true;
}

uint8_t gravity() {
  return pgm_read_byte(&GRAVITY[s.level > 10 ? 9 : s.level - 1]);
}

void spawn() {
  s.piece = s.next;
  s.next = rnd(7);
  s.rot = 0;
  s.px = 3;
  s.py = 0;
  s.gtimer = 0;
  s.phase = 0;
  s.noDrop = true;
  drawNext();
  pixAllHue(pgm_read_byte(&HUE[s.piece]));
  if (!fits(0, 3, 0)) { sfx(SFX_HIT); gameOver(); }
  drawPiece(s.piece + 1);
}

// Draw the full rows white, or back in their own colours.
void flashRows(bool white) {
  uint16_t bit = 1;
  for (uint8_t y = 0; y < BH; y++, bit <<= 1) {
    if (!(s.full & bit)) continue;
    if (white) for (uint8_t x = 0; x < BW; x++) drawCell(x, y, 8);
    else drawRow(y);
  }
}

void lock() {
  uint16_t m = shape(s.rot);
  for (uint8_t i = 0; i < 16; i++, m <<= 1)
    if (m & 0x8000) s.board[(s.py + (i >> 2)) * BW + s.px + (i & 3)] = s.piece + 1;
  sfx(SFX_BLIP);
  s.full = 0;
  for (uint8_t y = 0; y < BH; y++) {
    uint8_t x = 0;
    while (x < BW && s.board[y * BW + x]) x++;
    if (x == BW) s.full |= (uint16_t)1 << y;
  }
  if (!s.full) { spawn(); return; }
  s.phase = 1;
  s.timer = 10;
  flashRows(true);
  fxFlash(255, 255, 255, 6);
}

void collapse() {
  uint8_t n = 0;
  int8_t dst = BH - 1;
  uint16_t bit = (uint16_t)1 << (BH - 1);
  for (int8_t src = BH - 1; src >= 0; src--, bit >>= 1) {
    if (s.full & bit) { n++; continue; }
    if (dst != src) {
      memcpy(s.board + dst * BW, s.board + src * BW, BW);
      drawRow(dst);
    }
    dst--;
  }
  for (; dst >= 0; dst--) {
    memset(s.board + dst * BW, 0, BW);
    drawRow(dst);
  }
  score += (uint16_t)pgm_read_byte(&POINTS[n - 1]) * s.level;
  s.lines += n;
  s.msg = n;
  uint8_t lv = s.lines / 10 + 1;
  if (lv > s.level) { s.level = lv; sfx(SFX_LEVELUP); }
  if (n == 4) { fxMode(FX_RAINBOW, 0); s.rainbow = 20; }
  drawPanel();
  hud();
}

void start() {
  s.level = 1;
  s.next = rnd(7);
  lcdFillRect(BX - 1, 0, 1, LCD_H, C_GREY);
  lcdFillRect(BX + BW * 8, 0, 1, LCD_H, C_GREY);
  lcdFillRect(BX, 0, BW * 8, LCD_H, RGB(24, 24, 24));   // faint grid in the cell gaps
  for (uint8_t y = 0; y < BH; y++) drawRow(y);
  lcdTextP(7, 4, S_NEXT, C_GREY, C_BLACK, 1);
  lcdTextP(138, 4, S_LVL, C_GREY, C_BLACK, 1);
  lcdTextP(126, 30, S_LINES, C_GREY, C_BLACK, 1);
  drawPanel();
  spawn();
  soundPlay(MUSIC, 120, true);
}

void update() {
  if (s.rainbow && --s.rainbow == 0) fxMode(FX_HOLD, 0);
  if (s.phase) {                             // rows blink white / colour / white
    s.timer--;
    if (s.timer == 6 || s.timer == 2) flashRows(s.timer == 2);
    if (s.timer == 0) { collapse(); spawn(); }
    return;
  }
  if (!(btnHeld & BTN_DOWN)) s.noDrop = false;

  int8_t nx = s.px, ny = s.py;
  uint8_t nr = s.rot;
  if (btnRepeat & BTN_LEFT)  nx--;
  if (btnRepeat & BTN_RIGHT) nx++;
  if (nx != s.px && !fits(nr, nx, ny)) nx = s.px;
  if (btnPressed & BTN_UP) {                 // rotate, with a +-1 wall kick
    uint8_t r2 = (nr + 1) & 3;
    if (fits(r2, nx, ny)) nr = r2;
    else if (fits(r2, nx - 1, ny)) { nr = r2; nx--; }
    else if (fits(r2, nx + 1, ny)) { nr = r2; nx++; }
  }
  bool fall = (btnHeld & BTN_DOWN) && !s.noDrop && !(frameNo & 1);
  if (++s.gtimer >= gravity()) { s.gtimer = 0; fall = true; }
  bool landed = false;
  if (fall) {
    if (fits(nr, nx, ny + 1)) ny++;
    else landed = true;
  }
  if (nx != s.px || ny != s.py || nr != s.rot) {
    drawPiece(0);
    s.px = nx; s.py = ny; s.rot = nr;
    drawPiece(s.piece + 1);
  }
  if (landed) lock();
}

const char NAME[] PROGMEM = "Tetris";
const char TAG[]  PROGMEM = "TETRS\nSTACK\nTHE\nBLOKS\nCLEAR\nLINES";

} // namespace

const Game gameTetris PROGMEM = { ID_TETRIS, NAME, TAG, start, update, hud };

#endif
