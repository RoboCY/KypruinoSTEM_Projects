/*
  Kypruino Pet — a little virtual pet that lives in the EEPROM. Keep its four
  stats (FOOD, FUN, ZZZ, CLEAN) up; if any stat stays at zero for a whole
  minute the pet runs away. Score = age in minutes, so "best" = longest life.
  UP = feed, LEFT = play (press LEFT again when the pixels turn green),
  RIGHT = clean, DOWN = sleep / wake up. The pet is saved automatically.
*/
#include "Games.h"
#if GAME_PET

namespace {

struct State {
  uint8_t stat[4];     // FOOD, FUN, ZZZ, CLEAN 0..100
  uint16_t age;        // minutes
  uint8_t sleeping;
  uint8_t dec[4];      // seconds until the next decay of each stat
  uint8_t frame;       // 0..39 inside the current second
  uint8_t secs;        // 0..59 inside the current minute
  uint8_t zeroSec;     // seconds in a row with a stat at 0
  uint8_t mood;        // 0 happy, 1 meh, 2 sad, 3 asleep (as drawn)
  bool poop;           // poop icon is on the LCD
  uint8_t anim;        // frames left of the eating animation
  uint8_t blink;       // frames left with the eyes closed
  uint8_t play;        // 0 idle, 1 waiting for green, 2 green
  uint8_t playT;       // frames until green / frames since green
  uint8_t msg;         // temporary OLED message index
  uint8_t msgT;        // frames left of the temporary message
};
State& s = *(State*)gameRam;
static_assert(sizeof(State) <= GAME_RAM_SIZE, "Pet state too big");
static_assert(offsetof(State, sleeping) == 6, "save layout");

#define MAGIC   0x5E
#define PX      44             // pet position on the LCD (72x72, centred)
#define PY      28
#define C_DARK  RGB(0, 0, 32)  // background while asleep

// 24x24 sprite: rows 0-7 and 18-23 are shared, rows 8-17 come from a face
const uint8_t BODY[] PROGMEM = {
  0x00,0xFF,0x00, 0x03,0xFF,0xC0, 0x07,0xFF,0xE0, 0x0F,0xFF,0xF0,
  0x1F,0xFF,0xF8, 0x3F,0xFF,0xFC, 0x3F,0xFF,0xFC, 0x7F,0xFF,0xFE,
  0x3F,0xFF,0xFC, 0x1F,0xFF,0xF8, 0x0F,0xFF,0xF0, 0x07,0xE7,0xE0,
  0x03,0xC3,0xC0, 0x03,0xC3,0xC0,
};
const uint8_t FACE[] PROGMEM = {
  // happy
  0x7C,0xFF,0x3E, 0x78,0x7E,0x1E, 0x78,0x7E,0x1E, 0x7C,0xFF,0x3E, 0x7F,0xFF,0xFE,
  0x7F,0xFF,0xFE, 0x7D,0xFF,0xBE, 0x3E,0xFF,0x7C, 0x3F,0x00,0xFC, 0x3F,0xFF,0xFC,
  // sad
  0x7C,0xFF,0x3E, 0x78,0x7E,0x1E, 0x78,0x7E,0x1E, 0x7C,0xFF,0x3E, 0x7F,0xFF,0xFE,
  0x7F,0xFF,0xFE, 0x7F,0xFF,0xFE, 0x3F,0x00,0xFC, 0x3E,0xFF,0x7C, 0x3F,0xFF,0xFC,
  // asleep (eyes closed)
  0x7F,0xFF,0xFE, 0x7F,0xFF,0xFE, 0x78,0x7E,0x1E, 0x70,0x3C,0x0E, 0x7F,0xFF,0xFE,
  0x7F,0xFF,0xFE, 0x7F,0xFF,0xFE, 0x3F,0xE1,0xFC, 0x3F,0xFF,0xFC, 0x3F,0xFF,0xFC,
};

const uint8_t  HUE[4] PROGMEM = { 85, 36, 0, 165 };   // green, yellow, red, blue
const uint8_t  PERIOD[4] PROGMEM = { 6, 8, 12, 15 };   // seconds per decay point
const char LBL[] PROGMEM = "FPZC";
const char MSG[] PROGMEM =                             // 6 bytes per entry
  "HAPPY\0MEH\0\0\0SAD\0\0\0ZZZ\0\0\0HUNGY\0DIRTY\0WAIT\0\0GO!\0\0\0NICE!\0MISS\0\0YUM!\0\0FRESH";
enum { M_HAPPY, M_MEH, M_SAD, M_ZZZ, M_HUNGY, M_DIRTY, M_WAIT, M_GO, M_NICE, M_MISS, M_YUM, M_FRESH };
const char S_TITLE[] PROGMEM = "PET";
const char S_AGE[]   PROGMEM = "AGE";
const char S_HINT[]  PROGMEM = "U:FEED L:PLAY R:WASH D:ZZZ";
const char S_ZZZ[]   PROGMEM = "Zzz";
const char S_GREEN[] PROGMEM = "LEFT WHEN GREEN!";

uint16_t bgCol() { return s.sleeping ? C_DARK : C_BLACK; }
uint8_t faceOf(uint8_t mood) { return mood < 2 ? 0 : mood - 1; }

uint8_t moodCalc() {
  if (s.sleeping) return 3;
  uint16_t sum = 0;
  for (uint8_t i = 0; i < 4; i++) sum += s.stat[i];
  return sum >= 280 ? 0 : sum >= 160 ? 1 : 2;
}

__attribute__((noinline)) const uint8_t* rowPtr(uint8_t face, uint8_t r) {
  if (r >= 18) r -= 10;
  else if (r >= 8) return FACE + face * 30 + (r - 8) * 3;
  return BODY + r * 3;
}

// Draw sprite rows r0..r1-1 at 3x scale (one 3x3 block per sprite pixel).
void drawRows(uint8_t face, uint8_t r0, uint8_t r1) {
  uint16_t fg = lcdHsv(pgm_read_byte(&HUE[s.mood]), 255, 255), bg = bgCol();
  for (uint8_t r = r0; r < r1; r++) {
    const uint8_t* p = rowPtr(face, r);
    uint8_t b = 0, y = PY + r * 3;
    for (uint8_t x = PX; x < PX + 72; x += 3) {
      if (!((x - PX) & 7)) b = pgm_read_byte(p++);
      lcdFillRect(x, y, 3, 3, (b & 0x80) ? fg : bg);
      b <<= 1;
    }
  }
}

void drawPet() { drawRows(faceOf(s.mood), 0, 24); }

void setPix() {
  if (s.sleeping) pixAll(0, 0, 40);
  else pixAllHue(pgm_read_byte(&HUE[s.mood]));
}

void drawPoop(bool on) {
  s.poop = on;
  uint16_t bg = bgCol();
  lcdFillRect(122, 96, 16, 14, bg);
  if (!on) return;
  lcdFillRect(122, 103, 16, 7, C_BROWN);
  lcdFillRect(126, 97, 8, 6, C_BROWN);
}

void hud() {
  oledClear();
  oledTextCenterP(2, S_TITLE, 1);
  for (uint8_t i = 0; i < 4; i++) {
    oledVBar(1 + i * 8, 12, 6, 40, s.stat[i]);
    oledChar(1 + i * 8, 54, pgm_read_byte(&LBL[i]), 1);
  }
  hudLabel(66, S_AGE, s.age);
  uint8_t m;
  if (s.msgT) m = s.msg;
  else if (s.play) m = s.play == 1 ? M_WAIT : M_GO;
  else if (s.sleeping) m = M_ZZZ;
  else if (s.stat[0] < 30) m = M_HUNGY;
  else if (s.stat[3] < 30) m = M_DIRTY;
  else m = s.mood;
  hudMessage(MSG + m * 6);
}

__attribute__((noinline)) void say(uint8_t m) { s.msg = m; s.msgT = 60; }

// Redraw the pet / pixels if the mood changed, the poop icon, and the OLED.
void refresh() {
  uint8_t m = moodCalc();
  if (m != s.mood) { s.mood = m; drawPet(); setPix(); }
  bool dirty = s.stat[3] < 30;
  if (dirty != s.poop) drawPoop(dirty);
  hud();
}

void redrawAll() {
  s.mood = moodCalc();
  lcdClear(bgCol());
  drawPet();
  lcdTextCenterP(118, S_HINT, C_GREY, bgCol(), 1);
  if (s.sleeping) lcdTextP(120, 30, S_ZZZ, C_BLUE, C_DARK, 2);
  s.poop = false;
  setPix();
  refresh();
}

// EEPROM: magic byte, then stat[4], age, sleeping (7 bytes straight from State)
#define SAVE_LEN 7
void save() {
  eeprom_update_byte(EEPROM_PET_ADDR, MAGIC);
  eeprom_update_block(s.stat, EEPROM_PET_ADDR + 1, SAVE_LEN);
}

__attribute__((noinline)) void add(uint8_t i, int8_t d) {
  int16_t v = s.stat[i] + d;
  s.stat[i] = v < 0 ? 0 : v > 100 ? 100 : v;
}

void toggleSleep() {
  s.sleeping ^= 1;
  redrawAll();
  save();
}

void playResult(bool win) {
  add(1, win ? 25 : 5);
  sfx(win ? SFX_WIN : SFX_HIT);
  say(win ? M_NICE : M_MISS);
  s.play = 0;
  lcdFillRect(0, 4, LCD_W, 8, C_BLACK);
  setPix();
  refresh();
  save();
}

__attribute__((noinline)) void everySecond() {
  bool zero = false;
  for (uint8_t i = 0; i < 4; i++) {
    if (--s.dec[i] == 0) {
      uint8_t p = pgm_read_byte(&PERIOD[i]);
      s.dec[i] = (s.sleeping && i < 2) ? p * 2 : p;      // FOOD/FUN decay slower asleep
      if (!(s.sleeping && i == 2)) add(i, -1);
    }
    if (s.stat[i] == 0) zero = true;
  }
  if (s.sleeping && (s.secs & 1)) {
    add(2, 1);
    if (s.stat[2] == 100) { toggleSleep(); return; }  // fully rested: wake up
  }
  s.zeroSec = zero ? s.zeroSec + 1 : 0;
  if (s.zeroSec >= 60) {                              // the pet runs away
    eeprom_update_byte(EEPROM_PET_ADDR, 0);
    sfx(SFX_LOSE);
    gameOver();
    return;
  }
  if (++s.secs == 60) {
    s.secs = 0;
    s.age++;
    score = s.age;
    save();
  }
  refresh();
}

void start() {
  bool saved = eeprom_read_byte(EEPROM_PET_ADDR) == MAGIC;
  if (saved) eeprom_read_block(s.stat, EEPROM_PET_ADDR + 1, SAVE_LEN);
  for (uint8_t i = 0; i < 4; i++) {
    if (!saved) s.stat[i] = 80;
    s.dec[i] = pgm_read_byte(&PERIOD[i]);
  }
  score = s.age;
  redrawAll();
  if (!saved) save();
}

void update() {
  if (++s.frame == 40) { s.frame = 0; everySecond(); }
  if (s.msgT && --s.msgT == 0) hud();

  // eating animation: a shrinking orange snack next to the mouth
  if (s.anim) {
    s.anim--;
    if ((s.anim & 7) == 0) {
      lcdFillRect(22, 62, 12, 12, bgCol());
      if (s.anim) lcdFillRect(22, 62, s.anim >> 1, s.anim >> 1, C_ORANGE);
    }
  }

  // blink every ~3 s
  if (s.blink && --s.blink == 0) drawRows(faceOf(s.mood), 8, 12);
  if (!s.sleeping && (frameNo & 127) == 0) { s.blink = 6; drawRows(2, 8, 12); }

  // reaction mini-game
  if (s.play == 1) {
    if (btnPressed & BTN_LEFT) { playResult(false); return; }
    if (--s.playT == 0) { s.play = 2; pixAll(0, 255, 0); hud(); }
    return;
  }
  if (s.play == 2) {
    s.playT++;
    if (btnPressed & BTN_LEFT) playResult(s.playT <= 20);
    else if (s.playT > 40) playResult(false);
    return;
  }

  if (btnPressed & BTN_DOWN) { toggleSleep(); sfx(SFX_SELECT); return; }
  if (s.sleeping || !btnPressed) return;
  if (btnPressed & BTN_UP) {
    add(0, 25);
    s.anim = 25;
    sfx(SFX_COIN);
    fxFlash(255, 120, 0, 8);
    say(M_YUM);
  } else if (btnPressed & BTN_RIGHT) {
    add(3, 40);
    sfx(SFX_BLIP);
    fxFlash(0, 200, 255, 8);
    say(M_FRESH);
  } else {                                            // LEFT: start the game
    s.play = 1;
    s.playT = 40 + rnd(100);
    pixAll(0, 0, 0);
    lcdTextCenterP(4, S_GREEN, C_WHITE, C_BLACK, 1);
    hud();
    return;
  }
  refresh();
  save();
}

const char NAME[] PROGMEM = "Kypruino Pet";
const char TAG[]  PROGMEM = "PET\nFEED\nPLAY\nCLEAN\nSLEEP";

} // namespace

const Game gamePet PROGMEM = { ID_PET, NAME, TAG, start, update, hud };

#endif
