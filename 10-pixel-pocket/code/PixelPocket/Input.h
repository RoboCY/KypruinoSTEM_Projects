/*
  Input.h — the four onboard buttons read once per frame.
  btnHeld    : bits of buttons currently down
  btnPressed : bits that went down this frame
  btnRepeat  : pressed, plus auto-repeat while held (for menus and Tetris)
*/
#pragma once
#include "Config.h"

#define BTN_UP    1
#define BTN_DOWN  2
#define BTN_LEFT  4
#define BTN_RIGHT 8
#define BTN_ANY   15

extern uint8_t btnHeld, btnPressed, btnRepeat;

void inputInit();
void inputUpdate();
uint8_t inputWaitPress();     // blocks (keeping sound/pixels alive) until a button is pressed; returns it
