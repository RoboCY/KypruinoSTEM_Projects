/*
  Sound.h — buzzer on D9 driven by Timer1 in CTC toggle mode (no CPU time
  while a note plays) plus a non-blocking sequencer for music and effects.

  A sequence is a PROGMEM byte array of (note, ticks) pairs ending in END.
  Note 0 is a rest. One tick = the tempo in milliseconds given to soundPlay().
*/
#pragma once
#include "Config.h"

#define REST 0
#define END  0xFF

// Note numbers: 1 = C3 ... 48 = B6
#define N_C3 1
#define N_CS3 2
#define N_D3 3
#define N_DS3 4
#define N_E3 5
#define N_F3 6
#define N_FS3 7
#define N_G3 8
#define N_GS3 9
#define N_A3 10
#define N_AS3 11
#define N_B3 12
#define N_C4 13
#define N_CS4 14
#define N_D4 15
#define N_DS4 16
#define N_E4 17
#define N_F4 18
#define N_FS4 19
#define N_G4 20
#define N_GS4 21
#define N_A4 22
#define N_AS4 23
#define N_B4 24
#define N_C5 25
#define N_CS5 26
#define N_D5 27
#define N_DS5 28
#define N_E5 29
#define N_F5 30
#define N_FS5 31
#define N_G5 32
#define N_GS5 33
#define N_A5 34
#define N_AS5 35
#define N_B5 36
#define N_C6 37
#define N_CS6 38
#define N_D6 39
#define N_DS6 40
#define N_E6 41
#define N_F6 42
#define N_FS6 43
#define N_G6 44
#define N_GS6 45
#define N_A6 46
#define N_AS6 47
#define N_B6 48

enum {
  SFX_BLIP = 0, SFX_SELECT, SFX_HIT, SFX_COIN, SFX_LEVELUP, SFX_LOSE, SFX_WIN,
  SFX_BOUNCE, SFX_SHOOT, SFX_EXPLODE, SFX_JUMP, SFX_COUNT
};

extern bool soundMuted;

void soundInit();
void soundTone(uint16_t hz);                 // raw tone, 0 = off
void soundNote(uint8_t note);                // note number from the table above
void soundOff();
void soundPlay(const uint8_t* seq, uint8_t tempoMs, bool loop);  // background music
void soundStop();                            // stop music and effects
void sfx(uint8_t id);                        // short effect; music resumes afterwards
void soundUpdate();                          // per frame
uint16_t noteHz(uint8_t note);
