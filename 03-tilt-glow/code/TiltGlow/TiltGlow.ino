/*
 * ============================================================
 *  TiltGlow — MagicMotion DIY Mood Lamp
 *  Part of the Kypruino STEM Projects by ROBO (robo.com.cy)
 *  Guide: https://robo.com.cy/blogs/blog/magicmotion-diy-mood-lamp-kypruino-neopixel
 *
 *  Hardware: Kypruino (Arduino UNO compatible) + MPU6050 motion
 *  sensor + NeoPixel ring (pin D8)
 * ============================================================
 *
 *  WHAT THIS PROJECT DOES
 *  -------------------------------------------------------------
 *  This lamp sits on a table and reacts to how you move it:
 *
 *    • SHAKE it   -> the colour changes to the next one in the list
 *    • FLIP it    -> the lamp immediately starts fading out,
 *                    like it's "going to sleep"
 *    • TURN it upright again -> it immediately starts fading back on
 *    • TILT it    -> the brightness goes up or down, like a dimmer switch
 *    • Leave it alone -> each colour has its own gentle animation
 *
 *  Think of it as a lamp with moods: six colours, each with its own
 *  personality, and simple physical gestures instead of buttons.
 *
 *  WIRING
 *  -------------------------------------------------------------
 *  MPU6050 (motion sensor):  SDA -> A4   SCL -> A5   VCC -> 5V   GND -> GND
 *  NeoPixel (LED ring):      DIN -> D8   VCC -> 5V   GND -> GND
 *
 *  LIBRARY NEEDED (install via Arduino Library Manager):
 *  "Adafruit NeoPixel"
 *
 *  IMPORTANT — READ BEFORE POWERING ON
 *  -------------------------------------------------------------
 *  Keep the lamp sitting in its normal, upright position the moment
 *  the Arduino is powered on / reset. During the first couple of
 *  seconds, the code measures gravity and "remembers" this position
 *  as neutral/upright. Every gesture (tilt, flip, shake) is measured
 *  relative to that starting position, so if the lamp is crooked when
 *  it boots up, its idea of "upright" will be crooked too.
 *
 *  THE SIX COLOURS AND THEIR IDLE ANIMATIONS
 *  (an "idle animation" is what plays when you're not actively
 *  shaking, flipping, or tilting the lamp — brightness is untouched,
 *  only colours/patterns move)
 *  -------------------------------------------------------------
 *  White  — a warm, creamy-white dot slowly sweeps around the ring
 *  Yellow — random LEDs briefly flash orange, like little sparks
 *  Green  — a green/teal wave ripples smoothly around the ring
 *  Blue   — every LED gently shimmers between deep blue and cyan
 *  Red    — a soft double "heartbeat" pulse, then a short rest
 *  Purple — each LED drifts on its own between purple and violet-blue
 * ============================================================
 */

#include <Wire.h>              // lets the Arduino talk to the motion sensor
#include <Adafruit_NeoPixel.h> // controls the ring of colour LEDs
#include <math.h>              // gives us sin(), sqrt(), etc. for smooth animations

// ── NeoPixel (LED ring) settings ─────────────────────────────
#define NEO_PIN    8    // the LED ring's data wire is connected to Arduino pin 8
#define NEO_COUNT  12   // how many individual LEDs are on the ring
Adafruit_NeoPixel strip(NEO_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);

// ── MPU6050 (motion sensor) settings ─────────────────────────
// These are technical addresses required by the sensor's internal
// memory map — you don't need to change these.
#define MPU_ADDR      0x68     // the sensor's address on the I2C bus
#define PWR_MGMT_1    0x6B     // register used to "wake up" the sensor
#define ACCEL_XOUT_H  0x3B     // register where motion (acceleration) data starts
#define ACCEL_SCALE   16384.0  // divides raw sensor numbers into "g-force" units

// ── SHAKE detection settings ─────────────────────────────────
// "Shake" is detected as a sudden, sharp motion (extra g-force on
// top of normal gravity).
#define SHAKE_THRESHOLD  1.3   // how hard a shake needs to be to count (in g)
#define SHAKE_DEBOUNCE   700   // wait this many ms before allowing another shake
                               // (stops one shake being counted as many)

// ── FLIP detection settings ──────────────────────────────────
// "Flip" means the lamp has been turned upside down; "upright" means
// it's back the right way up. Both are measured by comparing the
// current orientation to the one recorded at start-up.
#define FLIP_DOT_THRESHOLD    -0.35  // below this = lamp is considered flipped
#define UPRIGHT_DOT_THRESHOLD  0.60  // above this = lamp is considered upright again

// ── TILT detection settings ──────────────────────────────────
// These numbers aren't currently used to trigger anything directly,
// but are calculated every loop and printed for reference/tuning.
#define TILT_MIN_CHANGE  0.45
#define TILT_MAX_CHANGE  1.70

// Brightness control only kicks in when the lamp is tilted to roughly
// a 35°-65° angle away from upright — think of it as a "sweet spot"
// zone where tilting acts like a dimmer switch.
#define TILT_MIN_ANGLE_DEG  35.0
#define TILT_MAX_ANGLE_DEG  65.0
#define MAX_BRIGHT       255.0   // fully bright
#define MIN_BRIGHT         0.0   // fully off
#define BRIGHTNESS_DROP_PER_MS  0.051  // how fast brightness fades, per millisecond

// ── Colour transition settings ───────────────────────────────
#define TRANSITION_SPEED  0.0015  // how quickly one colour blends into the next
                                
// ── Variables that track the lamp's current state ────────────
// colourState: which of the 6 colours is currently active
// (0=white 1=yellow 2=green 3=blue 4=red 5=purple)
int           colourState    = 0;

float         brightness     = MAX_BRIGHT;  // current brightness level (0-255)
float         brightnessDir  = -1.0;        // +1 = getting brighter, -1 = getting dimmer
bool          lampOn         = true;        // is the lamp currently lit at all?

unsigned long lastShake      = 0;  // timestamp of the last accepted shake
unsigned long lastLoop       = 0;  // timestamp of the previous loop, used to time animations

bool          waking          = false;  // true while the lamp is fading back on
float         wakeBrightness  = 0.0;    // brightness level while fading on

bool          fadingOff        = false;      // true while fading out after a flip
bool          flippedOffLocked = false;      // true once fully faded off — stays off
                                              // until the lamp is turned upright again
float         fadeOffBright    = MAX_BRIGHT; // brightness level while fading out

// Colour blending — the ring smoothly fades from one colour ("from")
// to the next ("to") rather than snapping instantly.
float         fromR = 255.0, fromG = 255.0, fromB = 255.0;
float         toR   = 255.0, toG   = 255.0, toB   = 255.0;
float         transitionProgress = 1.0;  // 0 = just started blending, 1 = blend finished

// start-up (see calibrateRestPosition below). 
float restX = 0.0, restY = 0.0, restZ = 1.0;       // normalised (for direction only)
float rawRestX = 0.0, rawRestY = 0.0, rawRestZ = 1.0; // raw (for measuring tilt amount)

// ── Variables used purely for the idle animations ────────────
unsigned long animTimer    = 0;    // general-purpose animation clock (currently unused directly)
float         animPhase    = 0.0;  // a continuously increasing number that drives
                                    // smooth wave-like motion in several animations
unsigned long animStartTime[6] = {0,0,0,0,0,0};  // records when each colour became active,
                                                  // so we can pause 1s before its animation starts

// White animation: one bright dot travels around the ring
int   sweepPos    = 0;
unsigned long sweepLast = 0;

// Yellow animation: random little "spark" flashes
int   lightningPixel  = -1;   // which LED is currently flashing (-1 = none)
unsigned long lightningOnAt  = 0;
unsigned long lightningNext  = 0;

// Red animation: a two-beat "heartbeat" pulse
int   heartStage  = 0;   // 0=resting 1=first beat 2=gap 3=second beat 4=resting
unsigned long heartTimer = 0;

// Purple animation: every LED drifts at its own random pace,
// so the whole ring never looks perfectly synchronised
float nebulaOffset[NEO_COUNT];

// ═════════════════════════════════════════════════════════════
//  SENSOR READING — talking to the motion sensor (MPU6050)
// ═════════════════════════════════════════════════════════════

// Reads the raw tilt/motion values from the sensor and converts them
// into "g" units (1.0 = normal gravity pulling straight down).
void readAccel(float &ax, float &ay, float &az) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);
  int16_t rx = (Wire.read() << 8) | Wire.read();
  int16_t ry = (Wire.read() << 8) | Wire.read();
  int16_t rz = (Wire.read() << 8) | Wire.read();
  ax = (float)rx / ACCEL_SCALE;
  ay = (float)ry / ACCEL_SCALE;
  az = (float)rz / ACCEL_SCALE;
}

// Runs once at start-up: takes 80 quick readings while the lamp sits
// still and upright, then averages them to learn what "upright" really is for this lamp.
void calibrateRestPosition() {
  float sx = 0.0, sy = 0.0, sz = 0.0;
  for (int i = 0; i < 80; i++) {
    float ax, ay, az;
    readAccel(ax, ay, az);
    sx += ax; sy += ay; sz += az;
    delay(10);
  }
  rawRestX = sx / 80.0;
  rawRestY = sy / 80.0;
  rawRestZ = sz / 80.0;
  restX = rawRestX; restY = rawRestY; restZ = rawRestZ;
  // Normalise into a pure direction (length 1) so it can be compared
  // with the current orientation using simple maths further down.
  float mag = sqrt(restX*restX + restY*restY + restZ*restZ);
  if (mag > 0.1) { restX /= mag; restY /= mag; restZ /= mag; }
  Serial.println("Calibration done.");
}

// Compares the current orientation to the learned "upright" direction.
// Returns a value from -1 to 1:
//   1  = pointing exactly the same way as upright.
//   0  = tilted 90° away from upright (on its side)
//  -1  = pointing exactly the opposite way (upside down)
// This number checks rotation.
float getOrientationDot(float ax, float ay, float az) {
  float mag = sqrt(ax*ax + ay*ay + az*az);
  if (mag < 0.1) return 1.0;
  return (ax/mag * restX) + (ay/mag * restY) + (az/mag * restZ);
}

// Measures how much "extra" force is being felt beyond plain gravity 
// Sitting still gives a value near 0; a sharp shake gives a spike.
float getDynamicG(float ax, float ay, float az) {
  float grav = (ax*restX) + (ay*restY) + (az*restZ);
  float dx = ax - grav*restX;
  float dy = ay - grav*restY;
  float dz = az - grav*restZ;
  return sqrt(dx*dx + dy*dy + dz*dz);
}

// Measures how far the current reading has drifted from the very first
// resting reading. Used only for the Serial debug
// print below, to help with tuning the tilt settings.
float getTiltChange(float ax, float ay, float az) {
  float dx = ax - rawRestX;
  float dy = ay - rawRestY;
  float dz = az - rawRestZ;
  return sqrt(dx*dx + dy*dy + dz*dz);
}

// ═════════════════════════════════════════════════════════════
//  COLOUR HELPERS
// ═════════════════════════════════════════════════════════════

// Looks up the plain red/green/blue values for a given colour
// slot (0-5). These are the "pure" colours each mode blends around.
void getColourRGB(int state, float &r, float &g, float &b) {
  switch (state) {
    case 0: r=255; g=255; b=255; break;  // White
    case 1: r=255; g=255; b=0;   break;  // Yellow
    case 2: r=0;   g=255; b=0;   break;  // Green
    case 3: r=0;   g=0;   b=255; break;  // Blue
    case 4: r=255; g=0;   b=0;   break;  // Red
    default:r=160; g=0;   b=255; break;  // Purple
  }
}

// Takes a colour and dims it, then converts it into the format
// the NeoPixel library expects. Used by animations that set each
// LED individually.
uint32_t applyBrightness(uint8_t r, uint8_t g, uint8_t b) {
  float s = brightness / 255.0;
  if (s < 0.0) s = 0.0;
  if (s > 1.0) s = 1.0;
  return strip.Color((uint8_t)(r*s), (uint8_t)(g*s), (uint8_t)(b*s));
}

// Sets every single LED on the ring to the same colour and brightness,
// then pushes that update out to the physical LEDs.
void setStrip(uint8_t r, uint8_t g, uint8_t b, float bright) {
  float s = bright / 255.0;
  if (s < 0.0) s = 0.0; if (s > 1.0) s = 1.0;
  uint32_t c = strip.Color((uint8_t)(r*s),(uint8_t)(g*s),(uint8_t)(b*s));
  for (int i = 0; i < strip.numPixels(); i++) strip.setPixelColor(i, c);
  strip.show();
}

// Draws the ring as one flat colour, partway blended between the
// previous colour and the new one.
// Used while a colour change is still fading in, and whenever the
// lamp is off (in which case it just stays off).
void renderColour() {
  if (!lampOn) { setStrip(0, 0, 0, 0); return; }
  float t = transitionProgress;
  float r = fromR + (toR - fromR) * t;
  float g = fromG + (toG - fromG) * t;
  float b = fromB + (toB - fromB) * t;
  setStrip((uint8_t)r, (uint8_t)g, (uint8_t)b, brightness);
}

// ═════════════════════════════════════════════════════════════
//  IDLE ANIMATIONS
//  Each of these plays continuously while the lamp is just idle.
//  They only ever change COLOUR/pattern — never brightness, so the
//  dimmer effect from tilting is always able to work.
// ═════════════════════════════════════════════════════════════

// 0 — WHITE MODE: a single warm-cream dot travels around the ring,
void animWhite(unsigned long now) {
  if (now - sweepLast > 80) {   // move to the next LED roughly every 80ms
    sweepLast = now;            // (a full lap takes about 1 second)
    sweepPos = (sweepPos + 1) % NEO_COUNT;
  }
  for (int i = 0; i < NEO_COUNT; i++) {
    if (i == sweepPos) {
      strip.setPixelColor(i, applyBrightness(255, 230, 180));  // warm cream dot
    } else {
      strip.setPixelColor(i, applyBrightness(255, 255, 255));  // plain white
    }
  }
  strip.show();
}

// 1 — YELLOW MODE: random LEDs briefly "spark" orange, then instantly
// return to yellow, like little flickers of static electricity.
void animYellow(unsigned long now) {
  // If a spark has been showing for 80ms, turn it off again.
  if (lightningPixel >= 0 && now - lightningOnAt > 80) {
    lightningPixel = -1;
  }
  // If there's no spark showing right now, and it's time for a new one,
  // pick a random LED to flash next.
  if (lightningPixel < 0 && now > lightningNext) {
    lightningPixel = random(NEO_COUNT);
    lightningOnAt  = now;
    lightningNext  = now + random(150, 600);  // next spark in 150-600ms
  }
  for (int i = 0; i < NEO_COUNT; i++) {
    if (i == lightningPixel) {
      strip.setPixelColor(i, applyBrightness(255, 140, 0));  // orange spark
    } else {
      strip.setPixelColor(i, applyBrightness(255, 255, 0));  // plain yellow
    }
  }
  strip.show();
}

// 2 — GREEN MODE: a smooth wave of colour ripples continuously around
// the ring, blending gently between green and teal.
void animGreen(unsigned long now) {
  animPhase += 0.003 * 30.0;  // advance the wave (tuned assuming ~30ms per loop)
  if (animPhase > TWO_PI) animPhase -= TWO_PI;  // keep the number from growing forever
  for (int i = 0; i < NEO_COUNT; i++) {
    // Each LED's position around the ring gets its own point on the
    // wave, so the colour appears to travel around the circle.
    float wave = (sin(animPhase + (TWO_PI * i / NEO_COUNT)) + 1.0) / 2.0;
    uint8_t g = 255;
    uint8_t r = 0;
    uint8_t b = (uint8_t)(160.0 * wave);   // blends toward teal as "wave" rises
    strip.setPixelColor(i, applyBrightness(r, g, b));
  }
  strip.show();
}

// 3 — BLUE MODE: every LED gently shimmers between deep blue and cyan,
// each one slightly out of step with its neighbours.
void animBlue(unsigned long now) {
  for (int i = 0; i < NEO_COUNT; i++) {
    // Offsetting the phase by each LED's position.
    float phase = animPhase + (i * 0.8);
    float shimmer = (sin(phase) + 1.0) / 2.0;
    uint8_t g = (uint8_t)(180.0 * shimmer);  // blends toward cyan as shimmer rises
    strip.setPixelColor(i, applyBrightness(0, g, 255));
  }
  animPhase += 0.002 * 30.0;
  if (animPhase > TWO_PI * 10) animPhase = 0;  // reset occasionally to avoid huge numbers
  strip.show();
}

// 4 — RED MODE: Two gentle pulses close together,
// then a longer pause before repeating.
void animRed(unsigned long now) {
  animPhase += 0.0018 * 30.0;
  if (animPhase > TWO_PI * 10) animPhase = 0;

  // Raising a sine wave to the 8th power turns its usual smooth hump
  // into an effective heartbeat.
  float firstBeatPulse  = pow(max(0.0f, sin(animPhase)), 8.0);          // first beat
  float secondBeatPulse = pow(max(0.0f, sin(animPhase - 0.8f)), 8.0);   // second beat, slightly delayed
  float pulse = min(1.0f, firstBeatPulse + secondBeatPulse);

  // Blend from a resting dark red up to a brighter orange-red on each beat.
  uint8_t r = (uint8_t)(180.0 + 75.0  * pulse);
  uint8_t g = (uint8_t)(        80.0  * pulse);
  uint8_t b = 0;
  setStrip(r, g, b, brightness);
}

// 5 — PURPLE MODE: every LED slowly drifts between purple and
// violet-blue, each at its own pace, giving a lava-lamp/nebula feel.
void animPurple(unsigned long now) {
  for (int i = 0; i < NEO_COUNT; i++) {
    // Each LED was given its own random starting point (nebulaOffset)
    // back when the colour was selected, so none of them move in sync.
    float phase = fmod(animPhase + nebulaOffset[i], TWO_PI);
    float drift = (sin(phase) + 1.0) / 2.0;
    uint8_t r = (uint8_t)(80.0 + 80.0 * drift);   // ranges between violet-blue and purple
    uint8_t g = 0;
    uint8_t b = 255;
    strip.setPixelColor(i, applyBrightness(r, g, b));
  }
  animPhase += 0.0015 * 30.0;
  if (animPhase > TWO_PI * 10) animPhase = 0;
  strip.show();
}

// Picks which animation to run based on the currently active colour.
void runIdleAnimation(unsigned long now) {
  switch (colourState) {
    case 0: animWhite(now);  break;
    case 1: animYellow(now); break;
    case 2: animGreen(now);  break;
    case 3: animBlue(now);   break;
    case 4: animRed(now);    break;
    case 5: animPurple(now); break;
  }
}

// Called every time the colour changes,so each
// animation starts from a clean, consistent state rather than picking
// up wherever the previous colour's animation left off.
void resetAnimState(unsigned long now) {
  animPhase      = 0.0;
  animStartTime[colourState] = now;  // start the 1-second "settle" delay for this colour
  sweepPos       = 0;
  sweepLast      = now;
  lightningPixel = -1;
  lightningNext  = now + random(200, 600);
  heartStage     = 0;
  heartTimer     = now;
  // Give the purple animation's LEDs fresh random starting points.
  for (int i = 0; i < NEO_COUNT; i++) {
    nebulaOffset[i] = random(0, 628) / 100.0;  // a random point between 0 and 2π
  }
}

// ═════════════════════════════════════════════════════════════
//  SETUP — runs once when the Arduino is powered on or reset
// ═════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);         // start USB debug output (view it in Serial Monitor)
  randomSeed(analogRead(0));    // use electrical noise on an unused pin to make
                                 // the "random" animations different every time

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // onboard LED 

  // Wake up the motion sensor.
  Wire.begin();
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(PWR_MGMT_1);
  Wire.write(0x00);
  Wire.endTransmission(true);
  delay(300);   // give the sensor a moment to settle after waking up

  // Turn the LED ring on at full white/full brightness immediately,
  // so there's visible confirmation the lamp has started up.
  strip.begin();
  strip.setBrightness(255);
  setStrip(255, 255, 255, 255);

  Serial.println("Keep lamp upright. Calibrating...");
  calibrateRestPosition();   // learn what "upright" looks like.

  // Get all the animation timers ready before the main loop starts.
  unsigned long now = millis();
  resetAnimState(now);
  lastLoop = now;

  Serial.println("Lamp ready.");
}

// ═════════════════════════════════════════════════════════════
//  MAIN LOOP — runs continuously, forever, many times per second
//  It reads the sensor, figures out
//  what gesture is happening, then decide what the LEDs
//  should do about it.
// ═════════════════════════════════════════════════════════════
void loop() {
  unsigned long now   = millis();      // current time since power-on, in milliseconds
  unsigned long delta = now - lastLoop; // time elapsed since the previous loop
  lastLoop = now;

  // ── STEP 1: Read the sensor and work out what's happening 
  float ax, ay, az;
  readAccel(ax, ay, az);

  float orientationDot = getOrientationDot(ax, ay, az); // how close to "upright" are we
  float dynamicG       = getDynamicG(ax, ay, az);       // how hard is it being shaken
  float changeAmount   = getTiltChange(ax, ay, az);     // how far tilted

  // Turn "orientationDot" into a more intuitive angle in degrees
  // (0° = perfectly upright, 180° = perfectly upside down).
  float dotClamped = orientationDot;
  if (dotClamped > 1.0) dotClamped = 1.0;
  if (dotClamped < -1.0) dotClamped = -1.0;
  float angleDeg = acos(dotClamped) * 180.0 / PI;

  bool shaking = (dynamicG > SHAKE_THRESHOLD);        // being shaken right now?
  bool flipped = (orientationDot < FLIP_DOT_THRESHOLD); // upside down right now?

  // Tilting only "counts" for brightness control when the lamp sits
  // in a specific angle range, and never at the same time as a shake
  // or a flip (those gestures take priority).
  bool tilted  = (!shaking &&
                  !flipped &&
                  angleDeg >= TILT_MIN_ANGLE_DEG &&
                  angleDeg <= TILT_MAX_ANGLE_DEG);

  // Print all the current readings to the Serial Monitor — handy for
  // debugging or for tuning the threshold numbers near the top of the file.
  Serial.print(dynamicG, 2);       Serial.print("  ");
  Serial.print(orientationDot, 2); Serial.print("  ");
  Serial.print(changeAmount, 2);   Serial.print("  ");
  Serial.print(angleDeg, 1);       Serial.print("  ");
  if      (shaking) Serial.print("SHAKE  ");
  else if (flipped) Serial.print("FLIP   ");
  else if (tilted)  Serial.print("TILT   ");
  else              Serial.print("NORMAL ");
  Serial.print("  col:"); Serial.print(colourState);
  Serial.print("  bright:"); Serial.println(brightness, 0);

  // Belt-and-braces: never allow tilt-brightness control while shaking.
  if (shaking) tilted = false;

  // ── STEP 2: SHAKE gesture (cycle to the next colour)
  if (shaking) {
    if (now - lastShake > SHAKE_DEBOUNCE) {  // ignore extra shakes that come too fast
      lastShake = now;

      // Start the new blend from whatever colour is currently showing
      // (even if it was mid-blend), so the transition never jumps.
      float t = transitionProgress;
      if (t < 0.0) t = 0.0;
      if (t > 1.0) t = 1.0;

      fromR = fromR + (toR - fromR) * t;
      fromG = fromG + (toG - fromG) * t;
      fromB = fromB + (toB - fromB) * t;

      colourState = (colourState + 1) % 6;   // move to the next colour, wrapping around
      getColourRGB(colourState, toR, toG, toB);

      transitionProgress = 0.0;    // start the fade-to-new-colour animation
      resetAnimState(now);         // give the new colour's animation a clean start
    }

    // Keep advancing the colour blend a little bit every loop.
    if (transitionProgress < 1.0) {
      transitionProgress += TRANSITION_SPEED * (float)delta;
      if (transitionProgress > 1.0) transitionProgress = 1.0;
    }

    renderColour();
    delay(30);   // small pause to control the animation/sensor-read speed
    return;      // skip everything else below while actively shaking
  }

  // If a shake just finished, keep finishing off any colour blend
  // that was still in progress.
  if (transitionProgress < 1.0) {
    transitionProgress += TRANSITION_SPEED * (float)delta;
    if (transitionProgress > 1.0) transitionProgress = 1.0;
  }

  // ── STEP 3: FLIP gesture — fade out immediately
  if (flipped) {
    // If the lamp already finished fading to fully off while upside
    // down,stay off until it's upright.
    if (flippedOffLocked) {
      lampOn = false;
      fadingOff = false;
      fadeOffBright = 0.0;
      digitalWrite(LED_BUILTIN, LOW);
      setStrip(0, 0, 0, 0);
      delay(30);
      return;
    }

    // Flip detected — begin fading out immediately, no waiting period.
    if (!fadingOff) {
      fadingOff = true;
      fadeOffBright = brightness;
    }

    fadeOffBright -= BRIGHTNESS_DROP_PER_MS * (float)delta;

    if (fadeOffBright <= 0.0) {
      // Fully faded out — lock the lamp off until it's turned upright.
      fadeOffBright = 0.0;
      brightness = 0.0;
      lampOn = false;
      fadingOff = false;
      flippedOffLocked = true;
      digitalWrite(LED_BUILTIN, LOW);
      setStrip(0, 0, 0, 0);
      delay(30);
      return;
    }

    // Still fading — show the current animation, but at the
    // temporarily reduced "fading out" brightness.
    float savedBrightness = brightness;
    brightness = fadeOffBright;
    runIdleAnimation(now);
    brightness = savedBrightness;

    delay(30);
    return;
  } else {
    // Not flipped — reset the fade-out state so a future flip starts fresh.
    fadingOff = false;
  }

  // ── STEP 4: RECOVERY — turned upright again → fade in immediately ──
  bool upright = (orientationDot >= UPRIGHT_DOT_THRESHOLD);
  if (!lampOn && !waking) {
    if (!upright) {
      // Lamp is still off — stay off until it's turned upright.
      setStrip(0, 0, 0, 0);
      delay(30);
      return;
    }
    // Upright detected — start waking up immediately, no waiting period.
    waking = true;
    flippedOffLocked = false;
    wakeBrightness = 0.0;
  }

  // ── STEP 5: FADE IN — brighten back up, in the same colour as before ──
  if (waking) {
    wakeBrightness += BRIGHTNESS_DROP_PER_MS * (float)delta;
    if (wakeBrightness >= MAX_BRIGHT) {
      // Reached full brightness — wake-up complete, resume normal operation.
      wakeBrightness = MAX_BRIGHT;
      waking         = false;
      flippedOffLocked = false;
      lampOn         = true;
      brightness     = MAX_BRIGHT;
      brightnessDir  = -1.0;
      getColourRGB(colourState, fromR, fromG, fromB);
      getColourRGB(colourState, toR,   toG,   toB);
      transitionProgress = 1.0;
      resetAnimState(now);
    }

    float wakeR, wakeG, wakeB;
    getColourRGB(colourState, wakeR, wakeG, wakeB);
    setStrip((uint8_t)wakeR, (uint8_t)wakeG, (uint8_t)wakeB, wakeBrightness);

    digitalWrite(LED_BUILTIN, HIGH);
    delay(30);
    return;
  }

  digitalWrite(LED_BUILTIN, HIGH);

  // ── STEP 6: TILT — brightness rises and falls
  if (tilted) {
    // Brightness bounces back and forth between fully off and fully
    // on for as long as the lamp is held in the tilt range.
    brightness += brightnessDir * BRIGHTNESS_DROP_PER_MS * (float)delta;
    if (brightness <= MIN_BRIGHT) { brightness = MIN_BRIGHT; brightnessDir =  1.0; }
    if (brightness >= MAX_BRIGHT) { brightness = MAX_BRIGHT; brightnessDir = -1.0; }
  }

  // ── STEP 7: Otherwise, just play the current colour's idle animation ──
  // (but only once any colour-blend has finished, and only after a
  // 1-second settle time since this colour became active,
  // otherwise show a plain blended colour instead)
  if (transitionProgress >= 1.0 && now - animStartTime[colourState] >= 1000) {
    runIdleAnimation(now);
  } else {
    renderColour();
  }

  delay(30);   // small pause — until process is repeated
}
