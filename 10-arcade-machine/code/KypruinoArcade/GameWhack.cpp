/*
  Whack-a-Mole — moles pop out of four holes laid out like the buttons.
  UP / DOWN / LEFT / RIGHT whack the mole in that hole before it hides.
  Red bombs must be left alone. A wrong button, a hit bomb or a mole that
  escapes costs a life (3 lives). The OLED shows your reaction time in ms.
*/
#include "Games.h"
#if GAME_WHACK

namespace {

struct State {
  uint8_t  lives;
  uint8_t  hole;     // 0..3 = something is up in this hole, 0xFF = none
  bool     bomb;     // the thing that is up is a bomb
  uint8_t  timer;    // frames left: mole visible, or gap before the next pop
  uint8_t  win;      // frames a mole stays up (40 → 12 as the score rises)
  uint8_t  ten;      // hits since the last rainbow
  uint8_t  msg;      // index into MSGS
  uint16_t ms;       // reaction time in ms (counts up while a mole is up)
};
State& s = *(State*)gameRam;
static_assert(sizeof(State) <= GAME_RAM_SIZE, "Whack state too big");

const uint8_t HX[4] PROGMEM = { 62, 62, 14, 110 };   // UP, DOWN, LEFT, RIGHT
const uint8_t HY[4] PROGMEM = { 6, 86, 46, 46 };

#define C_DIRT RGB(90, 50, 15)
#define C_PIT  RGB(40, 20, 0)
#define C_MOLE RGB(170, 110, 50)

const char S_MS[] PROGMEM = "MS";
// one message every 6 bytes, so hudMessage(MSGS + msg * 6) needs no pointer table
const char MSGS[] PROGMEM = "FAST!\0OK\0\0\0\0MISS!\0BOMB!\0GO!\0\0\0";
enum { M_FAST, M_OK, M_MISS, M_BOMB, M_GO };

const char NAME[] PROGMEM = "Whack-a-Mole";
const char TAG[]  PROGMEM = "WHACK\nHIT\nTHE\nMOLES";   // first line doubles as the OLED title

// Filled rect relative to the top-left corner of hole s.hole.
// Byte arguments keep every call site small.
void box(uint8_t dx, uint8_t dy, uint8_t w, uint8_t h, uint16_t c) {
  lcdFillRect(pgm_read_byte(&HX[s.hole]) + dx, pgm_read_byte(&HY[s.hole]) + dy, w, h, c);
}

// Draw the 20x20 blob in the current hole; with c == C_PIT it is erased.
void drawMole(uint16_t c) {
  box(8, 8, 20, 20, c);
  if (c == C_PIT) return;
  box(12, 13, 4, 4, C_BLACK);                      // eyes
  box(20, 13, 4, 4, C_BLACK);
  box(16, 20, 4, 3, s.bomb ? C_YELLOW : RGB(255, 120, 150));  // spark / nose
}

void hud() {
  oledClear();
  oledTextCenterP(2, TAG, 1);
  for (uint8_t i = 0, x = 4; i < 3; i++, x += 9)  // lives: block = alive, thin slot = lost
    oledFillRect(x, 12, 6, i < s.lives ? 6 : 1, true);
  hudScoreBlock(20);
  hudLabel(60, S_MS, s.ms);
  hudMessage(MSGS + s.msg * 6);
}

void hide() {
  drawMole(C_PIT);
  s.hole = 0xFF;
  s.timer = 8 + rnd(16);
}

void fail(uint8_t m) {
  s.msg = m;
  s.lives--;
  fxFlash(255, 0, 0, 16);
  sfx(SFX_HIT);
  hide();
  hud();
  if (!s.lives) gameOver();
}

void start() {
  s.lives = 3;
  s.timer = 30;
  s.win = 40;
  s.msg = M_GO;
  lcdClear(C_DGREEN);                              // grass
  for (s.hole = 0; s.hole < 4; s.hole++) {
    box(0, 0, 36, 36, C_DIRT);
    box(4, 4, 28, 28, C_PIT);
  }
  s.hole = 0xFF;
  pixAll(60, 25, 0);
}

void update() {
  if (s.hole == 0xFF) {                            // ---- waiting for the next pop
    if (--s.timer) return;
    fxMode(FX_HOLD, 0);                            // ends a rainbow
    s.hole = rnd(4);
    s.bomb = rnd(5) == 0;
    s.ms = 0;
    s.timer = s.win;
    drawMole(s.bomb ? C_RED : C_MOLE);
    sfx(s.bomb ? SFX_BOUNCE : SFX_BLIP);
    return;
  }

  s.ms += FRAME_MS;
  if (btnPressed) {                                // ---- a whack
    uint8_t pad = btnPressed & BTN_UP ? 0 : btnPressed & BTN_DOWN ? 1 : btnPressed & BTN_LEFT ? 2 : 3;
    if (pad != s.hole) { fail(M_MISS); return; }
    if (s.bomb)        { fail(M_BOMB); return; }
    score++;
    if (s.win > 12) s.win--;
    s.msg = s.ms < 300 ? M_FAST : M_OK;
    sfx(SFX_COIN);
    fxFlash(0, 255, 0, 8);
    hide();
    if (++s.ten == 10) {                           // every 10th mole: rainbow for 16 frames
      s.ten = 0;
      s.timer = 16;
      fxMode(FX_RAINBOW, 0);
      sfx(SFX_LEVELUP);
    }
    hud();
    return;
  }
  if (--s.timer) return;                           // ---- it went back down
  if (s.bomb) hide(); else fail(M_MISS);
}

} // namespace

const Game gameWhack PROGMEM = { ID_WHACK, NAME, TAG, start, update, hud };

#endif
