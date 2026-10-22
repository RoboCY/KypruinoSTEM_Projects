#include "Engine.h"

uint8_t gameRam[GAME_RAM_SIZE];
uint16_t score = 0, best = 0, frameNo = 0;

enum { ST_RUNNING, ST_OVER, ST_WON, ST_QUIT };
static uint8_t state;
static Game cur;                 // RAM copy of the running game's record
static uint16_t rs = 0xACE1;     // random state

// ---------------- small services ----------------

uint16_t rnd16() {
  rs ^= rs << 7;
  rs ^= rs >> 9;
  rs ^= rs << 8;
  return rs;
}

uint8_t rnd(uint8_t n) { return ((rnd16() & 0xFF) * (uint16_t)n) >> 8; }

void gameOver() { state = ST_OVER; }
void gameWon()  { state = ST_WON; }

static const char S_SCORE[] PROGMEM = "SCORE";
static const char S_BEST[]  PROGMEM = "BEST";

void hudLabel(int16_t y, const char* labelP, uint16_t value) {
  oledFillRect(0, y, OLED_W, 18, false);
  oledTextCenterP(y, labelP, 1);
  oledNumber(y + 9, value, 1);
}

void hudScoreBlock(int16_t y) {
  hudLabel(y, S_SCORE, score);
  hudLabel(y + 19, S_BEST, best);
}

void hudMessage(const char* linesP) {
  oledFillRect(0, 96, OLED_W, 32, false);
  oledLinesP(98, linesP, 1);
}

// ---------------- init + splash ----------------

static const char S_TITLE1[] PROGMEM = "KYPRUINO";
static const char S_TITLE2[] PROGMEM = "ARCADE";
static const char S_BY[]     PROGMEM = "by ROBO  robo.com.cy";

void engineInit() {
  lcdInit();
  oledInit();
  pixInit();
  soundInit();
  inputInit();

  if (eeprom_read_byte(EEPROM_MAGIC_ADDR) != 0xA5) {      // first run: wipe scores
    for (uint8_t i = 0; i < 16; i++) eeprom_update_word(EEPROM_BEST_ADDR + i, 0);
    eeprom_update_byte(EEPROM_SOUND_ADDR, 0);
    eeprom_update_byte(EEPROM_MAGIC_ADDR, 0xA5);
  }
  soundMuted = eeprom_read_byte(EEPROM_SOUND_ADDR) == 1;

  lcdTextCenterP(28, S_TITLE1, C_CYAN, C_BLACK, 3);
  lcdTextCenterP(60, S_TITLE2, C_YELLOW, C_BLACK, 3);
  lcdTextCenterP(100, S_BY, C_GREY, C_BLACK, 1);
  static const char S_HI[] PROGMEM = "HELLO\nPLAYR";
  oledLinesP(50, S_HI, 1);
  oledFlush();
  fxMode(FX_RAINBOW, 0);
  sfx(SFX_LEVELUP);
  for (uint8_t i = 0; i < 60; i++) { soundUpdate(); fxUpdate(); waitMs(FRAME_MS); }
}

// ---------------- menu ----------------

static const char S_BTNTEST[] PROGMEM = "Button test";
static const char S_SOUND_ON[]  PROGMEM = "Sound: ON ";
static const char S_SOUND_OFF[] PROGMEM = "Sound: OFF";
static const char S_PRESS[] PROGMEM = "PRESS\nRIGHT";
static const char S_MENU_TAG[] PROGMEM = "PICK\nA\nGAME";

#define MENU_TOP   18
#define ROW_H      12
#define ROWS       9

static void loadGame(uint8_t idx) {
  memcpy_P(&cur, (const Game*)pgm_read_ptr(&GAMES[idx]), sizeof(Game));
}

static void drawMenuRow(uint8_t item, uint8_t row, bool selected) {
  int16_t y = MENU_TOP + row * ROW_H;
  uint16_t bg = selected ? C_BLUE : C_BLACK;
  lcdFillRect(0, y, LCD_W, ROW_H, bg);
  const char* name;
  if (item < GAME_COUNT) { loadGame(item); name = cur.name; }
  else if (item == GAME_COUNT) name = S_BTNTEST;
  else name = soundMuted ? S_SOUND_OFF : S_SOUND_ON;
  if (selected) lcdChar(4, y + 2, '>', C_YELLOW, bg, 1);
  lcdTextP(16, y + 2, name, selected ? C_WHITE : C_GREY, bg, 1);
}

static void drawMenu(uint8_t sel, uint8_t first) {
  uint8_t total = GAME_COUNT + 2;
  for (uint8_t r = 0; r < ROWS; r++) {
    uint8_t item = first + r;
    if (item < total) drawMenuRow(item, r, item == sel);
    else lcdFillRect(0, MENU_TOP + r * ROW_H, LCD_W, ROW_H, C_BLACK);
  }
  // OLED: tagline + best score of the selected game
  oledClear();
  if (sel < GAME_COUNT) {
    loadGame(sel);
    oledLinesP(2, cur.tagline, 1);
    uint16_t b = eeprom_read_word(EEPROM_BEST_ADDR + cur.id);
    hudLabel(60, S_BEST, b == 0xFFFF ? 0 : b);
  } else {
    oledLinesP(2, S_MENU_TAG, 1);
  }
  oledLinesP(104, S_PRESS, 1);
  oledFlush();
  fxMode(FX_BREATHE, sel * 21);
}

static void drawMenuFrame() {
  lcdClear(C_BLACK);
  lcdFillRect(0, 0, LCD_W, 16, C_NAVY);
  static const char S_HDR[] PROGMEM = "KYPRUINO ARCADE";
  lcdTextCenterP(4, S_HDR, C_YELLOW, C_NAVY, 1);
}

static void attractMode() {
  int16_t x = 10, y = 50, dx = 1, dy = 1;
  uint8_t hue = 0;
  lcdClear(C_BLACK);
  fxMode(FX_RAINBOW, 0);
  for (;;) {
    inputUpdate();
    if (btnPressed) return;
    lcdTextP(x, y, S_TITLE1, C_BLACK, C_BLACK, 2);
    x += dx; y += dy;
    if (x <= 0 || x >= LCD_W - 96) dx = -dx;
    if (y <= 0 || y >= LCD_H - 16) dy = -dy;
    lcdTextP(x, y, S_TITLE1, lcdHsv(hue += 2, 255, 255), C_BLACK, 2);
    fxUpdate();
    waitMs(FRAME_MS);
  }
}

static void buttonTest() {
  lcdClear(C_BLACK);
  static const char S_EXIT[] PROGMEM = "LEFT+RIGHT to exit";
  lcdTextCenterP(116, S_EXIT, C_GREY, C_BLACK, 1);
  static const char S_T[] PROGMEM = "PRESS\nEACH\nBUTON";
  oledClear(); oledLinesP(40, S_T, 1); oledFlush();
  const int8_t px[4] = { 70, 70, 40, 100 };
  const int8_t py[4] = { 14, 74, 44, 44 };
  uint8_t last = 0xFF;
  for (;;) {
    inputUpdate();
    if ((btnHeld & (BTN_LEFT | BTN_RIGHT)) == (BTN_LEFT | BTN_RIGHT)) break;
    if (btnHeld != last) {
      last = btnHeld;
      for (uint8_t i = 0; i < 4; i++) {
        bool on = btnHeld & (1 << i);
        lcdFillRect(px[i], py[i], 20, 20, on ? C_GREEN : C_DGREY);
        lcdChar(px[i] + 7, py[i] + 6, "UDLR"[i], C_WHITE, on ? C_GREEN : C_DGREY, 1);
      }
      if (btnPressed) sfx(SFX_BLIP);
    }
    soundUpdate(); fxUpdate();
    waitMs(FRAME_MS);
  }
  while (btnHeld) { inputUpdate(); waitMs(FRAME_MS); }
}

// ---------------- pause / game over ----------------

static const char S_PAUSE[] PROGMEM = "PAUSE\n\nUP\nPLAY\n\nDOWN\nQUIT";

static void pauseScreen() {
  soundOff();
  oledClear();
  oledLinesP(20, S_PAUSE, 1);
  oledFlush();
  fxMode(FX_BREATHE, 160);
  while (btnHeld) { inputUpdate(); waitMs(FRAME_MS); }   // wait for the combo to be released
  for (;;) {
    inputUpdate();
    fxUpdate();
    if (btnPressed & BTN_UP) break;
    if (btnPressed & BTN_DOWN) { state = ST_QUIT; break; }
    waitMs(FRAME_MS);
  }
  fxMode(FX_HOLD, 0);
  while (btnHeld) { inputUpdate(); waitMs(FRAME_MS); }
}

static const char S_OVER[] PROGMEM = "GAME OVER";
static const char S_WIN[]  PROGMEM = "YOU WIN!";
static const char S_NEWBEST[] PROGMEM = "NEW BEST!";
static const char S_OVER_O[] PROGMEM = "GAME\nOVER";
static const char S_WIN_O[]  PROGMEM = "YOU\nWIN!";
static const char S_NEWBEST_O[] PROGMEM = "NEW\nBEST!";

static void endScreen() {
  bool won = state == ST_WON;
  bool newBest = score > best;
  if (newBest) {
    best = score;
    eeprom_update_word(EEPROM_BEST_ADDR + cur.id, best);
  }
  // let the game's last sound, flash and OLED message play out first
  oledFlush();
  for (uint8_t i = 0; i < 20; i++) { soundUpdate(); fxUpdate(); waitMs(FRAME_MS); }
  soundStop();
  uint16_t col = won ? C_GREEN : C_RED;
  lcdFillRect(24, 36, 112, 56, C_BLACK);
  lcdRect(24, 36, 112, 56, col);
  lcdRect(26, 38, 108, 52, col);
  lcdTextCenterP(44, won ? S_WIN : S_OVER, col, C_BLACK, 1);
  lcdTextP(44, 60, S_SCORE, C_GREY, C_BLACK, 1);
  lcdNumber(116, 60, score, C_WHITE, C_BLACK, 1);
  if (newBest) lcdTextCenterP(76, S_NEWBEST, C_YELLOW, C_BLACK, 1);
  oledClear();
  oledLinesP(4, won ? S_WIN_O : S_OVER_O, 1);
  hudScoreBlock(30);
  if (newBest) oledLinesP(100, S_NEWBEST_O, 1);
  oledFlush();
  if (newBest || won) { fxMode(FX_RAINBOW, 0); sfx(SFX_WIN); }
  else { fxMode(FX_HOLD, 0); pixAll(255, 0, 0); sfx(SFX_LOSE); }
  for (uint8_t i = 0; i < 32; i++) { soundUpdate(); fxUpdate(); waitMs(FRAME_MS); }
  inputWaitPress();
  while (btnHeld) { inputUpdate(); waitMs(FRAME_MS); }
}

// ---------------- game loop ----------------

static void runGame(uint8_t idx) {
  loadGame(idx);
  best = eeprom_read_word(EEPROM_BEST_ADDR + cur.id);
  if (best == 0xFFFF) best = 0;
  score = 0;
  frameNo = 0;
  state = ST_RUNNING;
  rs ^= millis();
  if (rs == 0) rs = 1;
  memset(gameRam, 0, GAME_RAM_SIZE);
  soundStop();
  fxMode(FX_HOLD, 0);
  pixAll(0, 0, 0);
  lcdClear(C_BLACK);
  oledClear();
  while (btnHeld) { inputUpdate(); waitMs(FRAME_MS); }
  cur.init();
  cur.hud();
  oledFlush();

  unsigned long next = millis();
  uint8_t comboFrames = 0;
  while (state == ST_RUNNING) {
    inputUpdate();
    // LEFT + RIGHT held together for a third of a second pauses the game
    if ((btnHeld & (BTN_LEFT | BTN_RIGHT)) == (BTN_LEFT | BTN_RIGHT)) comboFrames++;
    else comboFrames = 0;
    if (comboFrames >= 12) {
      comboFrames = 0;
      pauseScreen();
      if (state != ST_RUNNING) break;
      cur.hud();
      next = millis();
      continue;
    }
    cur.update();
    frameNo++;
    soundUpdate();
    fxUpdate();
    if ((frameNo & 3) == 0) oledFlush();
    next += FRAME_MS;
    long wait = (long)(next - millis());
    if (wait > 0) waitMs(wait); else next = millis();
  }
  if (state != ST_QUIT) endScreen();
  soundStop();
}

void engineMenu() {
  static uint8_t sel = 0;
  uint8_t first = 0, total = GAME_COUNT + 2;
  uint16_t idle = 0;
  drawMenuFrame();
  drawMenu(sel, first);
  for (;;) {
    inputUpdate();
    soundUpdate();
    fxUpdate();
    if (btnRepeat & BTN_DOWN) { sel = (sel + 1) % total; sfx(SFX_BLIP); }
    if (btnRepeat & BTN_UP)   { sel = (sel + total - 1) % total; sfx(SFX_BLIP); }
    if (btnRepeat & (BTN_UP | BTN_DOWN)) {
      if (sel < first) first = sel;
      if (sel >= first + ROWS) first = sel - ROWS + 1;
      drawMenu(sel, first);
      idle = 0;
      rs ^= millis();
    }
    if (btnPressed & BTN_RIGHT) {
      sfx(SFX_SELECT);
      for (uint8_t i = 0; i < 6; i++) { soundUpdate(); waitMs(FRAME_MS); }
      if (sel < GAME_COUNT) runGame(sel);
      else if (sel == GAME_COUNT) buttonTest();
      else { soundMuted = !soundMuted; eeprom_update_byte(EEPROM_SOUND_ADDR, soundMuted); }
      drawMenuFrame();
      drawMenu(sel, first);
      idle = 0;
      continue;
    }
    if (++idle > 1600) {          // 40 s without a press
      attractMode();
      drawMenuFrame();
      drawMenu(sel, first);
      idle = 0;
    }
    waitMs(FRAME_MS);
  }
}
