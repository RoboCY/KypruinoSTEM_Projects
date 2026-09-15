/*
  Breakout — smash the 50 coloured bricks with a bouncing ball.
  LEFT / RIGHT move the paddle, UP launches the ball from the paddle.
  Where the ball hits the paddle sets its angle. Top rows score more,
  each cleared wall speeds the ball up. 3 lives.
*/
#include "Games.h"
#if GAME_BREAKOUT

namespace {

#define TOP    12      // first brick row (px)
#define ROWS   5
#define COLS   10
#define PAD_Y  122
#define PAD_W  24
#define BALL_Y (PAD_Y - 4)

struct State {
  uint8_t brick[ROWS * COLS];  // 1 = alive
  uint8_t left;                // bricks still standing
  uint8_t px;                  // paddle left edge
  int16_t bx, by;              // ball position, 4 fraction bits
  int16_t vx, vy;              // ball velocity, 4 fraction bits
  uint8_t ox, oy;              // where the ball was last drawn
  uint8_t spd;                 // ball speed (1/16 px per frame)
  uint8_t zx;                  // horizontal speed per paddle zone step (3/8 of spd)
  uint8_t lives, level;
  bool stuck;                  // ball rides on the paddle until UP
  uint8_t hue[3];              // colours of the last 3 bricks hit
  const char* msg;             // OLED message (PROGMEM)
};
State& s = *(State*)gameRam;
static_assert(sizeof(State) <= GAME_RAM_SIZE, "Breakout state too big");

const uint16_t COL[ROWS] PROGMEM = { C_RED, C_ORANGE, C_YELLOW, C_GREEN, C_BLUE };
const uint8_t  HUE[ROWS] PROGMEM = { 0, 18, 42, 85, 170 };

const char S_TITLE[] PROGMEM = "BRICK";
const char S_LEVEL[] PROGMEM = "LEVEL";
const char S_GO[]    PROGMEM = "GO!";
const char S_NICE[]  PROGMEM = "NICE";
const char S_OUCH[]  PROGMEM = "OUCH!";
const char S_CLEAR[] PROGMEM = "CLEAR";

void hud() {
  oledClear();
  oledTextCenterP(2, S_TITLE, 1);
  oledHearts(11, s.lives, 3);
  hudScoreBlock(20);
  hudLabel(60, S_LEVEL, s.level);
  hudMessage(s.msg);
}

void say(const char* m) { s.msg = m; hud(); }

void paint(uint8_t r, uint8_t c, uint16_t col) {
  lcdFillRect(c << 4, TOP + (r << 3), 15, 7, col);
}
__attribute__((noinline)) void drawBrick(uint8_t r, uint8_t c) { paint(r, c, pgm_read_word(&COL[r])); }


__attribute__((noinline)) void paddle(uint16_t col) { lcdFillRect(s.px, PAD_Y, PAD_W, 4, col); }
__attribute__((noinline)) void ball(uint8_t x, uint8_t y, uint16_t col) { lcdFillRect(x, y, 4, 4, col); }

// Brick cell under pixel (x, y); false when outside the wall area.
bool cell(uint8_t x, uint8_t y, uint8_t& r, uint8_t& c) {
  uint8_t yy = y - TOP;
  if (yy >= ROWS * 8) return false;
  r = yy >> 3;
  c = x >> 4;
  return true;
}

void stick() {
  s.stuck = true;
  s.bx = (s.px + PAD_W / 2 - 2) << 4;
  s.by = BALL_Y << 4;
}

// Send the ball up from paddle zone z (0 = left edge .. 4 = right edge)
void bounce(uint8_t z) {
  int8_t f = (int8_t)z - 2;                 // -2 .. 2
  int16_t vx = f * (int16_t)s.zx;
  int16_t m = vx < 0 ? -vx : vx;
  if (!f) vx = s.vx < 0 ? -3 : 3;           // centre: nearly straight up
  s.vx = vx;
  s.vy = -(s.spd - (m >> 1));               // roughly keeps the speed constant
  sfx(SFX_BOUNCE);
}

// Kill the brick under (x, y) if there is one
bool hitBrick(uint8_t x, uint8_t y) {
  uint8_t r, c;
  if (!cell(x, y, r, c)) return false;
  uint8_t i = r * COLS + c;
  if (!s.brick[i]) return false;
  s.brick[i] = 0;
  s.left--;
  paint(r, c, C_BLACK);
  score += ROWS - r;
  sfx(SFX_BLIP);
  s.hue[2] = s.hue[1];
  s.hue[1] = s.hue[0];
  s.hue[0] = pgm_read_byte(&HUE[r]);
  for (i = 0; i < 3; i++) pixHue(i, s.hue[i]);
  say(S_NICE);
  return true;
}

__attribute__((noinline)) void physics() {
  int16_t nx = s.bx + s.vx, ny = s.by + s.vy;
  if ((uint16_t)nx > (LCD_W - 4) << 4) { nx = nx < 0 ? 0 : (LCD_W - 4) << 4; s.vx = -s.vx; }
  if (ny < 0) { ny = 0; s.vy = -s.vy; }
  uint8_t x = nx >> 4, y = ny >> 4;
  // Every brick the ball overlaps is smashed, so the ball never covers a live one
  uint8_t ly = s.vy < 0 ? y : y + 3, lx = s.vx < 0 ? x : x + 3;      // leading edges
  if (hitBrick(x, ly) | hitBrick(x + 3, ly)) s.vy = -s.vy;            // top / bottom
  else if (hitBrick(lx, y + 3 - (ly - y))) s.vx = -s.vx;             // side
  uint8_t d = x + 3 - s.px;                 // 0 .. 26 while over the paddle
  if (s.vy > 0 && (uint8_t)(y - BALL_Y) < 4 && d < PAD_W + 3) {
    bounce((uint8_t)(d * 3) >> 4);          // 5 zones
  } else if (y >= PAD_Y + 2) {              // lost the ball
    fxFlash(255, 0, 0, 20);
    sfx(SFX_HIT);
    if (!--s.lives) { gameOver(); return; }
    stick();
    say(S_OUCH);
    return;
  }
  s.bx = nx;
  s.by = ny;
}

void moveBall() {
  uint8_t x = s.bx >> 4, y = s.by >> 4;
  if (x == s.ox && y == s.oy) return;
  ball(s.ox, s.oy, C_BLACK);
  s.ox = x;
  s.oy = y;
  ball(x, y, C_WHITE);
}

void newLevel() {
  s.left = ROWS * COLS;
  for (uint8_t r = 0; r < ROWS; r++)
    for (uint8_t c = 0; c < COLS; c++) { s.brick[r * COLS + c] = 1; drawBrick(r, c); }
  paddle(C_CYAN);
  stick();
}

void start() {
  s.lives = 3;
  s.level = 1;
  s.spd = 32;
  s.zx = 12;
  s.px = (LCD_W - PAD_W) / 2;
  s.msg = S_GO;
  newLevel();
  moveBall();
}

void update() {
  int8_t d = 0;
  if (btnHeld & BTN_LEFT)  d = -3;
  if (btnHeld & BTN_RIGHT) d = 3;
  if (d) {
    uint8_t np = s.px + d;                  // wraps when going below 0
    if (np > LCD_W - PAD_W) np = d < 0 ? 0 : LCD_W - PAD_W;
    paddle(C_BLACK);
    s.px = np;
    paddle(C_CYAN);
  }
  if (s.stuck) {
    stick();
    if (btnPressed & BTN_UP) {
      s.stuck = false;
      bounce(frameNo & 1 ? 1 : 3);
      say(S_GO);
    }
  } else {
    physics();
  }
  if (!s.left) {                              // wall cleared
    sfx(SFX_LEVELUP);
    s.level++;
    if (s.spd < 56) { s.spd += 8; s.zx += 3; }
    lcdClear(C_BLACK);
    s.ox = s.oy = 0;                          // old ball image is gone
    newLevel();
    say(S_CLEAR);
  }
  moveBall();
}

const char NAME[] PROGMEM = "Breakout";
const char TAG[]  PROGMEM = "BRICK\nBREAK\nBOUNC\nSMASH";

} // namespace

const Game gameBreakout PROGMEM = { ID_BREAKOUT, NAME, TAG, start, update, hud };

#endif
