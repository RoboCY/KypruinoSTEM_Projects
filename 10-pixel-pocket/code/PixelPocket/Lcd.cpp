#include "Lcd.h"
#include "Font.h"

// ---------------- low level SPI ----------------

static inline void spiWrite(uint8_t b) {
  SPDR = b;
  while (!(SPSR & _BV(SPIF))) {}
}

static inline void csLow()  { PIN_LOW(LCD_CS_PIN); }
static inline void csHigh() { PIN_HIGH(LCD_CS_PIN); }

static void cmd(uint8_t c) {
  PIN_LOW(LCD_DC_PIN);
  spiWrite(c);
  PIN_HIGH(LCD_DC_PIN);
}

// Init table: command, count (bit 7 = delay follows), args..., [delay ms]
static const uint8_t INIT[] PROGMEM = {
  0x01, 0x80, 150,                 // SWRESET + 150 ms
  0x11, 0x80, 255,                 // SLPOUT + 255 ms
  0xB1, 3, 0x01, 0x2C, 0x2D,       // FRMCTR1
  0xB2, 3, 0x01, 0x2C, 0x2D,       // FRMCTR2
  0xB3, 6, 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D, // FRMCTR3
  0xB4, 1, 0x07,                   // INVCTR
  0xC0, 3, 0xA2, 0x02, 0x84,       // PWCTR1
  0xC1, 1, 0xC5,                   // PWCTR2
  0xC2, 2, 0x0A, 0x00,             // PWCTR3
  0xC3, 2, 0x8A, 0x2A,             // PWCTR4
  0xC4, 2, 0x8A, 0xEE,             // PWCTR5
  0xC5, 1, 0x0E,                   // VMCTR1
  0x20, 0,                         // INVOFF
  0x3A, 1, 0x05,                   // COLMOD 16 bit
  0xE0, 16, 0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d,
            0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10, // GMCTRP1
  0xE1, 16, 0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
            0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10, // GMCTRN1
  0x13, 0x80, 10,                  // NORON
  0x29, 0x80, 100,                 // DISPON
  0x00                             // end
};

#if LCD_ROTATION == 3
  #define MADCTL_VAL (0x80 | 0x20 | (LCD_BGR ? 0x08 : 0))
#else
  #define MADCTL_VAL (0x40 | 0x20 | (LCD_BGR ? 0x08 : 0))
#endif
// In landscape the panel's column offset applies to our Y axis and vice versa
#define X_OFF LCD_ROW_OFFSET
#define Y_OFF LCD_COL_OFFSET

void lcdInit() {
  PIN_OUTPUT(LCD_CS_PIN);
  PIN_OUTPUT(LCD_DC_PIN);
  PIN_OUTPUT(LCD_RST_PIN);
  PIN_OUTPUT(13);
  PIN_OUTPUT(11);
  PIN_OUTPUT(10);                 // SS must be an output for SPI master mode
  csHigh();
  SPCR = _BV(SPE) | _BV(MSTR);    // mode 0, MSB first, F_CPU/4
  SPSR = _BV(SPI2X);              // → F_CPU/2 = 8 MHz

  PIN_HIGH(LCD_RST_PIN); waitMs(5);
  PIN_LOW(LCD_RST_PIN);  waitMs(20);
  PIN_HIGH(LCD_RST_PIN); waitMs(150);

  csLow();
  const uint8_t* p = INIT;
  uint8_t c;
  while ((c = pgm_read_byte(p++)) != 0) {
    uint8_t n = pgm_read_byte(p++);
    bool hasDelay = n & 0x80;
    n &= 0x7F;
    cmd(c);
    while (n--) spiWrite(pgm_read_byte(p++));
    if (hasDelay) {                 // a delay byte follows when bit 7 was set
      uint8_t ms = pgm_read_byte(p++);
      waitMs(ms == 255 ? 500 : ms);
    }
  }
  cmd(0x36); spiWrite(MADCTL_VAL);   // orientation
  csHigh();
  lcdClear(C_BLACK);
}

static void setWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
  cmd(0x2A); spiWrite(0); spiWrite(x0 + X_OFF); spiWrite(0); spiWrite(x1 + X_OFF);
  cmd(0x2B); spiWrite(0); spiWrite(y0 + Y_OFF); spiWrite(0); spiWrite(y1 + Y_OFF);
  cmd(0x2C);
}

static inline void pushColor(uint16_t c) {
  spiWrite(c >> 8);
  spiWrite(c);
}

// ---------------- drawing ----------------

void lcdFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > LCD_W) w = LCD_W - x;
  if (y + h > LCD_H) h = LCD_H - y;
  if (w <= 0 || h <= 0) return;
  csLow();
  setWindow(x, y, x + w - 1, y + h - 1);
  uint16_t n = (uint16_t)w * h;
  uint8_t hi = c >> 8, lo = c;
  while (n--) {
    SPDR = hi; while (!(SPSR & _BV(SPIF))) {}
    SPDR = lo; while (!(SPSR & _BV(SPIF))) {}
  }
  csHigh();
}

void lcdClear(uint16_t c) { lcdFillRect(0, 0, LCD_W, LCD_H, c); }

void lcdPixel(int16_t x, int16_t y, uint16_t c) { lcdFillRect(x, y, 1, 1, c); }

void lcdRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
  lcdFillRect(x, y, w, 1, c);
  lcdFillRect(x, y + h - 1, w, 1, c);
  lcdFillRect(x, y, 1, h, c);
  lcdFillRect(x + w - 1, y, 1, h, c);
}

void lcdBitmap(int16_t x, int16_t y, const uint8_t* bits, uint8_t w, uint8_t h, uint16_t fg, uint16_t bg) {
  if (x < 0 || y < 0 || x + w > LCD_W || y + h > LCD_H) return;   // no clipping for sprites
  uint8_t bpr = (w + 7) >> 3;
  csLow();
  setWindow(x, y, x + w - 1, y + h - 1);
  for (uint8_t r = 0; r < h; r++) {
    const uint8_t* row = bits + r * bpr;
    uint8_t b = 0;
    for (uint8_t cx = 0; cx < w; cx++) {
      if ((cx & 7) == 0) b = pgm_read_byte(row + (cx >> 3));
      pushColor((b & 0x80) ? fg : bg);
      b <<= 1;
    }
  }
  csHigh();
}

void lcdChar(int16_t x, int16_t y, char ch, uint16_t fg, uint16_t bg, uint8_t size) {
  uint8_t cw = 6 * size, chh = 8 * size;
  if (x < 0 || y < 0 || x + cw > LCD_W || y + chh > LCD_H) return;
  uint8_t cols[5];
  for (uint8_t i = 0; i < 5; i++) cols[i] = fontCol(ch, i);
  csLow();
  setWindow(x, y, x + cw - 1, y + chh - 1);
  for (uint8_t row = 0; row < 8; row++) {
    uint8_t mask = 1 << row;
    for (uint8_t rep = 0; rep < size; rep++) {
      for (uint8_t col = 0; col < 6; col++) {
        uint16_t c = (col < 5 && (cols[col] & mask)) ? fg : bg;
        for (uint8_t s = 0; s < size; s++) pushColor(c);
      }
    }
  }
  csHigh();
}

void lcdText(int16_t x, int16_t y, const char* s, uint16_t fg, uint16_t bg, uint8_t size) {
  while (*s) { lcdChar(x, y, *s++, fg, bg, size); x += 6 * size; }
}

void lcdTextP(int16_t x, int16_t y, const char* s, uint16_t fg, uint16_t bg, uint8_t size) {
  char ch;
  while ((ch = pgm_read_byte(s++))) { lcdChar(x, y, ch, fg, bg, size); x += 6 * size; }
}

void lcdTextCenterP(int16_t y, const char* s, uint16_t fg, uint16_t bg, uint8_t size) {
  uint8_t len = strlen_P(s);
  lcdTextP((LCD_W - len * 6 * size) / 2, y, s, fg, bg, size);
}

void lcdNumber(int16_t xRight, int16_t y, uint16_t n, uint16_t fg, uint16_t bg, uint8_t size) {
  char buf[6];
  uint8_t i = 5;
  buf[i] = 0;
  do { buf[--i] = '0' + n % 10; n /= 10; } while (n && i);
  lcdText(xRight - (5 - i) * 6 * size, y, buf + i, fg, bg, size);
}

uint16_t lcdHsv(uint8_t h, uint8_t s, uint8_t v) {
  uint8_t region = h / 43;
  uint8_t rem = (h - region * 43) * 6;
  uint8_t p = ((uint16_t)v * (255 - s)) >> 8;
  uint8_t q = ((uint16_t)v * (255 - (((uint16_t)s * rem) >> 8))) >> 8;
  uint8_t t = ((uint16_t)v * (255 - (((uint16_t)s * (255 - rem)) >> 8))) >> 8;
  uint8_t r, g, b;
  switch (region) {
    case 0:  r = v; g = t; b = p; break;
    case 1:  r = q; g = v; b = p; break;
    case 2:  r = p; g = v; b = t; break;
    case 3:  r = p; g = q; b = v; break;
    case 4:  r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
  }
  return RGB(r, g, b);
}
