#include "Sound.h"

bool soundMuted = false;

static const uint16_t NOTE_HZ[48] PROGMEM = {
  131, 139, 147, 156, 165, 175, 185, 196, 208, 220, 233, 247,      // C3..B3
  262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494,      // C4..B4
  523, 554, 587, 622, 659, 698, 740, 784, 831, 880, 932, 988,      // C5..B5
  1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661, 1760, 1865, 1976 // C6..B6
};

// ---------------- sound effects (tempo 25 ms per tick) ----------------

static const uint8_t SFX_BLIP_S[]    PROGMEM = { N_E6, 1, END };
static const uint8_t SFX_SELECT_S[]  PROGMEM = { N_C5, 1, N_G5, 1, N_C6, 2, END };
static const uint8_t SFX_HIT_S[]     PROGMEM = { N_C4, 1, N_C3, 2, END };
static const uint8_t SFX_COIN_S[]    PROGMEM = { N_B5, 1, N_E6, 3, END };
static const uint8_t SFX_LEVELUP_S[] PROGMEM = { N_C5, 1, N_E5, 1, N_G5, 1, N_C6, 1, N_E6, 1, N_G6, 3, END };
static const uint8_t SFX_LOSE_S[]    PROGMEM = { N_G4, 3, N_FS4, 3, N_F4, 3, N_E4, 6, END };
static const uint8_t SFX_WIN_S[]     PROGMEM = { N_C5, 2, N_C5, 1, N_C5, 1, N_C5, 2, N_GS4, 2, N_AS4, 2, N_C5, 2, N_AS4, 1, N_C5, 6, END };
static const uint8_t SFX_BOUNCE_S[]  PROGMEM = { N_A5, 1, END };
static const uint8_t SFX_SHOOT_S[]   PROGMEM = { N_C6, 1, N_G5, 1, END };
static const uint8_t SFX_EXPLODE_S[] PROGMEM = { N_D3, 1, N_CS3, 1, N_C3, 2, END };
static const uint8_t SFX_JUMP_S[]    PROGMEM = { N_C5, 1, N_E5, 1, N_G5, 1, END };

static const uint8_t* const SFX_TABLE[SFX_COUNT] PROGMEM = {
  SFX_BLIP_S, SFX_SELECT_S, SFX_HIT_S, SFX_COIN_S, SFX_LEVELUP_S, SFX_LOSE_S, SFX_WIN_S,
  SFX_BOUNCE_S, SFX_SHOOT_S, SFX_EXPLODE_S, SFX_JUMP_S
};

// ---------------- sequencer state ----------------

static const uint8_t* seq = 0;      // currently playing sequence (music or sfx)
static uint8_t seqPos, seqTempo;
static bool seqLoop;
static unsigned long noteEnd;

static const uint8_t* music = 0;    // music parked while an effect plays
static uint8_t musicPos, musicTempo;
static bool musicLoop, sfxActive = false;

// ---------------- hardware ----------------

void soundInit() {
  PIN_OUTPUT(BUZZER_PIN);
  soundOff();
}

uint16_t noteHz(uint8_t note) {
  if (note == 0 || note > 48) return 0;
  return pgm_read_word(&NOTE_HZ[note - 1]);
}

void soundTone(uint16_t hz) {
  if (hz == 0 || soundMuted) { soundOff(); return; }
  uint16_t top = (uint16_t)(1000000UL / hz) - 1;   // prescaler 8, toggle → f = 1 MHz / (top+1)
  TCCR1A = _BV(COM1A0);
  TCCR1B = _BV(WGM12) | _BV(CS11);
  OCR1A = top;
  if (TCNT1 > top) TCNT1 = 0;
}

void soundNote(uint8_t note) { soundTone(noteHz(note)); }

void soundOff() {
  TCCR1A = 0;
  TCCR1B = 0;
  PIN_LOW(BUZZER_PIN);
}

// ---------------- sequencer ----------------

static void startNote() {
  uint8_t note = pgm_read_byte(seq + seqPos);
  if (note == END) {
    if (sfxActive) {                 // effect finished: go back to the music
      sfxActive = false;
      seq = music; seqPos = musicPos; seqTempo = musicTempo; seqLoop = musicLoop;
      if (!seq) { soundOff(); return; }
      note = pgm_read_byte(seq + seqPos);
      if (note == END) { if (seqLoop) { seqPos = 0; note = pgm_read_byte(seq); } else { seq = 0; soundOff(); return; } }
    } else if (seqLoop) {
      seqPos = 0;
      note = pgm_read_byte(seq);
    } else {
      seq = 0;
      soundOff();
      return;
    }
  }
  uint8_t ticks = pgm_read_byte(seq + seqPos + 1);
  seqPos += 2;
  soundNote(note);
  noteEnd = millis() + (uint16_t)ticks * seqTempo;
}

void soundPlay(const uint8_t* s, uint8_t tempoMs, bool loop) {
  music = s; musicPos = 0; musicTempo = tempoMs; musicLoop = loop;
  if (sfxActive) return;             // will start when the effect ends
  seq = s; seqPos = 0; seqTempo = tempoMs; seqLoop = loop;
  startNote();
}

void soundStop() {
  seq = 0; music = 0; sfxActive = false;
  soundOff();
}

void sfx(uint8_t id) {
  if (id >= SFX_COUNT) return;
  if (!sfxActive && seq) { music = seq; musicPos = seqPos; musicTempo = seqTempo; musicLoop = seqLoop; }
  sfxActive = true;
  seq = (const uint8_t*)pgm_read_ptr(&SFX_TABLE[id]);
  seqPos = 0; seqTempo = 25; seqLoop = false;
  startNote();
}

void soundUpdate() {
  if (!seq) return;
  unsigned long now = millis();
  if ((long)(now - noteEnd) >= 0) { startNote(); return; }
  if (noteEnd - now < 12) soundOff();   // tiny gap between notes so repeats are audible
}
