/*
  ============================================================
  Kypruino Ultrasonic Note Theremin
  Simplified Student Version
  ============================================================

  What this project does:
  - The ultrasonic sensor measures the distance of your hand.
  - The distance is converted into a musical note.
  - The potentiometer controls the vibrato effect.
  - The buzzer plays the selected note.

  Wiring:

  HC-SR04 Ultrasonic Sensor:
    VCC  -> 5V
    GND  -> GND
    TRIG -> D4
    ECHO -> D5

  Potentiometer:
    OUT -> A0
    +5  -> 5V
    GND -> GND

  Buzzer:
    Built-in Kypruino buzzer -> D9
*/

#include <math.h>

// ============================================================
// Pin settings
// ============================================================

#define TRIG_PIN 4
#define ECHO_PIN 5
#define POT_PIN A0
#define BUZZER_PIN 9

// ============================================================
// Main settings
// ============================================================

// The useful hand distance range.
const float MIN_DISTANCE_CM = 5.0;
const float MAX_DISTANCE_CM = 50.0;

// false = farther hand gives higher notes
// true  = closer hand gives higher notes
const bool CLOSE_IS_HIGH = false;

// The new note must be detected this many times before changing.
// This stops the sound from jumping too much.
const int READINGS_NEEDED_TO_CHANGE_NOTE = 3;

// Vibrato settings.
const int VIBRATO_DEAD_PERCENT = 6;
const bool INVERT_POT = false;
const float VIBRATO_RATE_HZ = 7.5;
const int MAX_VIBRATO_DEPTH_HZ = 45;

// Smoothing values.
// Higher number = smoother but slower response.
const float DISTANCE_SMOOTHING = 0.88;
const float POT_SMOOTHING = 0.85;

// How often the sensors are read.
const unsigned long SENSOR_INTERVAL_MS = 22;

// ============================================================
// Musical notes
// ============================================================

// Chromatic scale from C4 to C6.
// These numbers are note frequencies in Hz.
const int notes[] = {
  262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494,
  523, 554, 587, 622, 659, 698, 740, 784, 831, 880, 932, 988,
  1047
};

const int NOTE_COUNT = sizeof(notes) / sizeof(notes[0]);

// ============================================================
// Variables used while the program runs
// ============================================================

float smoothDistance = 20.0;
float smoothPot = 0;

bool validDistance = false;

int baseFrequency = 440;
int vibratoDepthHz = 0;

unsigned long lastSensorTime = 0;

// ============================================================
// Read distance from ultrasonic sensor
// ============================================================

float readDistanceCM() {
  // Send a short trigger pulse.
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  // Measure how long the echo pin stays HIGH.
  // Timeout prevents the program from waiting forever.
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 12000);

  // If no echo was received, return -1.
  if (duration == 0) {
    return -1;
  }

  // Convert time into distance.
  float distance = duration * 0.0343 / 2.0;

  // Ignore distances outside our useful range.
  if (distance < MIN_DISTANCE_CM || distance > MAX_DISTANCE_CM) {
    return -1;
  }

  return distance;
}

// ============================================================
// Read potentiometer smoothly
// ============================================================

int readAveragePot() {
  long total = 0;

  // Take a few readings and average them.
  // This makes the potentiometer less noisy.
  for (int i = 0; i < 6; i++) {
    total += analogRead(POT_PIN);
    delayMicroseconds(250);
  }

  return total / 6;
}

// ============================================================
// Convert potentiometer value into vibrato strength
// ============================================================

int getVibratoDepth(int potValue) {
  potValue = constrain(potValue, 0, 1023);

  int potPercent = map(potValue, 0, 1023, 0, 100);

  if (INVERT_POT) {
    potPercent = 100 - potPercent;
  }

  // Small potentiometer values give no vibrato.
  if (potPercent < VIBRATO_DEAD_PERCENT) {
    return 0;
  }

  // Convert the remaining potentiometer range into vibrato depth.
  float usablePercent = (float)(potPercent - VIBRATO_DEAD_PERCENT) /
                        (100.0 - VIBRATO_DEAD_PERCENT);

  usablePercent = constrain(usablePercent, 0.0, 1.0);

  return usablePercent * MAX_VIBRATO_DEPTH_HZ;
}

// ============================================================
// Convert distance into a musical note
// ============================================================

int chooseNoteFromDistance(float distanceCM) {
  distanceCM = constrain(distanceCM, MIN_DISTANCE_CM, MAX_DISTANCE_CM);

  // Convert distance into a number between 0 and 1.
  float ratio = (distanceCM - MIN_DISTANCE_CM) /
                (MAX_DISTANCE_CM - MIN_DISTANCE_CM);

  ratio = constrain(ratio, 0.0, 1.0);

  // Reverse the control direction if needed.
  if (CLOSE_IS_HIGH) {
    ratio = 1.0 - ratio;
  }

  // Convert ratio into a note index.
  int detectedNote = round(ratio * (NOTE_COUNT - 1));
  detectedNote = constrain(detectedNote, 0, NOTE_COUNT - 1);

  // These static variables remember their values between function calls.
  static int stableNote = 0;
  static int lastDetectedNote = 0;
  static int sameNoteCounter = 0;
  static bool firstRun = true;

  // On the first run, accept the first detected note immediately.
  if (firstRun) {
    stableNote = detectedNote;
    lastDetectedNote = detectedNote;
    firstRun = false;
  }

  // If the detected note is already the stable note, nothing changes.
  if (detectedNote == stableNote) {
    sameNoteCounter = 0;
  }

  // If a different note is detected, wait until it appears a few times.
  else {
    if (detectedNote == lastDetectedNote) {
      sameNoteCounter++;
    } else {
      sameNoteCounter = 1;
      lastDetectedNote = detectedNote;
    }

    if (sameNoteCounter >= READINGS_NEEDED_TO_CHANGE_NOTE) {
      stableNote = detectedNote;
      sameNoteCounter = 0;
    }
  }

  return notes[stableNote];
}

// ============================================================
// Update sensor values
// ============================================================

void updateSensors() {
  // Read the potentiometer and smooth it.
  int rawPot = readAveragePot();

  smoothPot = POT_SMOOTHING * smoothPot +
              (1.0 - POT_SMOOTHING) * rawPot;

  vibratoDepthHz = getVibratoDepth((int)smoothPot);

  // Read the ultrasonic sensor.
  float distance = readDistanceCM();

  // If the hand is not detected, stop the sound.
  if (distance < 0) {
    validDistance = false;
    noTone(BUZZER_PIN);
    return;
  }

  validDistance = true;

  // Smooth the distance measurement.
  smoothDistance = DISTANCE_SMOOTHING * smoothDistance +
                   (1.0 - DISTANCE_SMOOTHING) * distance;

  smoothDistance = constrain(smoothDistance, MIN_DISTANCE_CM, MAX_DISTANCE_CM);

  // Convert the distance into the base note frequency.
  baseFrequency = chooseNoteFromDistance(smoothDistance);
}

// ============================================================
// Play sound with vibrato
// ============================================================

void updateSound() {
  // Do not play anything if no valid hand distance is detected.
  if (!validDistance) {
    noTone(BUZZER_PIN);
    return;
  }

  // Create a smooth up-down wave for vibrato.
  float timeSeconds = millis() / 1000.0;
  float vibratoWave = sin(2.0 * PI * VIBRATO_RATE_HZ * timeSeconds);

  // Use the potentiometer to control how strong the vibrato is.
  int vibratoOffset = vibratoWave * vibratoDepthHz;

  int finalFrequency = baseFrequency + vibratoOffset;

  // Safety limit.
  if (finalFrequency < 40) {
    finalFrequency = 40;
  }

  tone(BUZZER_PIN, finalFrequency);
}

// ============================================================
// Setup
// ============================================================

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Start smoothing values from real readings.
  smoothDistance = 20.0;
  smoothPot = analogRead(POT_PIN);
}

// ============================================================
// Main loop
// ============================================================

void loop() {
  // Read sensors at a fixed interval.
  if (millis() - lastSensorTime >= SENSOR_INTERVAL_MS) {
    lastSensorTime = millis();
    updateSensors();
  }

  // Update the buzzer continuously so vibrato stays smooth.
  updateSound();
}