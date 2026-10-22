/*
  Space Shooter — a Galaga-lite. A 4x3 formation of aliens sweeps across the
  screen and creeps down; shoot them all to clear the wave. Every wave the
  aliens move faster and drop more bombs. LEFT / RIGHT steer the ship,
  UP fires (two shots on screen), DOWN drops the smart bomb (one per wave,
  wipes the screen but scores nothing). 3 lives. Score = 10 x wave per alien.
*/
#include "Games.h"
#if GAME_SHOOTER

namespace {

#define NSTAR   8
#define SHIP_Y  116
#define FX_MAX  90        // formation x 0..90: 4 columns 20 px apart, 10 px wide
#define NSHOT   5         // shots 0-1 = player bullets, 2-4 = alien bombs
#define NOINLINE __attribute__((noinline))

struct State {
  uint8_t  px;                    // ship x
  uint8_t  fx, fy;                // formation top-left
  int8_t   dir;                   // formation direction (+1 / -1)
  uint8_t  alive[12];             // one flag per alien, index = row * 4 + col
  uint8_t  left;                  // aliens still alive
  uint8_t  shx[NSHOT], shy[NSHOT];// shots, shy = 0xFF when free
  uint8_t  sx[NSTAR], sy[NSTAR];  // background stars
  uint8_t  wave, lives, invul, moveT, interval, chance, msg, msgT;
  bool     bombReady, wing;
};
State& s = *(State*)gameRam;
static_assert(sizeof(State) <= GAME_RAM_SIZE, "Shooter state too big");

const uint8_t SHIP[] PROGMEM = {           // 12x8
  0x06, 0x00, 0x06, 0x00, 0x0F, 0x00, 0x3F, 0xC0,
  0x7F, 0xE0, 0xFF, 0xF0, 0xDF, 0xB0, 0x8F, 0x10
};
const uint8_t ALIEN[2][16] PROGMEM = {     // 10x8, two wing frames
  { 0x21, 0x00, 0x12, 0x00, 0x3F, 0x00, 0x6D, 0x80,
    0xFF, 0xC0, 0xBF, 0x40, 0xA1, 0x40, 0x33, 0x00 },
  { 0x21, 0x00, 0x12, 0x00, 0x3F, 0x00, 0x6D, 0x80,
    0xFF, 0xC0, 0x7F, 0x80, 0x40, 0x80, 0x80, 0x40 }
};
// Row colours: row r of wave w uses COLS[r + (w & 3)]
const uint16_t COLS[6] PROGMEM = { C_MAGENTA, C_GREEN, C_ORANGE, C_WHITE, C_YELLOW, C_RED };

const char S_TITLE[] PROGMEM = "SPACE";
const char S_WAVE[]  PROGMEM = "WAVE";
const char S_READY[] PROGMEM = "BOMB\nREADY";
const char S_USED[]  PROGMEM = "BOMB\nUSED";
const char S_OUCH[]  PROGMEM = "OUCH!";
const char S_CLEAR[] PROGMEM = "WAVE\nCLEAR";
const char* const MSGS[4] PROGMEM = { S_READY, S_USED, S_OUCH, S_CLEAR };

NOINLINE uint8_t enemyX(uint8_t i) { return s.fx + (i & 3) * 20; }
NOINLINE uint8_t enemyY(uint8_t i) { return s.fy + (i >> 2) * 12; }
bool isAlive(uint8_t i) { return s.alive[i]; }
NOINLINE void shot(uint8_t x, uint8_t y, uint16_t c) { lcdFillRect(x, y, 2, 4, c); }
// True when a - b is in -off .. span-off-1, i.e. two 1-D spans overlap.
NOINLINE bool overlap(uint8_t a, uint8_t b, uint8_t off, uint8_t span) { return (uint8_t)(a - b + off) < span; }

// Draw sprite i (0-11 = alien, 12 = the ship) moved by (dx, dy) and wipe the
// strip it left behind. dx = dy = 0 just redraws it.
NOINLINE void sprite(uint8_t i, int8_t dx, int8_t dy) {
  uint8_t x, y, w;
  const uint8_t* bits;
  uint16_t c;
  if (i < 12) {
    x = enemyX(i); y = enemyY(i); w = 10; bits = ALIEN[s.wing];
    c = pgm_read_word(&COLS[(i >> 2) + (s.wave & 3)]);
  } else {
    x = s.px; y = SHIP_Y; w = 12; bits = SHIP;
    c = (s.invul & 8) ? C_DGREY : C_CYAN;
  }
  lcdBitmap(x + dx, y + dy, bits, w, 8, c, C_BLACK);
  if (dx > 0)      lcdFillRect(x, y, dx, 8, C_BLACK);
  else if (dx < 0) lcdFillRect(x + w + dx, y, -dx, 8, C_BLACK);
  if (dy > 0)      lcdFillRect(x, y, w, dy, C_BLACK);
}

NOINLINE void moveEnemies(int8_t dx, int8_t dy) {
  for (uint8_t i = 0; i < 12; i++) if (isAlive(i)) sprite(i, dx, dy);
}

void hud() {
  oledClear();
  oledTextCenterP(0, S_TITLE, 1);
  hudScoreBlock(10);
  oledHearts(51, s.lives, 3);
  hudLabel(60, S_WAVE, s.wave);
  for (uint8_t i = 0; i < 12; i++)                       // mini radar of the formation
    if (isAlive(i)) oledFillRect(10 + (i & 3) * 3, 82 + (i >> 2) * 4, 2, 3, true);
  hudMessage((const char*)pgm_read_ptr(&MSGS[s.msg ? s.msg + 1 : !s.bombReady]));
}

void showMsg(uint8_t m) { s.msg = m; s.msgT = 40; }

// Redraw the stage from scratch: wave start and after a lost life.
void layout() {
  lcdClear(C_BLACK);
  s.fx = 10; s.fy = 8; s.dir = 1; s.moveT = 0;
  for (uint8_t k = 0; k < NSHOT; k++) s.shy[k] = 0xFF;
  moveEnemies(0, 0);
  sprite(12, 0, 0);
  hud();
}

void newWave() {
  s.wave++;
  for (uint8_t i = 0; i < 12; i++) s.alive[i] = 1;
  s.left = 12;
  s.bombReady = true;
  s.interval = s.wave >= 3 ? 1 : 4 - s.wave;
  s.chance = s.wave < 5 ? 40 - s.wave * 6 : 10;
  pixAllHue(s.wave * 40);
  layout();
}

void loseLife() {
  fxFlash(255, 0, 0, 12);
  sfx(SFX_HIT);
  if (--s.lives == 0) { gameOver(); return; }
  s.invul = 80;
  showMsg(1);
  layout();
}

NOINLINE void killEnemy(uint8_t i, bool points) {
  lcdFillRect(enemyX(i), enemyY(i), 10, 8, C_BLACK);
  s.alive[i] = 0;
  s.left--;
  if (points) score += 10 * s.wave;
}

NOINLINE void moveFormation() {
  int8_t dx = s.dir, dy = 0;
  if ((uint8_t)(s.fx + s.dir) > FX_MAX) { s.dir = -s.dir; dx = 0; dy = 4; }
  moveEnemies(dx, dy);
  s.fx += dx; s.fy += dy;
  if (dy)
    for (uint8_t i = 0; i < 12; i++)
      if (isAlive(i) && enemyY(i) + 8 >= SHIP_Y) { loseLife(); return; }
}

// Put a shot in a free slot of the range first .. last.
NOINLINE void spawnShot(uint8_t first, uint8_t last, uint8_t x, uint8_t y) {
  for (uint8_t k = first; k <= last; k++)
    if (s.shy[k] == 0xFF) { s.shx[k] = x; s.shy[k] = y; return; }
}

NOINLINE void dropBomb() {
  uint8_t i = 8 + rnd(4);                    // lowest alive alien of a random column
  for (;;) {
    if (isAlive(i)) { spawnShot(2, NSHOT - 1, enemyX(i) + 4, enemyY(i) + 8); return; }
    if (i < 4) return;
    i -= 4;
  }
}

// Move every shot; returns true when a bomb took a life (stage was redrawn).
NOINLINE bool updateShots() {
  for (uint8_t k = 0; k < NSHOT; k++) {
    uint8_t x = s.shx[k], y = s.shy[k];
    if (y == 0xFF) continue;
    shot(x, y, C_BLACK);
    bool bullet = k < 2;
    if (bullet) {
      if (y < 4) { s.shy[k] = 0xFF; continue; }
      y -= 4;
    } else {
      y += 2 + (s.wave >= 4);
      if (y >= 124) { s.shy[k] = 0xFF; continue; }
    }
    s.shy[k] = y;
    if (bullet) {
      for (uint8_t i = 0; i < 12; i++)
        if (isAlive(i) && overlap(x, enemyX(i), 1, 11) && overlap(y, enemyY(i), 3, 11)) {
          killEnemy(i, true);
          sfx(SFX_EXPLODE);
          fxFlash(255, 140, 0, 3);
          hud();
          s.shy[k] = 0xFF;
          break;
        }
      if (s.shy[k] == 0xFF) continue;
    } else if (!s.invul && y > SHIP_Y - 4 && overlap(x, s.px, 1, 13)) {
      loseLife();
      return true;
    }
    shot(x, y, bullet ? C_YELLOW : C_RED);
  }
  return false;
}

NOINLINE void updateStars() {
  if (frameNo & 1) return;
  for (uint8_t k = 0; k < NSTAR; k++) {
    lcdPixel(s.sx[k], s.sy[k], C_BLACK);
    if (++s.sy[k] >= SHIP_Y - 4) { s.sy[k] = 0; s.sx[k] = rnd(LCD_W); }
    lcdPixel(s.sx[k], s.sy[k], C_GREY);
  }
}

void start() {
  s.lives = 3;
  s.px = 74;
  for (uint8_t k = 0; k < NSTAR; k++) { s.sx[k] = rnd(LCD_W); s.sy[k] = rnd(SHIP_Y - 4); }
  newWave();
}

void update() {
  if (s.msgT && !--s.msgT) { s.msg = 0; hud(); }
  updateStars();

  // ---- the ship
  if (s.invul) { s.invul--; if ((s.invul & 7) == 0) sprite(12, 0, 0); }
  int8_t d = 0;
  if ((btnHeld & BTN_LEFT) && s.px >= 2) d = -2;
  else if ((btnHeld & BTN_RIGHT) && s.px <= LCD_W - 14) d = 2;
  if (d) { sprite(12, d, 0); s.px += d; }
  if (btnPressed & BTN_UP) { spawnShot(0, 1, s.px + 5, SHIP_Y - 4); sfx(SFX_SHOOT); }
  if ((btnPressed & BTN_DOWN) && s.bombReady) {      // smart bomb
    s.bombReady = false;
    fxFlash(255, 255, 255, 8);
    sfx(SFX_EXPLODE);
    for (uint8_t i = 0; i < 12; i++) if (isAlive(i)) killEnemy(i, false);
  }

  // ---- shots
  if (updateShots()) return;
  if (!s.left) {
    sfx(SFX_LEVELUP);
    fxFlash(0, 255, 0, 10);
    showMsg(2);
    newWave();
    return;
  }
  if (rnd(s.chance) == 0) dropBomb();

  // ---- the formation
  bool moving = ++s.moveT >= s.interval;
  if (moving) s.moveT = 0;
  if ((frameNo & 15) == 0) { s.wing = !s.wing; if (!moving) moveEnemies(0, 0); }
  if (moving) moveFormation();
}

const char NAME[] PROGMEM = "Space Shooter";
const char TAG[]  PROGMEM = "SPACE\nSHOOT\nTHE\nALIEN\nWAVES";

} // namespace

const Game gameShooter PROGMEM = { ID_SHOOTER, NAME, TAG, start, update, hud };

#endif
