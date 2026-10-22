// Classic 5x7 ASCII font, characters 32..126, 5 column bytes each (bit 0 = top row).
#pragma once
#include <avr/pgmspace.h>

extern const uint8_t FONT5x7[] PROGMEM;

static inline uint8_t fontCol(char ch, uint8_t col) {
  if (ch < 32 || ch > 126) ch = '?';
  return pgm_read_byte(&FONT5x7[(ch - 32) * 5 + col]);
}
