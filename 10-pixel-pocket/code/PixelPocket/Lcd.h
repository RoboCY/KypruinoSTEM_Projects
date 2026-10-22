/*
  Lcd.h — minimal ST7735 128x160 driver (hardware SPI), used landscape 160x128.
  No framebuffer: everything is streamed straight to the panel, so games
  redraw only what changed.
*/
#pragma once
#include "Config.h"

#define LCD_W 160
#define LCD_H 128

// RGB565 helper (r, g, b are 0–255)
#define RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_RED     0xF800
#define C_GREEN   0x07E0
#define C_BLUE    0x001F
#define C_YELLOW  0xFFE0
#define C_CYAN    0x07FF
#define C_MAGENTA 0xF81F
#define C_ORANGE  0xFC00
#define C_GREY    0x8410
#define C_DGREY   0x4208
#define C_NAVY    0x0010
#define C_DGREEN  0x0320
#define C_BROWN   0x8200

void lcdInit();
void lcdFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c);
void lcdClear(uint16_t c);
void lcdPixel(int16_t x, int16_t y, uint16_t c);
void lcdRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c);   // outline
// 1-bit bitmap from PROGMEM, rows padded to whole bytes, MSB first.
void lcdBitmap(int16_t x, int16_t y, const uint8_t* bits, uint8_t w, uint8_t h, uint16_t fg, uint16_t bg);
// 5x7 font, each char drawn in a (6*size) x (8*size) cell with background.
void lcdChar(int16_t x, int16_t y, char ch, uint16_t fg, uint16_t bg, uint8_t size);
void lcdText(int16_t x, int16_t y, const char* s, uint16_t fg, uint16_t bg, uint8_t size);
void lcdTextP(int16_t x, int16_t y, const char* s, uint16_t fg, uint16_t bg, uint8_t size); // PROGMEM string
// Number right-aligned so its right edge is at xRight (up to 5 digits).
void lcdNumber(int16_t xRight, int16_t y, uint16_t n, uint16_t fg, uint16_t bg, uint8_t size);
// Centre a PROGMEM string horizontally on a row.
void lcdTextCenterP(int16_t y, const char* s, uint16_t fg, uint16_t bg, uint8_t size);
uint16_t lcdHsv(uint8_t h, uint8_t s, uint8_t v); // hue 0–255 → RGB565
