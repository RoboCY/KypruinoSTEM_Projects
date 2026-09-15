/*
  Snake — eat the fruit, grow longer, and don't hit the walls or yourself.
  UP / DOWN / LEFT / RIGHT steer (the turn is queued until the next step, so
  a quick tap always registers). The snake speeds up as it grows.
  Score = fruits eaten.

  The body is stored as a 2-bit "direction to the next segment" grid plus
  the head and tail cells, so 320 cells cost only 80 bytes of RAM.
*/
#include "Games.h"
#if GAME_SNAKE

namespace {

#define GW     20            // grid: 20 x 16 cells of 8 px = 160 x 128
#define GH     16
#define CELLS  (GW * GH)
#define NONE   0xFF
#define C_BODY RGB(0, 170, 0)
#define C_HEAD RGB(150, 255, 150)

struct State {
  uint8_t dirGrid[CELLS / 4];  // 2 bits per cell: direction from this segment to the next one (towards the head)
  uint8_t occ[CELLS / 8];      // 1 bit per cell: part of the snake
  uint8_t hx, hy, tx, ty;      // head and tail cells
  uint8_t dir;                 // heading: 0 = UP, 1 = RIGHT, 2 = DOWN, 3 = LEFT
  uint8_t pend;                // queued turn, NONE = empty
  uint8_t fx, fy, fc;          // fruit cell and colour index
  uint8_t timer;               // frames until the next step
  uint8_t ready;               // "get ready" frames before the snake moves
};
State& s = *(State*)gameRam;
static_assert(sizeof(State) <= GAME_RAM_SIZE, "Snake state too big");

const int8_t DX[4] PROGMEM = { 0, 1, 0, -1 };
const int8_t DY[4] PROGMEM = { -1, 0, 1, 0 };
const uint8_t BTN_OF[4] PROGMEM = { BTN_UP, BTN_RIGHT, BTN_DOWN, BTN_LEFT };
// fruit colours (LCD) and the matching NeoPixel hue
const uint16_t FRUIT_COL[6] PROGMEM = { C_RED, C_ORANGE, C_YELLOW, C_CYAN, RGB(90, 90, 255), C_MAGENTA };
const uint8_t  FRUIT_HUE[6] PROGMEM = { 0, 24, 42, 128, 170, 213 };

const char S_TITLE[] PROGMEM = "SNAKE";
const char S_LEN[]   PROGMEM = "LEN";
const char S_READY[] PROGMEM = "GET\nREADY";
const char S_GO[]    PROGMEM = "GO!";
const char S_YUM[]   PROGMEM = "YUM!";
const char S_GROW[]  PROGMEM = "GROW!";
const char S_BIG[]   PROGMEM = "BIG!";
const char S_HUGE[]  PROGMEM = "HUGE!";

// ---- cell grid helpers (kept out of line: each is called from several places)
#define NOINLINE __attribute__((noinline))
uint16_t idx(uint8_t x, uint8_t y) { return (uint16_t)y * GW + x; }

NOINLINE bool occupied(uint8_t x, uint8_t y) {
  uint16_t i = idx(x, y);
  return s.occ[i >> 3] & (1 << (i & 7));
}

NOINLINE void setDir(uint8_t x, uint8_t y, uint8_t d) {
  uint16_t i = idx(x, y);
  uint8_t sh = (i & 3) << 1;
  s.dirGrid[i >> 2] = (s.dirGrid[i >> 2] & ~(3 << sh)) | (d << sh);
}

uint8_t getDir(uint8_t x, uint8_t y) {
  uint16_t i = idx(x, y);
  return (s.dirGrid[i >> 2] >> ((i & 3) << 1)) & 3;
}

// Move a cell coordinate one step in direction d (out of range wraps past GW/GH).
void step(uint8_t& x, uint8_t& y, uint8_t d) {
  x += pgm_read_byte(&DX[d]);
  y += pgm_read_byte(&DY[d]);
}

// Draw a 7x7 block (1 px gap so the body looks segmented) and mark the cell
// as occupied, or free it when drawn black.
NOINLINE void cell(uint8_t x, uint8_t y, uint16_t c) {
  uint16_t i = idx(x, y);
  uint8_t m = 1 << (i & 7);
  if (c) s.occ[i >> 3] |= m; else s.occ[i >> 3] &= ~m;
  lcdFillRect(x * 8, y * 8, 7, 7, c);
}

void placeFruit() {
  uint8_t x = rnd(GW), y = rnd(GH);
  while (occupied(x, y)) {           // scan forward from a random cell to the first free one
    if (++x == GW) { x = 0; if (++y == GH) y = 0; }
  }
  s.fx = x; s.fy = y;
  s.fc = rnd(6);
  lcdFillRect(x * 8 + 1, y * 8 + 1, 5, 5, pgm_read_word(&FRUIT_COL[s.fc]));
}

void hud() {
  oledClear();
  oledTextCenterP(2, S_TITLE, 1);
  hudScoreBlock(12);
  oledTextCenterP(52, S_LEN, 1);
  uint16_t len = score + 3;          // the snake starts with 3 segments
  oledVBar(11, 61, 10, 33, len > 100 ? 100 : len);
  const char* m;
  if (s.ready)          m = S_READY;
  else if (score == 0)  m = S_GO;
  else if (score < 5)   m = S_YUM;
  else if (score < 10)  m = S_GROW;
  else if (score < 20)  m = S_BIG;
  else                  m = S_HUGE;
  hudMessage(m);
}

void start() {
  s.hx = 10; s.hy = 8;               // three segments in the middle, heading right
  s.tx = 8;  s.ty = 8;
  s.dir = 1;
  s.pend = NONE;
  for (uint8_t x = 8; x <= 10; x++) {
    setDir(x, 8, 1);
    cell(x, 8, x == 10 ? C_HEAD : C_BODY);
  }
  s.timer = 8;
  s.ready = 60;
  pixAllHue(85);
  fxMode(FX_CHASE, 85);
  placeFruit();
}

void update() {
  if (btnPressed) {                  // queue the turn for the next step
    uint8_t d = 0;
    while (!(btnPressed & pgm_read_byte(&BTN_OF[d]))) d++;
    s.pend = d;
  }

  if (s.ready) {                     // ---- get ready countdown
    if (--s.ready == 0) { fxMode(FX_HOLD, 0); hud(); }
    return;
  }

  if (--s.timer) return;
  s.timer = score >= 20 ? 3 : 8 - (score >> 2);   // faster as the snake grows

  if (s.pend != NONE) {              // apply the queued turn (reversing is ignored)
    if (s.pend != ((s.dir + 2) & 3)) s.dir = s.pend;
    s.pend = NONE;
  }

  uint8_t nx = s.hx, ny = s.hy;
  step(nx, ny, s.dir);
  bool eat = nx == s.fx && ny == s.fy;
  if (!eat) {                        // not growing: free the tail cell first
    cell(s.tx, s.ty, C_BLACK);
    step(s.tx, s.ty, getDir(s.tx, s.ty));
  }
  if (nx >= GW || ny >= GH || occupied(nx, ny)) {   // wall or self
    fxFlash(255, 0, 0, 20);
    sfx(SFX_HIT);
    gameOver();
    return;
  }

  setDir(s.hx, s.hy, s.dir);
  cell(s.hx, s.hy, C_BODY);
  s.hx = nx; s.hy = ny;
  cell(nx, ny, C_HEAD);

  if (eat) {
    score++;
    pixAllHue(pgm_read_byte(&FRUIT_HUE[s.fc]));
    fxFlash(255, 255, 255, 3);
    sfx(SFX_COIN);
    if (score + 3 >= CELLS) { gameWon(); return; }
    placeFruit();
    hud();
  }
}

const char NAME[] PROGMEM = "Snake";
const char TAG[]  PROGMEM = "SNAKE\nEAT\nGROW\nDONT\nCRASH";

} // namespace

const Game gameSnake PROGMEM = { ID_SNAKE, NAME, TAG, start, update, hud };

#endif
