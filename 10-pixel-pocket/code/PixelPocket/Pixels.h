/*
  Pixels.h — the 3 onboard NeoPixels (WS2812 on D8) with a small effects layer.
  Games set a "base" colour per pixel; short flashes and animated modes are
  layered on top by fxUpdate(), which the engine calls every frame.
*/
#pragma once
#include "Config.h"

enum { FX_HOLD = 0, FX_BREATHE, FX_RAINBOW, FX_CHASE };

void pixInit();
void pixSet(uint8_t i, uint8_t r, uint8_t g, uint8_t b);   // base colour of one pixel
void pixAll(uint8_t r, uint8_t g, uint8_t b);              // base colour of all
void pixHue(uint8_t i, uint8_t hue);                        // base colour from hue 0–255
void pixAllHue(uint8_t hue);
void fxFlash(uint8_t r, uint8_t g, uint8_t b, uint8_t frames); // override all pixels briefly
void fxMode(uint8_t mode, uint8_t hue);                     // FX_BREATHE / FX_CHASE use hue
void fxUpdate();                                            // per frame
