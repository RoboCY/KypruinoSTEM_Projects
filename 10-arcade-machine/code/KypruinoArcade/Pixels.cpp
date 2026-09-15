#include "Pixels.h"
#include <util/delay.h>

static uint8_t base[PIXEL_COUNT * 3];   // GRB order, unscaled
static uint8_t out[PIXEL_COUNT * 3];
static uint8_t mode = FX_HOLD, modeHue = 0;
static uint8_t flashFrames = 0, flashG, flashR, flashB;
static uint8_t tick = 0;
static bool dirty = true;

// 800 kHz WS2812 bit-bang for a 16 MHz AVR, PORTB bit 0 (D8). Interrupts are
// off for ~30 µs while the 9 bytes go out.
// Every read-write operand is early-clobber ("+&"): without it the compiler
// may put "next" and "lo" in the same register (next starts equal to lo),
// which leaves the data line stuck high after the first 1 bit.
static void show() {
  volatile uint8_t* port = &PORTB;
  uint8_t hi = PORTB | _BV(0), lo = PORTB & ~_BV(0);
  uint8_t* ptr = out;
  uint16_t i = sizeof(out);
  uint8_t next = lo, bit = 8, b = *ptr++;
  cli();
  asm volatile(
    "head20:"                   "\n\t"
    "st   %a[port],  %[hi]"    "\n\t"
    "sbrc %[byte],  7"         "\n\t"
    "mov  %[next], %[hi]"      "\n\t"
    "dec  %[bit]"              "\n\t"
    "st   %a[port],  %[next]"  "\n\t"
    "mov  %[next] ,  %[lo]"    "\n\t"
    "breq nextbyte20"          "\n\t"
    "rol  %[byte]"             "\n\t"
    "rjmp .+0"                 "\n\t"
    "nop"                      "\n\t"
    "st   %a[port],  %[lo]"    "\n\t"
    "nop"                      "\n\t"
    "rjmp .+0"                 "\n\t"
    "rjmp head20"              "\n\t"
    "nextbyte20:"              "\n\t"
    "ldi  %[bit]  ,  8"        "\n\t"
    "ld   %[byte] ,  %a[ptr]+" "\n\t"
    "st   %a[port], %[lo]"     "\n\t"
    "nop"                      "\n\t"
    "sbiw %[count], 1"         "\n\t"
    "brne head20"              "\n"
    : [port] "+&e" (port), [byte] "+&r" (b), [bit] "+&r" (bit), [next] "+&r" (next),
      [count] "+&w" (i), [ptr] "+&e" (ptr)
    : [hi] "r" (hi), [lo] "r" (lo));
  sei();
  _delay_us(60);           // latch
}

static void wheel(uint8_t h, uint8_t* r, uint8_t* g, uint8_t* b) {
  if (h < 85)       { *r = 255 - h * 3; *g = h * 3;       *b = 0; }
  else if (h < 170) { h -= 85; *r = 0;  *g = 255 - h * 3; *b = h * 3; }
  else              { h -= 170; *r = h * 3; *g = 0;       *b = 255 - h * 3; }
}

static inline uint8_t scale(uint8_t v, uint8_t s) { return ((uint16_t)v * s) >> 8; }

static void render(const uint8_t* src, uint8_t level) {
  uint8_t s = scale(PIXEL_BRIGHTNESS, level);
  for (uint8_t i = 0; i < sizeof(out); i++) out[i] = scale(src[i], s);
  show();
}

void pixInit() {
  PIN_OUTPUT(PIXEL_PIN);
  PIN_LOW(PIXEL_PIN);
  pixAll(0, 0, 0);
  render(base, 255);
}

void pixSet(uint8_t i, uint8_t r, uint8_t g, uint8_t b) {
  if (i >= PIXEL_COUNT) return;
  base[i * 3] = g; base[i * 3 + 1] = r; base[i * 3 + 2] = b;
  dirty = true;
}

void pixAll(uint8_t r, uint8_t g, uint8_t b) {
  for (uint8_t i = 0; i < PIXEL_COUNT; i++) pixSet(i, r, g, b);
}

void pixHue(uint8_t i, uint8_t hue) {
  uint8_t r, g, b;
  wheel(hue, &r, &g, &b);
  pixSet(i, r, g, b);
}

void pixAllHue(uint8_t hue) {
  for (uint8_t i = 0; i < PIXEL_COUNT; i++) pixHue(i, hue);
}

void fxFlash(uint8_t r, uint8_t g, uint8_t b, uint8_t frames) {
  flashR = r; flashG = g; flashB = b;
  flashFrames = frames;
}

void fxMode(uint8_t m, uint8_t hue) {
  mode = m;
  modeHue = hue;
  dirty = true;
}

void fxUpdate() {
  tick++;
  if (flashFrames) {
    flashFrames--;
    uint8_t tmp[PIXEL_COUNT * 3];
    for (uint8_t i = 0; i < PIXEL_COUNT; i++) { tmp[i * 3] = flashG; tmp[i * 3 + 1] = flashR; tmp[i * 3 + 2] = flashB; }
    render(tmp, 255);
    dirty = true;              // restore base when the flash ends
    return;
  }
  uint8_t tmp[PIXEL_COUNT * 3];
  uint8_t r, g, b;
  switch (mode) {
    case FX_BREATHE: {
      uint8_t t = tick & 127;
      uint8_t level = (t < 64 ? t : 127 - t) * 4;
      wheel(modeHue, &r, &g, &b);
      for (uint8_t i = 0; i < PIXEL_COUNT; i++) { tmp[i * 3] = g; tmp[i * 3 + 1] = r; tmp[i * 3 + 2] = b; }
      render(tmp, level < 12 ? 12 : level);
      return;
    }
    case FX_RAINBOW:
      for (uint8_t i = 0; i < PIXEL_COUNT; i++) {
        wheel(tick * 3 + i * 60, &r, &g, &b);
        tmp[i * 3] = g; tmp[i * 3 + 1] = r; tmp[i * 3 + 2] = b;
      }
      render(tmp, 255);
      return;
    case FX_CHASE: {
      wheel(modeHue, &r, &g, &b);
      uint8_t lit = (tick >> 3) % PIXEL_COUNT;
      for (uint8_t i = 0; i < PIXEL_COUNT; i++) {
        bool on = i == lit;
        tmp[i * 3] = on ? g : 0; tmp[i * 3 + 1] = on ? r : 0; tmp[i * 3 + 2] = on ? b : 0;
      }
      render(tmp, 255);
      return;
    }
    default:
      if (dirty) { dirty = false; render(base, 255); }
  }
}
