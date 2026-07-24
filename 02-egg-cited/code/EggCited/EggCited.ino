/*
  ============================================================
  EggCited — Interactive 3D Printed Easter Egg
  Part of the Kypruino STEM Projects by ROBO (robo.com.cy)
  Guide: https://robo.com.cy/blogs/blog/interactive-3d-printed-easter-egg-kypruino-arduino
  ============================================================

  What this project does:
  - A PIR sensor detects when the egg is found.
  - The onboard NeoPixels flash, then play a smooth pastel
    colour animation.
  - The buzzer plays a short reward sound.
  - The egg then resets and waits to be found again.

  Wiring:
    PIR sensor:  VCC -> 5V, GND -> GND, OUT -> D7
    Built-in Kypruino: NeoPixels -> D8, Buzzer -> D9

  Libraries:
    Adafruit NeoPixel
  ============================================================
*/

#include <Adafruit_NeoPixel.h>

#define PIXELS_PIN 8
#define NUMPIXELS 3
#define BUZZER_PIN 9
#define MOTION_PIN 7

#define WAIT 2000

Adafruit_NeoPixel pixels(NUMPIXELS, PIXELS_PIN, NEO_GRB + NEO_KHZ800);

// Easter pastel colours
uint8_t colors[][3] = {
  {255, 120, 180}, // pink
  {255, 255, 120}, // yellow
  {120, 220, 255}, // light blue
  {180, 255, 180}  // light green
};

int numColors = 4;
int val = LOW;

void setup() {
  Serial.begin(9600);
  pinMode(MOTION_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  pixels.begin();
  pixels.setBrightness(255);
  pixels.clear();
  pixels.show();
}

// Smooth transition with pulse
void smoothTransition(uint8_t r1, uint8_t g1, uint8_t b1,
                      uint8_t r2, uint8_t g2, uint8_t b2,
                      int steps, int delayTime) {

  for (int i = 0; i <= steps; i++) {
    float t = (float)i / steps;

    uint8_t r = r1 + (r2 - r1) * t;
    uint8_t g = g1 + (g2 - g1) * t;
    uint8_t b = b1 + (b2 - b1) * t;

    for (int p = 0; p < NUMPIXELS; p++) {
      pixels.setPixelColor(p, pixels.Color(r, g, b));
    }

    pixels.show();
    delay(delayTime);
  }
}

// Fast pastel sweep
void easterGlowCycle() {
  for (int c = 0; c < numColors; c++) {
    int next = (c + 1) % numColors;

    smoothTransition(
      colors[c][0], colors[c][1], colors[c][2],
      colors[next][0], colors[next][1], colors[next][2],
      25, 8
    );
  }
}

// Custom sound: C4, C4, G4
void playTaDa() {
  int baseDuration = 120;
  int gap = 40; // small silence between notes

  // C6 (2 duration)
  tone(BUZZER_PIN, 1047);
  delay(2 * baseDuration);
  noTone(BUZZER_PIN);
  delay(gap);

  // C6 (1 duration)
  tone(BUZZER_PIN, 1047);
  delay(1 * baseDuration);
  noTone(BUZZER_PIN);
  delay(gap);

  // G6 (3 duration)
  tone(BUZZER_PIN, 1568);
  delay(3 * baseDuration);
  noTone(BUZZER_PIN);
}

void loop() {
  val = digitalRead(MOTION_PIN);

  if (val == HIGH) {
    Serial.println("Egg Found!");

    // flash
    for (int p = 0; p < NUMPIXELS; p++) {
      pixels.setPixelColor(p, pixels.Color(255, 255, 255));
    }
    pixels.setBrightness(255);
    pixels.show();
    delay(80);

    // animation + sound (twice)
    for (int i = 0; i < 2; i++) {
      easterGlowCycle();
      playTaDa();
    }

    pixels.clear();
    pixels.show();
    noTone(BUZZER_PIN);

    delay(WAIT);
  }
}