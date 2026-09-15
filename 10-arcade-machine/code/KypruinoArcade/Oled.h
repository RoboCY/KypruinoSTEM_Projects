/*
  Oled.h — 128x32 SSD1306 over I2C, mounted PORTRAIT: 32 pixels wide, 128 tall.
  Uses a 512-byte framebuffer and its own tiny I2C master (no Wire library).
  Drawing only touches the buffer; oledFlush() sends it when something changed.
  Everything still works if the OLED is not plugged in.
*/
#pragma once
#include "Config.h"

#define OLED_W 32
#define OLED_H 128

extern bool oledPresent;

void oledInit();
void oledClear();
void oledPixel(uint8_t x, uint8_t y, bool on);
void oledFillRect(int8_t x, int16_t y, uint8_t w, uint8_t h, bool on);
void oledRect(int8_t x, int16_t y, uint8_t w, uint8_t h);               // outline
void oledChar(int8_t x, int16_t y, char ch, uint8_t size);               // sets pixels only
void oledTextP(int8_t x, int16_t y, const char* s, uint8_t size);        // PROGMEM string
void oledTextCenterP(int16_t y, const char* s, uint8_t size);            // one line, centred
void oledLinesP(int16_t y, const char* s, uint8_t size);                 // '\n' separated lines, centred
void oledNumber(int16_t y, uint16_t n, uint8_t size);                    // centred number
void oledNumberAt(int8_t xRight, int16_t y, uint16_t n, uint8_t size);   // right-aligned number
void oledVBar(int8_t x, int16_t y, uint8_t w, uint8_t h, uint8_t percent); // gauge filled from bottom
void oledHearts(int16_t y, uint8_t n, uint8_t max);                      // row of hearts (max 4)
void oledFlush();                                                        // send if dirty
