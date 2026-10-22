/*
  Lane Racer — top-down road with three lanes and oncoming traffic. LEFT / RIGHT
  snap your car to the next lane, hold UP to boost (double speed, double score),
  hold DOWN to brake. Score = distance driven, +5 for every car you pass.
  Hitting a car ends the race. Traffic gets faster the further you drive.
*/
#include "Games.h"
#if GAME_RACER

namespace {

#define LANES     3
#define SLOTS     3          // enemy cars on screen at once
#define CAR_W     12
#define CAR_H     16
#define PLAYER_Y  104
#define GAP       56         // px a car must travel before the next one may spawn

struct State {
  uint8_t  lane;             // player lane 0..2
  uint8_t  acc;              // scroll accumulator, 1/16 px
  uint8_t  phase;            // lane-dash phase 0..15
  uint8_t  metres;           // px since the last score tick
  uint16_t dist;             // 8-px units driven (drives the speed)
  uint8_t  eff;              // effective speed last frame, 1/16 px per frame
  uint8_t  mode;             // 0 VROOM, 1 BOOST, 2 FAST!, 3 CRASH
  uint16_t hudScore;         // score last drawn on the OLED
  uint8_t  eLane[SLOTS];     // 0xFF = slot empty
  uint8_t  eY[SLOTS];        // car top + 16 (0 = just above the screen)
  uint8_t  eCol[SLOTS];      // palette index, bit 7 = already passed
};
State& s = *(State*)gameRam;
static_assert(sizeof(State) <= GAME_RAM_SIZE, "Racer state too big");

const uint16_t PAL[8] PROGMEM = {
  C_RED, C_YELLOW, C_GREEN, C_MAGENTA, C_ORANGE, RGB(80, 80, 255), C_CYAN, C_WHITE
};
const char S_TITLE[] PROGMEM = "RACER";
const char S_SPEED[] PROGMEM = "SPEED";
const char S_MSG[]   PROGMEM = "VROOM\0BOOST\0FAST!\0CRASH";   // 6 bytes per message

uint8_t laneX(uint8_t lane) { return 42 + (lane << 5); }

// Car: coloured 12x16 cell with black notches between the wheels and a dark
// windscreen. Clipped rects, so it can slide in from above and out below.
// Always paints the whole cell (no trails when it moves).
void drawCar(int16_t x, int16_t y, uint16_t body) {
  lcdFillRect(x, y, CAR_W, CAR_H, body);
  lcdFillRect(x, y + 5, 2, 6, C_BLACK);
  lcdFillRect(x + 10, y + 5, 2, 6, C_BLACK);
  lcdFillRect(x + 3, y + 3, 6, 3, C_NAVY);
}

// h rows of both lane lines starting at row y
void dashRows(int16_t y, uint8_t h, uint16_t c) {
  lcdFillRect(63, y, 2, h, c);
  lcdFillRect(95, y, 2, h, c);
}

// Move every dash down by d (<= 8) px: erase the rows it left, draw the rows it entered
void scrollDashes(uint8_t d) {
  for (int16_t y = (int16_t)s.phase - 16; y < LCD_H; y += 16) {
    dashRows(y, d, C_BLACK);
    dashRows(y + 8, d, C_WHITE);
  }
  s.phase = (s.phase + d) & 15;
}

void spawn(uint8_t lane) {
  for (uint8_t i = 0; i < SLOTS; i++)
    if (s.eLane[i] == 0xFF) { s.eLane[i] = lane; s.eY[i] = 0; s.eCol[i] = rnd(6); return; }
}

void hud() {
  oledClear();
  oledTextCenterP(2, S_TITLE, 1);
  hudScoreBlock(12);
  oledTextCenterP(54, S_SPEED, 1);
  oledVBar(11, 64, 10, 30, s.eff - (s.eff >> 2));           // 128 -> 96 %
  hudMessage(S_MSG + s.mode * 6);
  s.hudScore = score;
}

void start() {
  lcdFillRect(0, 0, 32, LCD_H, C_DGREY);
  lcdFillRect(128, 0, 32, LCD_H, C_DGREY);
  scrollDashes(8);                               // draws the first dashes (phase 0 -> 8)
  s.lane = 1;
  for (uint8_t i = 0; i < SLOTS; i++) s.eLane[i] = 0xFF;
  drawCar(laneX(1), PLAYER_Y, C_WHITE);
}

void update() {
  // ---- lane change
  uint8_t nl = s.lane;
  if (btnPressed & BTN_LEFT  && nl > 0) nl--;
  if (btnPressed & BTN_RIGHT && nl < LANES - 1) nl++;
  if (nl != s.lane) {
    lcdFillRect(laneX(s.lane), PLAYER_Y, CAR_W, CAR_H, C_BLACK);
    s.lane = nl;
    drawCar(laneX(nl), PLAYER_Y, C_WHITE);
  }

  // ---- speed: rises with distance, x2 on boost, /2 on brake
  uint8_t base = 24 + (s.dist >> 5);
  if (base > 64) base = 64;
  bool boost = btnHeld & BTN_UP;
  uint8_t eff = boost ? base << 1 : (btnHeld & BTN_DOWN) ? base >> 1 : base;
  if (btnPressed & BTN_UP) { fxFlash(255, 255, 255, 4); sfx(SFX_SELECT); }
  if (eff != s.eff) {
    s.eff = eff;
    pixAllHue(((128 - eff) * 13) >> 4);          // green (slow) -> yellow -> red (fast)
  }
  uint8_t mode = boost ? 1 : base >= 48 ? 2 : 0;
  bool redraw = mode != s.mode;
  s.mode = mode;

  // ---- scroll the road
  uint8_t a = s.acc + eff;                       // max 15 + 128
  uint8_t d = a >> 4;                            // 0..8 px this frame
  s.acc = a & 15;
  if (d) {
    scrollDashes(d);
    s.metres += d;
    if (s.metres >= 8) { s.metres -= 8; s.dist++; score++; }

    bool clear = true;                           // room at the top for a new car?
    for (uint8_t i = 0; i < SLOTS; i++) {
      if (s.eLane[i] == 0xFF) continue;
      uint8_t x = laneX(s.eLane[i]);
      int16_t y = (int16_t)s.eY[i] - 16;
      lcdFillRect(x, y, CAR_W, d, C_BLACK);      // rows the car left behind
      s.eY[i] += d;
      if (s.eY[i] >= LCD_H + 16) { s.eLane[i] = 0xFF; continue; }
      drawCar(x, y + d, pgm_read_word(&PAL[s.eCol[i] & 7]));
      if (s.eY[i] < GAP) clear = false;
      if (s.eLane[i] == s.lane && s.eY[i] > PLAYER_Y + 2 && s.eY[i] < PLAYER_Y + CAR_H + 14) {
        drawCar(laneX(s.lane), PLAYER_Y, C_RED);
        s.mode = 3;
        hud();
        fxFlash(255, 0, 0, 20);
        sfx(SFX_EXPLODE);
        gameOver();
        return;
      }
      if (!(s.eCol[i] & 0x80) && s.eY[i] >= PLAYER_Y + CAR_H + 16) {  // fully behind us
        s.eCol[i] |= 0x80;
        score += 5;
        sfx(SFX_BLIP);
      }
    }
    if (clear && rnd(8) == 0) {
      uint8_t l = rnd(LANES);
      spawn(l);
      if (rnd(3) == 0) {                         // sometimes a pair, never all three lanes
        if (++l == LANES) l = 0;
        spawn(l);
      }
    }
  }

  if (redraw || ((frameNo & 7) == 0 && score != s.hudScore)) hud();
}

const char NAME[] PROGMEM = "Lane Racer";
const char TAG[]  PROGMEM = "RACER\nDODGE\nTHE\nCARS";

} // namespace

const Game gameRacer PROGMEM = { ID_RACER, NAME, TAG, start, update, hud };

#endif
