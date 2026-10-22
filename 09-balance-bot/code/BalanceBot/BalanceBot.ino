/*
  Kypruino Self-Balancing Robot

  A two-wheeled robot that balances on its own. 200 times per second it measures its tilt with an
  MPU6050 and drives a single TT gear motor (which turns both wheels) through an L298N to stay
  upright. It also holds its spot by leaning gently against its own movement, and fine-tunes its
  balance angle while it runs.

  Board:      Kypruino (select "Arduino UNO" in the Arduino IDE)
  Libraries:  MPU6050_light, Adafruit NeoPixel (Sketch > Include Library > Manage Libraries...)

  WIRING
    MPU6050   SDA -> A4   SCL -> A5   VCC -> 5V   GND -> GND
              Mount it firmly, with its Y axis pointing up and its X axis along the wheel axle.
              Upright then reads about 90 deg (send s in the Serial Monitor to check the pitch).
    L298N     ENA -> D5 (remove the ENA jumper)   IN1 -> D10   IN2 -> D11   OUT1/OUT2 -> motor
              5V -> Kypruino 5V   GND -> Kypruino GND   (keep the 5V-enable jumper on: the
              L298N's regulator powers the Kypruino)
    Power     two 9V batteries in parallel -> L298N 12V / GND
    Kypruino  NeoPixel (D8) and buzzer (D9) switches ON. Button B (D6) = start / stop.

  GETTING STARTED
    1. Power it on lying flat and still: it calibrates the gyroscope (blue lights, then 2 beeps).
    2. Hold it upright and still: after 0.4 s it starts balancing. Then let go.
    3. If the wheels drive away from the lean instead of underneath it, swap the two motor wires
       or send f in the Serial Monitor (saved in EEPROM).
    4. If the side lights look reversed, swap PIX_BACK and PIX_FWD below.
    The balance angle starts at 92 deg and is adjusted while it runs; long runs save part of the
    correction in EEPROM. Send Z# to set it yourself (e.g. Z91.3).

  LIGHTS (3 NeoPixels)                          SOUNDS
    blue           calibrating - keep it still    2 rising beeps  ready
    green blink    ready; an amber side = tip     1 low beep      it moved while calibrating
                   it that way to start           falling tones   it fell
    all green      upright, about to start        3 high beeps    sensor dropped out
    balancing      middle green = on target,      3 low beeps     the last run was cut short
                   red side = leaning that way                    by a reset (power)
    red blink      fell
    purple x3      the last run was cut short by a reset (power)
    yellow         sensor dropped out, recovering

  BLACK BOX: the last ~2 s of each of the last 3 runs (pitch, tilt, rate, motor PWM, 5 V supply)
  and why each one ended - fell back / fell forward / fell sideways / sensor / button / reset
  (brownout, power cut, watchdog) - are kept in EEPROM, so runs without a cable can be checked
  afterwards. Send D to print it, E to erase it.

  SERIAL MONITOR (115200 baud, newline)
    s status   v text telemetry (20 Hz)   V binary data stream (200 Hz)   D black box   E erase it
    x stop   b start   a0/a1 automatic start off/on
    p# d# u# h# gains Kp Kd KV KX   r# return lean cap   n# deadband   Z# balance angle
    f flip motor direction   C re-calibrate the gyro (lying still)
    M motor test   W deadband sweep (both with the wheels in the air)
    Values sent this way are lost on restart: copy the ones that work into the code below.
*/

#include <Wire.h>
#include <EEPROM.h>
#include <avr/wdt.h>
#include <MPU6050_light.h>
#include <Adafruit_NeoPixel.h>

// ================================ hardware ================================
const uint8_t ENA = 5;                  // L298N ENA: motor speed (PWM, Timer0, ~980 Hz)
const uint8_t IN1 = 10, IN2 = 11;       // L298N direction (D8/D9 are the NeoPixels and buzzer)
const uint8_t PIX_PIN = 8, PIX_N = 3;   // Kypruino onboard NeoPixels
const uint8_t PIX_BACK = 0, PIX_MID = 1, PIX_FWD = 2;   // swap BACK/FWD if the side lights look reversed
const uint8_t BUZZER = 9;               // Kypruino onboard passive buzzer (tone)
const uint8_t BUTTON = 6;               // Kypruino button B, active low
const uint8_t MPU_ADDR = 0x68;

MPU6050 mpu(Wire);
Adafruit_NeoPixel pix(PIX_N, PIX_PIN, NEO_GRB + NEO_KHZ800);

// ================================ tuning ================================
// Balance - tuned on our robot; a different build may need small changes (try them live over serial)
float Kp = 32;            // push per degree of lean
float Kd = 2.5;           // push per deg/s of falling speed - damps the swing of the heavy top. Too high and the
                          // motor buzzes back and forth ~10 times a second; too low and it overshoots
float KV = 10;            // deg of extra lean against the estimated wheel speed (1 = top speed). The key
                          // term: it makes the robot brake a drift on purpose, instead of following the
                          // drift until the motor runs out of speed
float KX = 2;             // deg of lean against the estimated distance travelled (holds its spot)
float RETURN_LEAN = 0.6;  // deg, cap on that distance lean. Without it, the pull back grows with distance: after a
                          // long push it races back, runs out of motor speed and falls. Capped, it walks back
                          // upright at a steady speed. Higher = faster return; below ~0.5 it holds its spot less firmly
// Motor
int   DEADBAND = 90;      // PWM below which the geared motor doesn't move; added to every correction so even a
                          // small one turns the wheel. Measure yours with W (wheels in the air)
const int DB_RAMP = 12;   // the very smallest pushes fade in instead of jumping straight to DEADBAND
const int PWM_MAX = 255;
// Safety / starting
const float FALL_DEG = 25;      // tipped further than this, it can't recover: cut the motor and record the fall
const float SIDE_FALL_DEG = 25; // tipped over sideways (it can only balance forwards and backwards)
const float ARM_ROLL = 12;      // never start while lying on its side - pitch means nothing there
const float ARM_DEG = 5;        // starts when held within this of the balance angle...
const float ARM_RATE = 20;      // ...nearly still (deg/s)...
const unsigned ARM_MS = 400;    // ...for this long
const float SOFT_DEG_S = 5;     // soft start: the aim glides from where it was let go to the balance angle
// Balance-angle trim: a steady push in one direction means the balance angle is slightly off,
// so the aim slides slowly until the push disappears
const float TRIM_K = 0.0001, TRIM_RANGE = 4;
const float CALM_DEG = 6;       // trim only while calm, never while catching a fall
// Estimators
const float GYRO_COEF = 0.99;   // tilt filter: share of the gyro at 200 Hz (the accelerometer corrects drift in ~0.5 s)
const float D_FILT = 0.4;       // gyro low-pass for the D term
const float MOTOR_TAU = 0.15;   // s, TT motor spin-up time: lag for the speed estimate
const float X_LEAK_S = 0;       // s, how fast the distance estimate forgets; 0 = never
const float SHIFT_MAX = 6;      // deg, cap on the speed + distance lean
const float DT = 0.005;         // 200 Hz control loop

// ================================ storage ================================
enum Reason : uint8_t { R_NONE, R_FALL_BACK, R_FALL_FWD, R_SENSOR, R_BUTTON, R_STOPPED, R_RESET, R_FALL_SIDE };
struct Sample  { uint8_t pitch; int8_t tilt, rate, pwm; uint8_t vcc; int8_t roll; };   // 6 bytes, one every 40 ms
const uint8_t BB_N = 48;                                                  // ~1.9 s before the end of a run
// The run being recorded lives in RAM that is NOT cleared at reset, so a run cut short by a
// brownout can still be saved at the next boot.
struct LiveRun { uint16_t magic; uint8_t head, count, minVcc; uint16_t durDs; int16_t balance10; Sample s[BB_N]; };
struct RunHdr  { uint16_t magic, seq; uint8_t reason, resetFlags, minVcc, count; uint16_t durDs; int16_t balance10; };
struct Counters { uint16_t magic, boots, powerOns, warmResets, watchdogs, brownoutEnds,
                  runs, fallsBack, fallsFwd, sensorEnds, buttonEnds, resetEnds, sensorFaults, nextSeq,
                  fallsSide, resyncs;
                  uint32_t upDs, accRejects; };
struct Config  { uint16_t magic; float balance; int8_t dir; };

const int EE_CFG = 0, EE_CNT = 16, EE_ARMED = 63, EE_BB = 64;     // EEPROM map (1024 bytes)
const uint8_t BB_SLOTS = 3;
const int BB_SLOT_SIZE = sizeof(RunHdr) + BB_N * sizeof(Sample);   // 12 + 288 = 300 -> ends at 964
const uint16_t LIVE_MAGIC = 0x51A7, BB_MAGIC = 0xB1B1, CNT_MAGIC = 0xC0B9, CFG_MAGIC = 0xC0F2;

LiveRun live __attribute__((section(".noinit")));
Counters cnt;
Config cfg;
RunHdr bbHdr;
int bbPend = -1, bbTotal = 0;           // incremental EEPROM save (1 byte per loop while waiting)

// Why did the board restart? The bootloader clears the reset flags (MCUSR), so the sketch works it out:
//  - RAM survives a warm reset (button, USB, brief dip) but not a power cut -> aliveMagic tells which
//  - the watchdog runs in interrupt-then-reset mode: the interrupt marks wdFired before the reset
//  - a run cut short with RAM intact AND the 5 V supply recorded below 3.5 V = brownout
enum ResetKind : uint8_t { K_NONE, K_POWERON, K_WARM, K_WATCHDOG, K_BROWNOUT };
uint16_t aliveMagic __attribute__((section(".noinit")));
volatile uint16_t wdFired __attribute__((section(".noinit")));
const uint16_t ALIVE_MAGIC = 0xA11E, WD_MAGIC = 0x0DD5;
ISR(WDT_vect) { wdFired = WD_MAGIC; }          // first time-out: note it; the next one resets the chip
void wdtStart() {                               // 0.25 s: interrupt, then reset 0.25 s later
  cli(); wdt_reset(); MCUSR &= ~_BV(WDRF);
  WDTCSR = _BV(WDCE) | _BV(WDE);
  WDTCSR = _BV(WDIE) | _BV(WDE) | _BV(WDP2);
  sei();
}

// ================================ state ================================
enum State : uint8_t { WAIT, BAL };
enum Ui : uint8_t { UI_CAL, UI_READY, UI_BAL, UI_FELL, UI_FAULT };
State state = WAIT;
Ui ui = UI_CAL;
bool autoArm = true, telem = false, telemBin = false, pitchInit = false;
float balanceAngle = 92, trim = 0, armOffset = 0;
float rollF = 0, lastBadAy = 0, lastBadAz = 0, lastBadMag = 0;
uint8_t badRun = 0, zeroRun = 0;
bool mpuAsleep = false;           // the MPU reset itself (supply dip) and sleeps: it still answers but reads all zeros
float pitch = 0, roll = 0, rateNow = 0, gyroBias = 0, rateF = 0, pushLP = 0, vEst = 0, xEst = 0;
float lastPush = 0;
int lastPwm = 0;
unsigned long pitchUs = 0, lastTick = 0, armedAt = 0, stillSince = 0, uiSince = 0;
unsigned i2cFaults = 0;

// ================================ small helpers ================================
void waitMs(unsigned ms) { while (ms >= 10) { delay(10); wdt_reset(); ms -= 10; } }

void pixAll(uint8_t r, uint8_t g, uint8_t b) {
  for (uint8_t i = 0; i < PIX_N; i++) pix.setPixelColor(i, r, g, b);
  pix.show();
}
void beep(unsigned hz, unsigned ms) { tone(BUZZER, hz, ms); waitMs(ms + 40); }
void setUi(uint8_t u) { ui = (Ui)u; uiSince = millis(); }   // uint8_t: the IDE's generated prototypes come before the enum

// Supply voltage at the chip in mV via the internal 1.1 V bandgap. Non-blocking: returns the
// last conversion and starts the next one.
int vccMv() {
  static int v = 0;
  if (!(ADCSRA & _BV(ADSC))) { v = (int)(1125300L / ADC); ADCSRA |= _BV(ADSC); }
  return v;
}
uint8_t vccCode(int mv) { return (uint8_t)constrain((mv - 2000) / 16, 0, 255); }

bool buttonEdge() {                     // one true per press, debounced
  static bool prev = HIGH; static unsigned long t = 0;
  bool r = digitalRead(BUTTON);
  bool e = (r == LOW && prev == HIGH && millis() - t > 60);
  if (e) t = millis();
  prev = r; return e;
}

// ================================ config + counters ================================
void cfgSave() { cfg.magic = CFG_MAGIC; cfg.balance = balanceAngle; EEPROM.put(EE_CFG, cfg); }
void cfgLoad() {
  EEPROM.get(EE_CFG, cfg);
  if (cfg.magic != CFG_MAGIC || !(cfg.balance > 75 && cfg.balance < 105) || (cfg.dir != 1 && cfg.dir != -1)) {
    cfg.dir = 1; balanceAngle = 92; cfgSave();
  }
  balanceAngle = cfg.balance;
}
void cntSave() { EEPROM.put(EE_CNT, cnt); }
void cntLoad() {
  EEPROM.get(EE_CNT, cnt);
  if (cnt.magic != CNT_MAGIC) { memset(&cnt, 0, sizeof(cnt)); cnt.magic = CNT_MAGIC; }
}

// ================================ motor ================================
void motorStop() { analogWrite(ENA, 0); digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); lastPush = 0; lastPwm = 0; }

void motor(float push) {                // push: + = drive forward (toward a forward lean)
  float a = fabs(push);
  int m = (a < DB_RAMP) ? (int)(a * (DEADBAND + DB_RAMP) / DB_RAMP) : (int)(a + DEADBAND);
  if (m > PWM_MAX) m = PWM_MAX;
  bool fwd = (push >= 0) == (cfg.dir > 0);
  digitalWrite(IN1, fwd ? HIGH : LOW);
  digitalWrite(IN2, fwd ? LOW : HIGH);
  analogWrite(ENA, m);
  lastPush = push; lastPwm = push >= 0 ? m : -m;
}

// ================================ black box ================================
const __FlashStringHelper* reasonName(uint8_t r) {
  switch (r) {
    case R_FALL_BACK: return F("FALL_BACK");  case R_FALL_FWD: return F("FALL_FWD");
    case R_SENSOR:    return F("SENSOR");     case R_BUTTON:   return F("BUTTON");
    case R_STOPPED:   return F("STOPPED");    case R_RESET:    return F("RESET");
    case R_FALL_SIDE: return F("FALL_SIDE");
  }
  return F("?");
}
const __FlashStringHelper* resetName(uint8_t k) {
  switch (k) {
    case K_POWERON:  return F("power-cut");     // memory wiped: the board lost power completely
    case K_BROWNOUT: return F("brownout");      // memory kept, 5 V supply recorded collapsing
    case K_WATCHDOG: return F("watchdog");      // the program froze
    case K_WARM:     return F("warm-reset");    // memory kept, no dip seen: button / USB / dip < 5 ms
  }
  return F("-");
}

void bbBegin(uint8_t reason, uint8_t flags) {   // start saving the frozen live run to the next slot
  bbHdr.magic = BB_MAGIC; bbHdr.seq = cnt.nextSeq++;
  bbHdr.reason = reason; bbHdr.resetFlags = flags;
  bbHdr.minVcc = live.minVcc; bbHdr.count = live.count;
  bbHdr.durDs = live.durDs; bbHdr.balance10 = live.balance10;
  int slot = EE_BB + (bbHdr.seq % BB_SLOTS) * BB_SLOT_SIZE;
  EEPROM.update(slot, 0); EEPROM.update(slot + 1, 0);   // old header invalid until the new one is complete
  bbTotal = bbHdr.count * sizeof(Sample) + sizeof(RunHdr);
  bbPend = 0;
  cntSave();
}
void bbStep() {                                 // samples first, header last
  if (bbPend < 0) return;
  int slot = EE_BB + (bbHdr.seq % BB_SLOTS) * BB_SLOT_SIZE;
  int sb = bbHdr.count * sizeof(Sample);
  if (bbPend < sb) {
    uint8_t j = bbPend / sizeof(Sample), k = bbPend % sizeof(Sample);
    uint8_t idx = (live.head + BB_N - live.count + j) % BB_N;           // oldest first
    EEPROM.update(slot + sizeof(RunHdr) + bbPend, ((uint8_t*)&live.s[idx])[k]);
  } else {
    EEPROM.update(slot + (bbPend - sb), ((uint8_t*)&bbHdr)[bbPend - sb]);
  }
  if (++bbPend >= bbTotal) { bbPend = -1; live.magic = 0; }
}
void bbFlush() { while (bbPend >= 0) bbStep(); }

void bbRecord(float tilt, int vcc) {            // every tick: track the lowest supply; every 40 ms: a sample
  static uint8_t div = 0;
  if (vcc > 2000 && vcc < 6000) { uint8_t c = vccCode(vcc); if (c < live.minVcc) live.minVcc = c; }
  live.durDs = (millis() - armedAt) / 100;
  if (++div < 8) return;
  div = 0;
  Sample &s = live.s[live.head];
  s.pitch = (uint8_t)constrain((pitch - 60) * 4, 0, 255);
  s.tilt  = (int8_t)constrain(tilt * 4, -127, 127);
  s.rate  = (int8_t)constrain(rateNow / 4, -127, 127);
  s.pwm   = (int8_t)constrain(lastPwm / 2, -127, 127);
  s.vcc   = vccCode(vcc);
  s.roll  = (int8_t)constrain(rollF * 2, -127, 127);
  live.head = (live.head + 1) % BB_N;
  if (live.count < BB_N) live.count++;
}

void bbDump() {
  Serial.print(F("bb counters runs=")); Serial.print(cnt.runs);
  Serial.print(F(" up=")); Serial.print(cnt.upDs / 10);
  Serial.print(F(" fallsback=")); Serial.print(cnt.fallsBack);
  Serial.print(F(" fallsfwd=")); Serial.print(cnt.fallsFwd);
  Serial.print(F(" sensorends=")); Serial.print(cnt.sensorEnds);
  Serial.print(F(" buttonends=")); Serial.print(cnt.buttonEnds);
  Serial.print(F(" resetends=")); Serial.print(cnt.resetEnds);
  Serial.print(F(" brownoutends=")); Serial.print(cnt.brownoutEnds);
  Serial.print(F(" boots=")); Serial.print(cnt.boots);
  Serial.print(F(" powerons=")); Serial.print(cnt.powerOns);
  Serial.print(F(" warmresets=")); Serial.print(cnt.warmResets);
  Serial.print(F(" watchdogs=")); Serial.print(cnt.watchdogs);
  Serial.print(F(" sensorfaults=")); Serial.print(cnt.sensorFaults);
  Serial.print(F(" fallsside=")); Serial.print(cnt.fallsSide);
  Serial.print(F(" accrejects=")); Serial.print(cnt.accRejects);
  Serial.print(F(" resyncs=")); Serial.println(cnt.resyncs);
  RunHdr h[BB_SLOTS]; uint8_t ord[BB_SLOTS], n = 0;
  for (uint8_t s = 0; s < BB_SLOTS; s++) {
    EEPROM.get(EE_BB + s * BB_SLOT_SIZE, h[s]);
    if (h[s].magic == BB_MAGIC && h[s].count <= BB_N) ord[n++] = s;
  }
  for (uint8_t i = 0; i < n; i++)                // oldest run first
    for (uint8_t j = i + 1; j < n; j++)
      if (h[ord[j]].seq < h[ord[i]].seq) { uint8_t t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
  for (uint8_t i = 0; i < n; i++) {
    RunHdr &r = h[ord[i]];
    Serial.print(F("bb run seq=")); Serial.print(r.seq);
    Serial.print(F(" reason=")); Serial.print(reasonName(r.reason));
    Serial.print(F(" reset=")); Serial.print(resetName(r.resetFlags));
    Serial.print(F(" dur=")); Serial.print(r.durDs / 10.0, 1);
    Serial.print(F(" balance=")); Serial.print(r.balance10 / 10.0, 1);
    Serial.print(F(" minvcc=")); Serial.print(r.minVcc == 255 ? 0 : 2000 + 16 * r.minVcc);
    Serial.print(F(" n=")); Serial.println(r.count);
    for (uint8_t j = 0; j < r.count; j++) {
      Sample s; EEPROM.get(EE_BB + ord[i] * BB_SLOT_SIZE + sizeof(RunHdr) + j * sizeof(Sample), s);
      Serial.print(F("bb s ")); Serial.print(60 + s.pitch / 4.0, 2);
      Serial.print(' '); Serial.print(s.tilt / 4.0, 2);
      Serial.print(' '); Serial.print(s.rate * 4);
      Serial.print(' '); Serial.print(s.pwm * 2);
      Serial.print(' '); Serial.print(2000 + 16 * s.vcc);
      Serial.print(' '); Serial.println(s.roll / 2.0, 1);
      wdt_reset();
    }
  }
  Serial.println(F("bb end"));
}
void bbErase() {
  memset(&cnt, 0, sizeof(cnt)); cnt.magic = CNT_MAGIC; cntSave();
  for (uint8_t s = 0; s < BB_SLOTS; s++) { EEPROM.update(EE_BB + s * BB_SLOT_SIZE, 0); EEPROM.update(EE_BB + s * BB_SLOT_SIZE + 1, 0); }
  Serial.println(F("black box erased"));
}

// ================================ sensor ================================
const uint32_t I2C_HZ = 100000;         // 100 kHz rides out motor noise better than 400 kHz
void i2cStart() { Wire.begin(); Wire.setClock(I2C_HZ); Wire.setWireTimeout(3000, true); }
bool mpuAnswers() { Wire.beginTransmission(MPU_ADDR); return Wire.endTransmission() == 0 && !Wire.getWireTimeoutFlag(); }
void i2cBusClear() {                    // a slave stuck mid-byte holds SDA low: clock it out
  Wire.end();
  pinMode(SDA, INPUT_PULLUP); pinMode(SCL, INPUT_PULLUP); delayMicroseconds(10);
  for (uint8_t i = 0; i < 9 && digitalRead(SDA) == LOW; i++) {
    pinMode(SCL, OUTPUT); digitalWrite(SCL, LOW); delayMicroseconds(10);
    pinMode(SCL, INPUT_PULLUP); delayMicroseconds(10);
  }
  pinMode(SDA, OUTPUT); digitalWrite(SDA, LOW); delayMicroseconds(10);   // STOP condition
  pinMode(SCL, INPUT_PULLUP); delayMicroseconds(10);
  pinMode(SDA, INPUT_PULLUP); delayMicroseconds(10);
}
bool mpuRestart() {                     // bus clear + full MPU reset + re-config; gyro offsets kept
  i2cBusClear(); i2cStart();
  Wire.beginTransmission(MPU_ADDR); Wire.write(0x6B); Wire.write(0x80); Wire.endTransmission();
  waitMs(100);
  Wire.clearWireTimeoutFlag();
  if (mpu.begin(1, 1) != 0 || Wire.getWireTimeoutFlag()) { Wire.clearWireTimeoutFlag(); return false; }   // gyro +-500 dps, accel +-4 g (motor jolts can exceed 2 g)
  mpu.writeData(0x1A, 3);                 // on-chip low-pass ~44 Hz: smooths gearbox vibration, ~5 ms delay
  return true;
}

// Gyro zero: measured lying dead still, then checked to read ~0; redo if it moved.
void calibrateGyro() {
  setUi(UI_CAL);
  for (uint8_t tries = 1; ; tries++) {
    pixAll(0, 0, 30);
    mpu.calcOffsets(true, false);
    float sx = 0, sy = 0, sz = 0, mx = 0;
    for (int i = 0; i < 100; i++) {
      mpu.update();
      float gx = mpu.getGyroX(), gy = mpu.getGyroY(), gz = mpu.getGyroZ();
      sx += gx; sy += gy; sz += gz;
      mx = max(mx, max(fabs(gx), max(fabs(gy), fabs(gz))));
      delay(3);
    }
    sx /= 100; sy /= 100; sz /= 100;
    if (fabs(sx) < 1.0 && fabs(sy) < 1.0 && fabs(sz) < 1.0 && mx < 5.0) {
      Serial.print(F("gyro calibrated, residual ")); Serial.print(sx, 2); Serial.println(F(" deg/s"));
      break;
    }
    Serial.println(F("robot moved during gyro calibration - keep it still, retrying"));
    pixAll(0, 0, 0); beep(300, 120); waitMs(400);
    if (tries >= 20) { Serial.println(F("GYRO NOT CALIBRATED - lay it still and send C")); break; }
  }
  gyroBias = 0; pitchInit = false;
}

void endRun(uint8_t reason);            // used by sensorRecover
void sensorRecover() {
  motorStop();
  if (state == BAL) endRun(R_SENSOR);
  i2cFaults++; cnt.sensorFaults++; cntSave();
  setUi(UI_FAULT); pixAll(40, 30, 0);
  Serial.print(F("MPU not answering (fault #")); Serial.print(i2cFaults); Serial.println(F(") - motor cut, resetting sensor"));
  beep(1500, 60); beep(1500, 60); beep(1500, 60);
  unsigned tries = 0;
  while (!mpuRestart()) {
    if (++tries % 4 == 0) { Serial.print(F("  still no MPU after ")); Serial.print(tries); Serial.println(F(" tries - check SDA/SCL/VCC/GND")); }
    waitMs(250);
  }
  Serial.print(F("MPU back after ")); Serial.print(tries + 1); Serial.println(F(" tries"));
  stillSince = 0; pitchInit = false; setUi(UI_READY);
}

// Tilt filter. pitch = atan2(ay, az) uses only the forward/backward plane, so a side-to-side
// wobble doesn't show up as tilt.
void updatePitch() {
  float raw = mpu.getGyroX();
  float ax = mpu.getAccX(), ay = mpu.getAccY(), az = mpu.getAccZ();
  float aMag = sqrt(ax * ax + ay * ay + az * az);
  if (aMag < 0.2) { if (++zeroRun > 10) mpuAsleep = true; } else zeroRun = 0;   // gravity never reads ~0
  float accPitch = atan2(ay, az) * RAD_TO_DEG;
  if (aMag > 0.7 && aMag < 1.3) {                          // roll from good samples only, smoothed (~0.1 s)
    roll = atan2(ax, sqrt(ay * ay + az * az)) * RAD_TO_DEG;
    rollF += (roll - rollF) * 0.05;
  }
  // The gyro zero drifts as the MPU warms up: re-learn it whenever the robot lies still between runs.
  static float lastRaw = 0, lastAcc = 0;
  static uint8_t quietN = 0;
  bool quiet = fabs(raw - lastRaw) < 1.5 && fabs(accPitch - lastAcc) < 1.0;
  lastRaw = raw; lastAcc = accPitch;
  quietN = quiet ? (quietN < 255 ? quietN + 1 : 255) : 0;
  if (state == WAIT && quietN > 60) gyroBias += (raw - gyroBias) * 0.0025;
  rateNow = raw - gyroBias;
  unsigned long now = micros();
  // Complementary filter: pitch follows the gyro and is nudged 1% toward the accelerometer, but only
  // when that reading is believable (total ~1 g and within 20 deg). A single bad reading during a hard
  // motor jolt is skipped, so the angle can never jump.
  if (!pitchInit) { pitch = accPitch; rollF = roll; pitchInit = true; badRun = 0; }
  else {
    float pred = pitch + rateNow * ((now - pitchUs) * 1e-6);
    float d = accPitch - pred;
    while (d > 180) d -= 360;                              // compare the short way round only
    while (d < -180) d += 360;
    bool sane = aMag > 0.7 && aMag < 1.3 && fabs(d) < 20;
    if (sane) { pitch = pred + (1 - GYRO_COEF) * d; badRun = 0; }
    else {
      pitch = pred;                                        // gyro only through a bad sample
      if (state == BAL) { cnt.accRejects++; lastBadAy = ay; lastBadAz = az; lastBadMag = aMag; }
      // a long run of "bad" samples while gravity looks normal means the estimate itself is off
      // (e.g. gyro saturated while being handled): re-sync to the accelerometer
      if (aMag > 0.9 && aMag < 1.1 && ++badRun > 100) { pitch = accPitch; badRun = 0; if (state == BAL) cnt.resyncs++; }
    }
  }
  pitchUs = now;
}

// ================================ lights ================================
void showUi(float tilt) {               // 20 Hz
  static unsigned long last = 0;
  if (millis() - last < 50) return;
  last = millis();
  bool blink = (millis() / 250) & 1;
  for (uint8_t i = 0; i < PIX_N; i++) pix.setPixelColor(i, 0);
  switch (ui) {
    case UI_CAL:
      if (blink) for (uint8_t i = 0; i < PIX_N; i++) pix.setPixelColor(i, 0, 0, 30);
      break;
    case UI_READY: {
      float off = pitch - balanceAngle;
      if (fabs(off) < ARM_DEG && autoArm) {                 // upright: about to start
        for (uint8_t i = 0; i < PIX_N; i++) pix.setPixelColor(i, 0, 25, 0);
      } else {
        pix.setPixelColor(PIX_MID, 0, blink ? 35 : 5, 0);
        if (off < -ARM_DEG) pix.setPixelColor(PIX_FWD, 35, 18, 0);      // tip it forward
        else if (off > ARM_DEG) pix.setPixelColor(PIX_BACK, 35, 18, 0); // tip it back
      }
      break;
    }
    case UI_BAL: {                                           // spirit level
      float a = fabs(tilt);
      pix.setPixelColor(PIX_MID, 0, a < 1 ? 45 : 8, 0);
      uint8_t r = (uint8_t)min(80.0, a * 8.0);
      if (tilt > 1) pix.setPixelColor(PIX_FWD, r, 0, 0);
      else if (tilt < -1) pix.setPixelColor(PIX_BACK, r, 0, 0);
      break;
    }
    case UI_FELL:
      if (blink) for (uint8_t i = 0; i < PIX_N; i++) pix.setPixelColor(i, 50, 0, 0);
      if (millis() - uiSince > 1200) setUi(UI_READY);
      break;
    case UI_FAULT:
      for (uint8_t i = 0; i < PIX_N; i++) pix.setPixelColor(i, 40, 30, 0);
      break;
  }
  pix.show();
}

// ================================ run control ================================
void arm() {
  rateF = 0; pushLP = 0; vEst = 0; xEst = 0; trim = 0;
  armOffset = constrain(pitch - balanceAngle, -8.0, 8.0);    // soft start from where it is
  live.magic = LIVE_MAGIC; live.head = 0; live.count = 0; live.minVcc = 255;
  live.durDs = 0; live.balance10 = (int16_t)(balanceAngle * 10);
  armedAt = millis(); state = BAL; setUi(UI_BAL);
  EEPROM.update(EE_ARMED, 1);           // if a reset ends this run, the next boot will know
  Serial.print(F("balancing, balance angle ")); Serial.println(balanceAngle, 2);
}

void endRun(uint8_t reason) {
  motorStop();
  state = WAIT; stillSince = 0;
  live.durDs = (millis() - armedAt) / 100;
  EEPROM.update(EE_ARMED, 0);
  cnt.runs++; cnt.upDs += live.durDs;
  if (reason == R_FALL_BACK) cnt.fallsBack++;
  else if (reason == R_FALL_FWD) cnt.fallsFwd++;
  else if (reason == R_SENSOR) cnt.sensorEnds++;
  else if (reason == R_FALL_SIDE) cnt.fallsSide++;
  else cnt.buttonEnds++;
  // A long run that ended in an ordinary fall: keep half of what the trim learned.
  if (live.durDs >= 100 && (reason == R_FALL_BACK || reason == R_FALL_FWD)) {
    balanceAngle = constrain(balanceAngle + 0.5 * trim, 80.0, 100.0);
    cfgSave();
  }
  bbBegin(reason, 0);
  Serial.print(F("run ended: ")); Serial.print(reasonName(reason));
  Serial.print(F(" after ")); Serial.print(live.durDs / 10.0, 1); Serial.println(F(" s"));
  trim = 0; armOffset = 0;
  if (reason == R_FALL_BACK || reason == R_FALL_FWD || reason == R_FALL_SIDE) { setUi(UI_FELL); beep(700, 70); beep(450, 70); beep(260, 120); }
  else setUi(UI_READY);
}

void balanceStep(int vcc) {
  if (buttonEdge()) { endRun(R_BUTTON); return; }
  const float softStep = SOFT_DEG_S * DT;
  if (armOffset > softStep) armOffset -= softStep;
  else if (armOffset < -softStep) armOffset += softStep;
  else armOffset = 0;
  float aim = balanceAngle + trim + armOffset;
  float tilt = pitch - aim;
  if (fabs(rollF) > SIDE_FALL_DEG) { endRun(R_FALL_SIDE); return; }
  if (fabs(tilt) > FALL_DEG) { endRun(tilt < 0 ? R_FALL_BACK : R_FALL_FWD); return; }

  // lean against estimated speed and distance: moving / moved forward -> aim further back.
  // The distance part is capped, so far from its spot it walks back at a steady speed.
  float xLean = KX * xEst;
  bool farAway = fabs(xLean) > RETURN_LEAN;
  if (farAway) xLean = xLean > 0 ? RETURN_LEAN : -RETURN_LEAN;
  float shift = constrain(KV * vEst + xLean, -SHIFT_MAX, SHIFT_MAX);
  float lean = tilt + shift;
  rateF += (rateNow - rateF) * D_FILT;
  float push = Kp * lean + Kd * rateF;
  bool calm = fabs(lean) < CALM_DEG && fabs(push) < PWM_MAX;
  push = constrain(push, -(float)PWM_MAX, (float)PWM_MAX);
  motor(push);

  vEst += (constrain(push / (PWM_MAX - DEADBAND), -1.0, 1.0) - vEst) * (DT / MOTOR_TAU);
  xEst += vEst * DT - (X_LEAK_S > 0 ? xEst * (DT / X_LEAK_S) : 0);
  if (calm && !farAway) {               // while it walks back the push is motion, not a wrong angle: the trim
    pushLP += (push - pushLP) * 0.01;   // would soak it up and add to the pull, so it pauses
    trim = constrain(trim - TRIM_K * pushLP, -TRIM_RANGE, TRIM_RANGE);
  }
  bbRecord(tilt, vcc);
}

void waitStep() {
  bbStep();                             // finish saving the last run first
  bool canArm = bbPend < 0;
  if (buttonEdge() && canArm) { arm(); return; }
  bool nearAim = fabs(pitch - balanceAngle) < ARM_DEG && fabs(rateNow) < ARM_RATE && fabs(rollF) < ARM_ROLL;
  if (autoArm && canArm && nearAim) {
    if (!stillSince) stillSince = millis();
    else if (millis() - stillSince > ARM_MS) arm();
  } else stillSince = 0;
}

// ================================ tests (robot held in the air) ================================
void motorTest() {
  autoArm = false;
  Serial.println(F("motor test: FORWARD 0.6 s (front light green), then BACKWARD 0.6 s (back light green)"));
  for (int8_t s = 1; s >= -1; s -= 2) {
    for (uint8_t i = 0; i < PIX_N; i++) pix.setPixelColor(i, 0);
    pix.setPixelColor(s > 0 ? PIX_FWD : PIX_BACK, 0, 60, 0); pix.show();
    motor(s * 60.0);                    // PWM ~165
    waitMs(600); motorStop(); waitMs(500);
  }
  Serial.println(F("done - auto-arm OFF: send a1 or press B once it's back on the floor"));
  pitchInit = false;
}

void deadbandSweep() {                  // finds DEADBAND: vibration at each PWM step - where it jumps, the wheel starts
  autoArm = false;
  Serial.println(F("sweep pwm accStd_mg gyroRms_dps"));
  for (int sgn = 1; sgn >= -1; sgn -= 2) {
    for (int p = 0; p <= 160; p += 10) {
      bool fwd = (sgn > 0) == (cfg.dir > 0);
      digitalWrite(IN1, fwd ? HIGH : LOW); digitalWrite(IN2, fwd ? LOW : HIGH);
      analogWrite(ENA, p);
      waitMs(250);
      float s = 0, s2 = 0, g2 = 0; int n = 0;
      unsigned long t0 = millis();
      while (millis() - t0 < 400) {
        wdt_reset();
        if (!mpuAnswers()) break;
        mpu.update();
        float a = sqrt(sq(mpu.getAccX()) + sq(mpu.getAccY()) + sq(mpu.getAccZ()));
        s += a; s2 += a * a; g2 += sq(mpu.getGyroX()) + sq(mpu.getGyroY()) + sq(mpu.getGyroZ()); n++;
        delay(3);
      }
      motorStop();
      if (n > 1) {
        float m = s / n;
        Serial.print(F("sweep ")); Serial.print(p * sgn); Serial.print(' ');
        Serial.print(1000.0 * sqrt(max(0.0, s2 / n - m * m)), 1); Serial.print(' ');
        Serial.println(sqrt(g2 / n), 1);
      }
      waitMs(300);
    }
    waitMs(700);
  }
  Serial.println(F("sweep done - auto-arm OFF: send a1 or press B once it's back on the floor"));
  pitchInit = false;
}

// ================================ fast data stream ================================
// 17-byte binary packet every 5 ms (send V), for logging the robot's motion at full rate:
// A5 | ms u16 | pitch*100 i16 | rate*10 i16 | pwm i16 | aim*100 i16 | vEst*100 i8 | xEst*50 i8 | roll*2 i8 | rejects u8 | state u8 | checksum u8
// pwm is the command just decided at this tick (applied for the next 5 ms).
void sendPacket() {
  uint8_t b[17];
  uint16_t ms = (uint16_t)millis();
  int16_t p = (int16_t)(pitch * 100), r = (int16_t)constrain(rateNow * 10, -32000, 32000), w = (int16_t)lastPwm;
  int16_t a = (int16_t)((balanceAngle + trim + armOffset) * 100);
  b[0] = 0xA5;
  memcpy(&b[1], &ms, 2); memcpy(&b[3], &p, 2); memcpy(&b[5], &r, 2); memcpy(&b[7], &w, 2); memcpy(&b[9], &a, 2);
  b[11] = (uint8_t)(int8_t)constrain(vEst * 100, -127, 127);
  b[12] = (uint8_t)(int8_t)constrain(xEst * 50, -127, 127);
  b[13] = (uint8_t)(int8_t)constrain(rollF * 2, -127, 127);
  b[14] = (uint8_t)cnt.accRejects;                              // low byte: a change = a rejected sample
  b[15] = (state == BAL) ? 1 : 0;
  uint8_t cs = 0; for (uint8_t i = 1; i < 16; i++) cs += b[i];
  b[16] = cs;
  if (Serial.availableForWrite() >= 17) Serial.write(b, 17);   // never block the control loop
}

// ================================ serial ================================
void printStatus() {
  Serial.print(state == BAL ? F("BALANCING") : F("waiting"));
  Serial.print(F("  balance ")); Serial.print(balanceAngle, 2);
  Serial.print(F("  trim ")); Serial.print(trim, 2);
  Serial.print(F("  pitch ")); Serial.print(pitch, 1);
  Serial.print(F("  gyroBias ")); Serial.print(gyroBias, 2);
  Serial.print(F("  Kp ")); Serial.print(Kp, 1);
  Serial.print(F(" Kd ")); Serial.print(Kd, 1);
  Serial.print(F(" KV ")); Serial.print(KV, 1);
  Serial.print(F(" KX ")); Serial.print(KX, 1);
  Serial.print(F(" ret ")); Serial.print(RETURN_LEAN, 2);
  Serial.print(F("  deadband ")); Serial.print(DEADBAND);
  Serial.print(F("  dir ")); Serial.print(cfg.dir);
  Serial.print(F("  autoArm ")); Serial.print(autoArm);
  Serial.print(F("  runs ")); Serial.print(cnt.runs);
  Serial.print(F("  cutByReset ")); Serial.print(cnt.resetEnds);
  Serial.print(F(" (brownout ")); Serial.print(cnt.brownoutEnds); Serial.print(')');
  Serial.print(F("  sensorFaults ")); Serial.print(cnt.sensorFaults);
  Serial.print(F("  roll ")); Serial.print(rollF, 1);
  Serial.print(F("  accRejects ")); Serial.print(cnt.accRejects);
  Serial.print(F(" (last ay ")); Serial.print(lastBadAy, 2); Serial.print(F(" az ")); Serial.print(lastBadAz, 2);
  Serial.print(F(" |a| ")); Serial.print(lastBadMag, 2); Serial.print(F(")  resyncs ")); Serial.println(cnt.resyncs);
}

void command(char c, float v, bool num) {
  switch (c) {
    case 's': printStatus(); break;
    case 'v': telem = !telem; break;
    case 'V': telemBin = !telemBin; break;
    case 'D': bbDump(); break;
    case 'E': bbErase(); break;
    case 'x': if (state == BAL) endRun(R_STOPPED); break;
    case 'b': if (state == WAIT && bbPend < 0) arm(); break;
    case 'a': autoArm = (v != 0); stillSince = 0; Serial.println(autoArm ? F("auto-arm ON") : F("auto-arm OFF")); break;
    case 'p': Kp = v; break;
    case 'd': Kd = v; break;
    case 'u': KV = v; break;
    case 'h': KX = v; break;
    case 'r': RETURN_LEAN = v; break;
    case 'n': DEADBAND = (int)v; break;
    case 'Z': if (num && v > 75 && v < 105) { balanceAngle = v; cfgSave(); } printStatus(); break;
    case 'f': cfg.dir = -cfg.dir; cfgSave(); Serial.print(F("motor direction ")); Serial.println(cfg.dir); break;
    case 'C': if (state == WAIT) { wdt_disable(); calibrateGyro(); setUi(UI_READY); wdtStart(); } break;
    case 'M': if (state == WAIT) motorTest(); break;
    case 'W': if (state == WAIT) deadbandSweep(); break;
  }
}

void serialPoll() {
  static char bf[12]; static uint8_t n = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') { bf[n] = 0; if (n) command(bf[0], atof(bf + 1), n > 1); n = 0; }
    else if (n < 11) bf[n++] = c;
  }
}

// ================================ main ================================
void setup() {
  bool ramKept = (aliveMagic == ALIVE_MAGIC);
  uint8_t kind = !ramKept ? K_POWERON : (wdFired == WD_MAGIC ? K_WATCHDOG : K_WARM);
  aliveMagic = ALIVE_MAGIC; wdFired = 0;
  MCUSR = 0; wdt_disable();             // a watchdog reset leaves it armed - calibration would trip it
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT); motorStop();
  pinMode(BUTTON, INPUT_PULLUP);
  Serial.begin(115200);
  pix.begin(); pixAll(0, 0, 30);
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);   // AVcc ref, measure the bandgap
  ADCSRA |= _BV(ADEN) | _BV(ADSC);

  // What reset us - and was it in the middle of a run?
  cntLoad(); cnt.boots++;
  if (kind == K_POWERON) cnt.powerOns++; else if (kind == K_WATCHDOG) cnt.watchdogs++; else cnt.warmResets++;
  bool cut = EEPROM.read(EE_ARMED) == 1;
  if (cut) {
    EEPROM.update(EE_ARMED, 0); cnt.resetEnds++;
    if (!ramKept || live.magic != LIVE_MAGIC || live.count > BB_N || live.head >= BB_N) {   // RAM lost
      live.count = 0; live.head = 0; live.minVcc = 255; live.durDs = 0; live.balance10 = 0;
    } else if (kind == K_WARM && live.minVcc < vccCode(3500)) kind = K_BROWNOUT;   // supply was collapsing
    if (kind == K_BROWNOUT) cnt.brownoutEnds++;
    bbBegin(R_RESET, kind); bbFlush();
  }
  cntSave();
  Serial.print(F("boot: ")); Serial.print(kind == K_POWERON ? F("power-on (memory wiped)") : resetName(kind));
  if (cut) Serial.print(F("  - it was BALANCING: that run was cut short by this reset"));
  Serial.println();
  cfgLoad();

  i2cStart();
  while (!mpuRestart()) {
    Serial.println(F("no MPU - check SDA=A4 SCL=A5 VCC=5V GND"));
    pixAll(40, 30, 0); beep(300, 150); pixAll(0, 0, 0); waitMs(300);
  }
  Serial.println(F("hold still for calibration..."));
  waitMs(500);
  calibrateGyro();
  if (cut) for (uint8_t i = 0; i < 3; i++) { pixAll(40, 0, 40); beep(250, 160); pixAll(0, 0, 0); waitMs(150); }
  setUi(UI_READY);
  beep(2000, 70); beep(2600, 70);
  printStatus();
  Serial.println(F("stand it up - it arms itself (button B also arms / stops)"));
  wdtStart();                           // anything that locks up -> marked, then reset, motor off
}

void loop() {
  wdt_reset();
  if (!(WDTCSR & _BV(WDIE))) { wdFired = 0; wdtStart(); }   // a near miss we survived: not a freeze
  serialPoll();
  if (millis() - lastTick < 5) return;  // everything below at a fixed 200 Hz
  lastTick = millis();

  if (!mpuAnswers()) { sensorRecover(); return; }          // never feed the filter a dead read
  mpu.update();
  if (Wire.getWireTimeoutFlag()) { Wire.clearWireTimeoutFlag(); sensorRecover(); return; }
  updatePitch();
  if (mpuAsleep) {
    mpuAsleep = false; zeroRun = 0;
    Serial.println(F("MPU reads all zeros - it reset itself (supply dip?) - re-initialising"));
    sensorRecover(); return;
  }
  int vcc = vccMv();
  float tilt = pitch - (balanceAngle + trim + armOffset);

  static unsigned long lastT = 0;
  if (telem && millis() - lastT >= 50) {   // ms pitch rate push pwm state vcc vEst xEst roll tilt
    lastT = millis();
    Serial.print(lastT); Serial.print(' ');
    Serial.print(pitch, 1); Serial.print(' ');
    Serial.print(rateNow, 0); Serial.print(' ');
    Serial.print(lastPush, 0); Serial.print(' ');
    Serial.print(lastPwm); Serial.print(' ');
    Serial.print(state == BAL ? 'B' : 'W'); Serial.print(' ');
    Serial.print(vcc); Serial.print(' ');
    Serial.print((int)(vEst * 100)); Serial.print(' ');
    Serial.print((int)(xEst * 100)); Serial.print(' ');
    Serial.print(roll, 1); Serial.print(' ');
    Serial.println(tilt, 1);
  }
  showUi(tilt);

  if (state == WAIT) waitStep();
  else balanceStep(vcc);
  if (telemBin) sendPacket();
}
