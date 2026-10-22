#include "Oled.h"
#include "Font.h"

bool oledPresent = false;
static uint8_t buf[512];
static bool dirty = false;

// ---------------- tiny blocking TWI master ----------------

static bool twiWait() {
  uint16_t t = 0;
  while (!(TWCR & _BV(TWINT))) {
    if (++t > 4000) return false;   // ~1 ms: bus stuck or nothing connected
  }
  return true;
}

static bool twiStart() {
  TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
  if (!twiWait()) return false;
  TWDR = OLED_ADDR << 1;
  TWCR = _BV(TWINT) | _BV(TWEN);
  if (!twiWait()) return false;
  return (TWSR & 0xF8) == 0x18;     // SLA+W acknowledged
}

static bool twiWrite(uint8_t b) {
  TWDR = b;
  TWCR = _BV(TWINT) | _BV(TWEN);
  return twiWait();
}

static void twiStop() {
  TWCR = _BV(TWINT) | _BV(TWSTO) | _BV(TWEN);
}

static bool oledCmd(uint8_t c) {
  if (!twiStart()) { twiStop(); return false; }
  twiWrite(0x80);
  twiWrite(c);
  twiStop();
  return true;
}

static const uint8_t OLED_INIT[] PROGMEM = {
  0xAE, 0xD5, 0x80, 0xA8, 0x1F, 0xD3, 0x00, 0x40, 0x8D, 0x14, 0x20, 0x00,
  0xA1, 0xC8, 0xDA, 0x02, 0x81, 0x8F, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
};

void oledInit() {
  PORTC |= 0x30;                    // pull-ups on A4/A5 help if the module has none
  TWSR = 0;
  TWBR = 12;                        // 400 kHz at 16 MHz
  oledPresent = true;
  for (uint8_t i = 0; i < sizeof(OLED_INIT); i++) {
    if (!oledCmd(pgm_read_byte(&OLED_INIT[i]))) { oledPresent = false; return; }
  }
  oledClear();
  dirty = true;
  oledFlush();
}

// ---------------- buffer drawing (portrait coordinates) ----------------

void oledClear() {
  memset(buf, 0, sizeof(buf));
  dirty = true;
}

void oledPixel(uint8_t x, uint8_t y, bool on) {
  if (x >= OLED_W || y >= OLED_H) return;
#if OLED_FLIP
  uint8_t nx = 127 - y, ny = x;
#else
  uint8_t nx = y, ny = 31 - x;
#endif
  uint8_t* p = &buf[(ny >> 3) * 128 + nx];
  uint8_t m = 1 << (ny & 7);
  if (on) *p |= m; else *p &= ~m;
  dirty = true;
}

void oledFillRect(int8_t x, int16_t y, uint8_t w, uint8_t h, bool on) {
  for (uint8_t i = 0; i < w; i++)
    for (uint8_t j = 0; j < h; j++)
      if (x + i >= 0 && y + j >= 0) oledPixel(x + i, y + j, on);
}

void oledRect(int8_t x, int16_t y, uint8_t w, uint8_t h) {
  oledFillRect(x, y, w, 1, true);
  oledFillRect(x, y + h - 1, w, 1, true);
  oledFillRect(x, y, 1, h, true);
  oledFillRect(x + w - 1, y, 1, h, true);
}

void oledChar(int8_t x, int16_t y, char ch, uint8_t size) {
  for (uint8_t col = 0; col < 5; col++) {
    uint8_t bits = fontCol(ch, col);
    for (uint8_t row = 0; row < 7; row++) {
      if (bits & (1 << row)) oledFillRect(x + col * size, y + row * size, size, size, true);
    }
  }
}

void oledTextP(int8_t x, int16_t y, const char* s, uint8_t size) {
  char ch;
  while ((ch = pgm_read_byte(s++))) { oledChar(x, y, ch, size); x += 6 * size; }
}

static uint8_t lineLenP(const char* s) {
  uint8_t n = 0;
  char ch;
  while ((ch = pgm_read_byte(s++)) && ch != '\n') n++;
  return n;
}

void oledTextCenterP(int16_t y, const char* s, uint8_t size) {
  uint8_t len = lineLenP(s);
  int8_t x = (OLED_W - (len * 6 - 1) * size) / 2;
  for (uint8_t i = 0; i < len; i++) { oledChar(x, y, pgm_read_byte(s + i), size); x += 6 * size; }
}

void oledLinesP(int16_t y, const char* s, uint8_t size) {
  while (pgm_read_byte(s)) {
    oledTextCenterP(y, s, size);
    y += 8 * size;
    s += lineLenP(s);
    if (pgm_read_byte(s) == '\n') s++;
  }
}

static uint8_t toDigits(char* out, uint16_t n) {
  char tmp[5];
  uint8_t i = 0;
  do { tmp[i++] = '0' + n % 10; n /= 10; } while (n);
  uint8_t len = i;
  for (uint8_t j = 0; j < len; j++) out[j] = tmp[len - 1 - j];
  return len;
}

void oledNumber(int16_t y, uint16_t n, uint8_t size) {
  char d[5];
  uint8_t len = toDigits(d, n);
  int8_t x = (OLED_W - (len * 6 - 1) * size) / 2;
  for (uint8_t i = 0; i < len; i++) { oledChar(x, y, d[i], size); x += 6 * size; }
}

void oledNumberAt(int8_t xRight, int16_t y, uint16_t n, uint8_t size) {
  char d[5];
  uint8_t len = toDigits(d, n);
  int8_t x = xRight - (len * 6 - 1) * size;
  for (uint8_t i = 0; i < len; i++) { oledChar(x, y, d[i], size); x += 6 * size; }
}

void oledVBar(int8_t x, int16_t y, uint8_t w, uint8_t h, uint8_t percent) {
  oledFillRect(x, y, w, h, false);
  oledRect(x, y, w, h);
  if (percent > 100) percent = 100;
  uint8_t fill = ((uint16_t)(h - 2) * percent) / 100;
  oledFillRect(x + 1, y + h - 1 - fill, w - 2, fill, true);
}

static const uint8_t HEART[] PROGMEM = { 0x36, 0x7F, 0x7F, 0x3E, 0x1C, 0x08 }; // 7 wide, 6 rows (MSB left)

void oledHearts(int16_t y, uint8_t n, uint8_t max) {
  int8_t x = (OLED_W - max * 8) / 2 + 1;
  for (uint8_t i = 0; i < max; i++) {
    for (uint8_t r = 0; r < 6; r++) {
      uint8_t b = pgm_read_byte(&HEART[r]);
      for (uint8_t c = 0; c < 7; c++) {
        bool on = (b & (0x40 >> c)) && (i < n || r == 0 || r == 5 || c == 0 || c == 6);
        if (b & (0x40 >> c)) oledPixel(x + c, y + r, on);
      }
    }
    x += 8;
  }
}

// ---------------- flush ----------------

void oledFlush() {
  if (!dirty || !oledPresent) return;
  dirty = false;
  oledCmd(0x21); oledCmd(0); oledCmd(127);   // column range
  oledCmd(0x22); oledCmd(0); oledCmd(3);     // page range
  if (!twiStart()) { twiStop(); return; }
  twiWrite(0x40);
  for (uint16_t i = 0; i < 512; i++) twiWrite(buf[i]);
  twiStop();
}
