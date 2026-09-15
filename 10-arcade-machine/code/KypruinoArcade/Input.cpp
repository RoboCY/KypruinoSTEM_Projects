#include "Input.h"
#include "Sound.h"
#include "Pixels.h"

uint8_t btnHeld = 0, btnPressed = 0, btnRepeat = 0;
static uint8_t holdFrames = 0;

void inputInit() {
  PIN_PULLUP(BTN_UP_PIN);
  PIN_PULLUP(BTN_DOWN_PIN);
  PIN_PULLUP(BTN_LEFT_PIN);
  PIN_PULLUP(BTN_RIGHT_PIN);
}

void inputUpdate() {
  uint8_t now = 0;
  if (!PIN_READ(BTN_UP_PIN))    now |= BTN_UP;
  if (!PIN_READ(BTN_DOWN_PIN))  now |= BTN_DOWN;
  if (!PIN_READ(BTN_LEFT_PIN))  now |= BTN_LEFT;
  if (!PIN_READ(BTN_RIGHT_PIN)) now |= BTN_RIGHT;
  btnPressed = now & ~btnHeld;
  btnRepeat = btnPressed;
  if (now && now == btnHeld) {
    holdFrames++;
    if (holdFrames > 14 && (holdFrames & 3) == 0) btnRepeat = now;   // repeat every 4 frames after 350 ms
  } else {
    holdFrames = 0;
  }
  btnHeld = now;
}

uint8_t inputWaitPress() {
  for (;;) {
    inputUpdate();
    soundUpdate();
    fxUpdate();
    if (btnPressed) return btnPressed;
    waitMs(FRAME_MS);
  }
}

void waitMs(uint16_t ms) {
  unsigned long t = millis();
  while (millis() - t < ms) {}
}
