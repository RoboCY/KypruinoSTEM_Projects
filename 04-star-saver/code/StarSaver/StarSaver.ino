/*
  ============================================================
  StarSaver — Smart Street Light Model for Dark Skies
  Part of the Kypruino STEM Projects by ROBO (robo.com.cy)
  Guide: https://robo.com.cy/blogs/blog/smart-street-light-model-dark-skies-kypruino
  ============================================================

  A planetarium demonstration model for light-pollution awareness,
  built for DarkSky Week: "a home under the stars".

  What this project does:
  - An LDR module on the roof measures how much light is around.
  - When it gets dark, a downward-facing warm white street lamp
    switches on outside the house.
  - At the same time the onboard NeoPixels light the house interior
    with a dim warm glow, as if someone is home.
  - When the light comes back, everything switches off again.

  Wiring:
    LDR module:  VCC -> 5V, GND -> GND, SIG -> A0
    Street lamp: LED anode -> D10 via series resistor, cathode -> GND
    Built-in Kypruino: NeoPixels -> D8

  Libraries:
    Adafruit NeoPixel

  TUNING:
  Open the Serial Monitor at 9600 baud to watch the live LDR value,
  then set darkThreshold between your "room lit" and "room dark"
  readings. Some LDR modules read higher in the dark instead of
  lower — if the lamp behaves backwards, flip the comparison in
  loop() from < to >.
  ============================================================
*/

#include <Adafruit_NeoPixel.h>

// -------------------- Pin Settings --------------------

const int LDR_PIN  = A0;   // LDR module analog output
const int LAMP_PIN = 10;   // Warm white street lamp LED

// Kypruino onboard NeoPixels (house interior)
const int PIXEL_PIN   = 8;
const int PIXEL_COUNT = 3;

Adafruit_NeoPixel pixels(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

// -------------------- Street Lamp Settings --------------------

// Brightness of the warm white street lamp
// 0 = off, 255 = full brightness
const int LAMP_BRIGHTNESS = 255;

// -------------------- House Glow Settings --------------------

// Dim warm interior glow, kept low so the street lamp stays the
// star of the demonstration
const int HOUSE_R = 35;
const int HOUSE_G = 28;
const int HOUSE_B = 18;

// -------------------- Darkness Threshold --------------------

// Below this reading the model treats the room as dark.
// Adjust after testing — see TUNING above.
int darkThreshold = 500;

// -------------------- Setup --------------------

void setup() {
  pinMode(LAMP_PIN, OUTPUT);

  pixels.begin();

  turnEverythingOff();

  Serial.begin(9600);
  Serial.println("StarSaver Started");
  Serial.print("Dark threshold: ");
  Serial.println(darkThreshold);
}

// -------------------- Main Loop --------------------

void loop() {
  int ldrValue = analogRead(LDR_PIN);

  // Assumes lower value = darker
  if (ldrValue < darkThreshold) {
    turnOnLamp();
    setHousePixels(HOUSE_R, HOUSE_G, HOUSE_B);
  } else {
    turnEverythingOff();
  }

  // Handy while tuning darkThreshold
  Serial.print("LDR: ");
  Serial.print(ldrValue);
  Serial.print("  Lamp: ");
  Serial.println(ldrValue < darkThreshold ? "ON" : "OFF");

  delay(100);
}

// -------------------- Street Lamp Helpers --------------------

void turnOnLamp() {
  analogWrite(LAMP_PIN, LAMP_BRIGHTNESS);
}

void turnOffLamp() {
  analogWrite(LAMP_PIN, 0);
}

// -------------------- House Glow Helpers --------------------

void setHousePixels(int r, int g, int b) {
  for (int i = 0; i < PIXEL_COUNT; i++) {
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
  pixels.show();
}

void turnOffHousePixels() {
  pixels.clear();
  pixels.show();
}

// -------------------- Everything Off --------------------

void turnEverythingOff() {
  turnOffLamp();
  turnOffHousePixels();
}
