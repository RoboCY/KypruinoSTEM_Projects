/*
  Beat Lanes — a rhythm game. Notes fall down four coloured lanes; press the
  lane's button (LEFT / UP / DOWN / RIGHT, matching the lanes left to right)
  when a note crosses the white hit line. PERFECT = 3 points, GOOD = 1 point,
  both times a combo multiplier (combo/10 + 1, max 4). Only the notes you hit
  are heard, so the tune itself tells you how well you are playing. Health
  starts at 50: PERFECT +3, GOOD +1, MISS -8, 0 = game over. Songs alternate
  Ode to Joy / Twinkle Twinkle and get faster every two rounds.
*/
#include "Games.h"
#if GAME_BEAT

namespace {

#define HIT_Y  106            // note top when exactly on the hit line (line at 108..109)
#define LEAD   56             // frames a note needs to fall from the top to the hit line
#define MAX_NOTES 8

struct Note { uint8_t lane, pitch, len; int8_t y; };   // pitch 0 = free slot

struct State {
  Note notes[MAX_NOTES];
  uint16_t songFrame;   // frames since the song started
  uint16_t nextDue;     // songFrame at which the next note should be hit
  uint8_t pos;          // byte index into the song
  uint8_t round;        // songs completed
  uint8_t tick;         // frames per eighth note
  uint8_t health;
  uint8_t combo;
  uint8_t sndTimer;     // frames until the hit note is silenced
  uint8_t beatTimer;    // frames until the next pixel pulse
  uint8_t pulse;        // frames the pixel pulse still lasts
  uint8_t lastLane;     // lane of the last hit → pixel colour
  uint8_t judge;        // last message shown on the OLED
  bool done;            // song data exhausted
};
State& s = *(State*)gameRam;
static_assert(sizeof(State) <= GAME_RAM_SIZE, "Beat state too big");

// Lanes left to right: 0 = LEFT (yellow), 1 = UP (red), 2 = DOWN (green), 3 = RIGHT (blue)
const uint16_t COL_DIM[4] PROGMEM = { RGB(56, 56, 0), RGB(72, 0, 0), RGB(0, 56, 0), RGB(0, 0, 96) };
const uint16_t COL_LIT[4] PROGMEM = { C_YELLOW, C_RED, C_GREEN, C_BLUE };
const uint8_t  BTN_OF[4]  PROGMEM = { BTN_LEFT, BTN_UP, BTN_DOWN, BTN_RIGHT };
const uint8_t  RGB_OF[12] PROGMEM = { 255, 255, 0,  255, 0, 0,  0, 255, 0,  0, 0, 255 };
const uint8_t  TEMPO[8]   PROGMEM = { 8, 8, 6, 6, 5, 5, 4, 4 };   // frames per eighth note

// Songs: (note, lane<<6 | eighth-note ticks) pairs ending in END. Each song
// maps its pitches to lanes so LEFT and RIGHT never follow each other
// (LEFT+RIGHT held together is the engine's pause combo).
#define OL(n) ((n) == N_C4 || (n) == N_G3 ? 0 : (n) == N_E4 || (n) == N_G4 ? 1 : (n) == N_D4 ? 2 : 3)
#define OD(n, t) n, (uint8_t)((OL(n) << 6) | (t))
const uint8_t SONG_ODE[] PROGMEM = {
  OD(N_E4,2), OD(N_E4,2), OD(N_F4,2), OD(N_G4,2), OD(N_G4,2), OD(N_F4,2), OD(N_E4,2), OD(N_D4,2),
  OD(N_C4,2), OD(N_C4,2), OD(N_D4,2), OD(N_E4,2), OD(N_E4,3), OD(N_D4,1), OD(N_D4,4),
  OD(N_E4,2), OD(N_E4,2), OD(N_F4,2), OD(N_G4,2), OD(N_G4,2), OD(N_F4,2), OD(N_E4,2), OD(N_D4,2),
  OD(N_C4,2), OD(N_C4,2), OD(N_D4,2), OD(N_E4,2), OD(N_D4,3), OD(N_C4,1), OD(N_C4,4),
  OD(N_D4,2), OD(N_D4,2), OD(N_E4,2), OD(N_C4,2), OD(N_D4,2), OD(N_E4,1), OD(N_F4,1), OD(N_E4,2), OD(N_C4,2),
  OD(N_D4,2), OD(N_E4,1), OD(N_F4,1), OD(N_E4,2), OD(N_D4,2), OD(N_C4,2), OD(N_D4,2), OD(N_G3,4),
  OD(N_E4,2), OD(N_E4,2), OD(N_F4,2), OD(N_G4,2), OD(N_G4,2), OD(N_F4,2), OD(N_E4,2), OD(N_D4,2),
  OD(N_C4,2), OD(N_C4,2), OD(N_D4,2), OD(N_E4,2), OD(N_D4,3), OD(N_C4,1), OD(N_C4,4),
  END
};
#undef OL
#undef OD

#define TL(n) ((n) == N_C4 ? 0 : (n) == N_G4 || (n) == N_D4 ? 1 : (n) == N_E4 ? 3 : 2)
#define TW(n, t) n, (uint8_t)((TL(n) << 6) | (t))
const uint8_t SONG_TWINKLE[] PROGMEM = {
  TW(N_C4,2), TW(N_C4,2), TW(N_G4,2), TW(N_G4,2), TW(N_A4,2), TW(N_A4,2), TW(N_G4,4),
  TW(N_F4,2), TW(N_F4,2), TW(N_E4,2), TW(N_E4,2), TW(N_D4,2), TW(N_D4,2), TW(N_C4,4),
  TW(N_G4,2), TW(N_G4,2), TW(N_F4,2), TW(N_F4,2), TW(N_E4,2), TW(N_E4,2), TW(N_D4,4),
  TW(N_G4,2), TW(N_G4,2), TW(N_F4,2), TW(N_F4,2), TW(N_E4,2), TW(N_E4,2), TW(N_D4,4),
  TW(N_C4,2), TW(N_C4,2), TW(N_G4,2), TW(N_G4,2), TW(N_A4,2), TW(N_A4,2), TW(N_G4,4),
  TW(N_F4,2), TW(N_F4,2), TW(N_E4,2), TW(N_E4,2), TW(N_D4,2), TW(N_D4,2), TW(N_C4,4),
  END
};
#undef TL
#undef TW

enum { J_MISS = 0, J_GOOD = 5, J_PERFECT = 10, J_COMBO = 16, J_GO = 22 };   // offsets into S_MSG
const char S_MSG[]   PROGMEM = "MISS\0GOOD\0PRFCT\0COMBO\0GO!";
const char S_TITLE[] PROGMEM = "BEAT";
const char S_COMBO[] PROGMEM = "COMBO";
const char S_HP[]    PROGMEM = "HP";

// ---------------- LCD ----------------

int16_t laneX(uint8_t lane) { return lane * 40 + 4; }

// Repaint lane background under a note strip, restoring the hit line if covered.
void paintBg(uint8_t lane, int16_t y, uint8_t h) {
  int16_t x = laneX(lane);
  lcdFillRect(x, y, 32, h, pgm_read_word(&COL_DIM[lane]));
  int16_t a = y > 108 ? y : 108, b = y + h < 110 ? y + h : 110;
  if (b > a) lcdFillRect(x, a, 32, b - a, C_WHITE);
}

void drawNote(Note& n, int16_t y, uint8_t h) {
  lcdFillRect(laneX(n.lane), y, 32, h, pgm_read_word(&COL_LIT[n.lane]));
}

void killNote(Note& n) {
  paintBg(n.lane, n.y, 6);
  n.pitch = 0;
}

// ---------------- OLED ----------------

void hudStats() {
  hudScoreBlock(10);
  oledFillRect(7, 58, 25, 16, false);
  oledNumberAt(31, 58, s.combo, 2);
  oledVBar(0, 58, 6, 36, s.health);
}

void hud() {
  oledClear();
  oledTextCenterP(0, S_TITLE, 1);
  oledTextCenterP(49, S_COMBO, 1);
  oledTextP(8, 86, S_HP, 1);
  hudStats();
  hudMessage(S_MSG + s.judge);
}

void showJudge(uint8_t j) {
  s.judge = j;
  hudMessage(S_MSG + j);
  hudStats();
}

// ---------------- pixels / sound ----------------

void pixLane(uint8_t shift) {
  const uint8_t* p = RGB_OF + s.lastLane * 3;
  pixAll(pgm_read_byte(p) >> shift, pgm_read_byte(p + 1) >> shift, pgm_read_byte(p + 2) >> shift);
}

const uint8_t* song() { return (s.round & 1) ? SONG_TWINKLE : SONG_ODE; }

void beginSong() {
  s.tick = pgm_read_byte(&TEMPO[s.round < 7 ? s.round : 7]);
  s.pos = 0;
  s.done = false;
  s.songFrame = 0;
  s.nextDue = s.tick * (s.tick < 8 ? 16 : 8);   // whole beats of lead-in, at least LEAD frames
  s.beatTimer = s.tick * 2;
}

// ---------------- judgement ----------------

void miss(bool loseHp) {
  s.combo = 0;
  fxMode(FX_HOLD, 0);
  fxFlash(255, 0, 0, 4);
  if (loseHp) {
    if (s.health <= 8) { s.health = 0; gameOver(); }
    else s.health -= 8;
  }
  showJudge(J_MISS);
}

void hit(Note& n, uint8_t pts) {
  soundNote(n.pitch);
  s.sndTimer = n.len * s.tick - 2;
  s.lastLane = n.lane;
  killNote(n);
  if (s.combo < 99) s.combo++;
  uint8_t tens = s.combo / 10;
  score += pts * (tens < 3 ? tens + 1 : 4);
  s.health = s.health + pts > 100 ? 100 : s.health + pts;
  if (s.combo >= 10) fxMode(FX_RAINBOW, 0);
  showJudge(s.combo == tens * 10 ? J_COMBO : pts == 3 ? J_PERFECT : J_GOOD);
}

void press(uint8_t lane) {
  Note* best = 0;
  uint8_t bestD = 255;
  for (uint8_t i = 0; i < MAX_NOTES; i++) {
    Note& n = s.notes[i];
    if (!n.pitch || n.lane != lane) continue;
    int8_t d = n.y - HIT_Y;
    uint8_t ad = d < 0 ? -d : d;
    if (ad < bestD) { bestD = ad; best = &n; }
  }
  if (bestD <= 10) hit(*best, bestD <= 4 ? 3 : 1);
  else miss(false);
}

// ---------------- game ----------------

void start() {
  s.health = 50;
  s.lastLane = 1;
  s.judge = J_GO;
  beginSong();
  for (uint8_t i = 0; i < 4; i++) {
    lcdFillRect(i * 40 + 2, 0, 36, LCD_H, pgm_read_word(&COL_DIM[i]));
    lcdFillRect(i * 40 + 2, 108, 36, 2, C_WHITE);
  }
  pixLane(3);
}

void update() {
  if (s.sndTimer && --s.sndTimer == 0) soundOff();
  if (s.pulse && --s.pulse == 0) pixLane(3);
  if (--s.beatTimer == 0) {                  // pulse the pixels on every beat
    s.beatTimer = s.tick * 2;
    s.pulse = 3;
    pixLane(0);
  }

  // move the notes down; notes past the hit zone are missed
  uint8_t active = 0;
  for (uint8_t i = 0; i < MAX_NOTES; i++) {
    Note& n = s.notes[i];
    if (!n.pitch) continue;
    if (n.y > HIT_Y + 10) { killNote(n); miss(true); if (!s.health) return; continue; }
    paintBg(n.lane, n.y, 2);
    n.y += 2;
    drawNote(n, n.y + 4, 2);
    active++;
  }

  // spawn notes that are due within LEAD frames, placed so they arrive on time
  const uint8_t* sg = song();
  while (!s.done) {
    uint16_t ahead = s.nextDue - s.songFrame;
    if (ahead > LEAD) break;
    Note* n = 0;
    for (uint8_t i = 0; i < MAX_NOTES; i++) if (!s.notes[i].pitch) { n = &s.notes[i]; break; }
    if (!n) break;
    uint8_t t = pgm_read_byte(sg + s.pos + 1);
    n->pitch = pgm_read_byte(sg + s.pos);
    n->lane = t >> 6;
    n->len = t & 63;
    n->y = HIT_Y - (int8_t)(ahead * 2);
    drawNote(*n, n->y, 6);
    s.pos += 2;
    s.nextDue += n->len * s.tick;
    active++;
    if (pgm_read_byte(sg + s.pos) == END) s.done = true;
  }
  s.songFrame++;

  for (uint8_t i = 0; i < 4; i++)
    if (btnPressed & pgm_read_byte(&BTN_OF[i])) press(i);

  if (s.done && !active) {                   // song finished with health left
    sfx(SFX_LEVELUP);
    s.round++;
    beginSong();
  }
}

const char NAME[] PROGMEM = "Beat Lanes";
const char TAG[]  PROGMEM = "BEAT\nHIT\nTHE\nNOTES\nIN\nTIME";

} // namespace

const Game gameBeat PROGMEM = { ID_BEAT, NAME, TAG, start, update, hud };

#endif
