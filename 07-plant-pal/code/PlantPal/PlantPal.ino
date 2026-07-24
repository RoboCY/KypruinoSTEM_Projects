/*
  ============================================================
  PlantPal — Smart Plant Monitor
  Part of the Kypruino STEM Projects by ROBO (robo.com.cy)
  Guide: https://robo.com.cy/blogs/blog/smart-plant-monitor-kypruino-oled
  ============================================================

  Board: Kypruino / Arduino-compatible
  Display: 0.96" SSD1306 128x64 I2C OLED

  PlantPal is a small plant monitoring system that uses the
  Kypruino board to read environmental conditions around a
  potted plant and present them through a friendly OLED interface.

  The system monitors:
    - Soil moisture using a capacitive soil moisture sensor
    - Ambient light using an analog light sensor module
    - Air temperature and humidity using an AHT10 sensor

  In addition to displaying the live sensor readings, PlantPal
  also calculates derived features to make the readings more useful:

    - VPD (Vapor Pressure Deficit):
        A combined temperature and humidity indicator that estimates
        how strongly the air is pulling moisture from the plant.
        Higher VPD means drier air and faster moisture loss.

    - 24-hour ambient light average:
        A rolling light estimate that avoids judging the plant based
        only on temporary darkness, such as night-time conditions.

    - Plant comfort score:
        A simple weighted score based on soil moisture, temperature,
        air dryness, and daily ambient light. This score controls the
        plant mood face shown on the OLED.

  The OLED interface rotates between:
    - A large animated mood face
    - A compact sensor information screen
    - A suggested action screen

  Different plant care profiles can be selected in the code so the
  same hardware can be adapted for dry-loving plants, herbs, balanced
  houseplants, humid-loving plants, orchids, or custom plants.

  Notes:
    - Use AO/AOUT from the light sensor module, not DO/DOUT.
    - The displayed light percentage represents light present:
        brighter = higher percentage
        darker   = lower percentage
    - Ambient light is displayed immediately, but it is only included
      in the comfort score after the first 24-hour average is ready.
    - The 24-hour light history resets if the board loses power.
  ============================================================
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_AHTX0.h>
#include <math.h>

// ============================================================
// OLED SETTINGS
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_AHTX0 aht;

// 0 = normal, 2 = upside down / 180 degrees
#define OLED_ROTATION 2

// ============================================================
// PIN SETTINGS
// ============================================================

#define SOIL_PIN A0
#define LIGHT_PIN A1

// ============================================================
// SOIL SENSOR CALIBRATION
// ============================================================
// Check the info screen and adjust these.
// For many capacitive soil sensors:
// dry soil = higher raw value
// wet soil = lower raw value

#define SOIL_RAW_DRY 800
#define SOIL_RAW_WET 350

// ============================================================
// LIGHT SENSOR CALIBRATION
// ============================================================
// Check the info screen and adjust these.

#define LIGHT_RAW_LOW 100
#define LIGHT_RAW_HIGH 900
#define LIGHT_DARK_GIVES_HIGH_RAW 0

// ============================================================
// LIGHT AVERAGING SETTINGS
// ============================================================

// Real use: one stored average per hour
#define ONE_HOUR_MS 3600000UL

// Quick testing only:
// #define ONE_HOUR_MS 10000UL

#define LIGHT_HISTORY_HOURS 24

// ============================================================
// CHOOSE PLANT CARE PROFILE
// Uncomment ONE profile !!!
// ============================================================

// Dry-loving:
// Good for small cactus, succulents, aloe, echeveria,
// haworthia, jade plant, small sedum.
// #define PROFILE_DRY_LOVING

// Bright herb:
// Good for basil, parsley, coriander/cilantro, small edible herbs
// mint can also work but may need repotting more often
// #define PROFILE_BRIGHT_HERB

// Balanced houseplant:
// Good for pothos cuttings, peperomia, spider plant,
// small philodendron, baby rubberplant.
#define PROFILE_BALANCED_HOUSEPLANT

// Humid-loving:
// Good for small ferns, fittonia/nerve plant,
// baby tears, small calathea-style plants
// #define PROFILE_HUMID_LOVING

// Orchid / airy media:
// Good for mini orchids and small phalaenopsis orchids
// grown in bark or airy orchid media
// #define PROFILE_ORCHID_AIRY

// Flowering plant:
// Good for carnations, African violet, small geranium/pelargonium, primrose
// some begonias also fit but may prefer higher humidity
// Avoid using this as a precise profile for specialist flowers.
// #define PROFILE_FLOWERING

// Custom:
// Use this when your plant does not fit the groups above,
// or when you want to manually adjust the thresholds.
// #define PROFILE_CUSTOM


// ============================================================
// PLANT PROFILE SETTINGS
// These are simple educational defaults for small potted plants.
// ============================================================

#ifdef PROFILE_DRY_LOVING
  #define PLANT_NAME "Dry-loving"
  #define SOIL_DRY_LIMIT 18
  #define SOIL_WET_LIMIT 60
  #define TEMP_LOW_LIMIT 10
  #define TEMP_HIGH_LIMIT 36
  #define VPD_LOW_LIMIT 0.5
  #define VPD_HIGH_LIMIT 2.3
  #define LIGHT_DAILY_LOW_LIMIT 50
  #define LIGHT_DAILY_HIGH_LIMIT 100
#endif

#ifdef PROFILE_BRIGHT_HERB
  #define PLANT_NAME "Herb"
  #define SOIL_DRY_LIMIT 45
  #define SOIL_WET_LIMIT 85
  #define TEMP_LOW_LIMIT 16
  #define TEMP_HIGH_LIMIT 32
  #define VPD_LOW_LIMIT 0.4
  #define VPD_HIGH_LIMIT 1.8
  #define LIGHT_DAILY_LOW_LIMIT 45
  #define LIGHT_DAILY_HIGH_LIMIT 100
#endif

#ifdef PROFILE_BALANCED_HOUSEPLANT
  #define PLANT_NAME "Houseplant"
  #define SOIL_DRY_LIMIT 35
  #define SOIL_WET_LIMIT 80
  #define TEMP_LOW_LIMIT 15
  #define TEMP_HIGH_LIMIT 32
  #define VPD_LOW_LIMIT 0.4
  #define VPD_HIGH_LIMIT 1.8
  #define LIGHT_DAILY_LOW_LIMIT 30
  #define LIGHT_DAILY_HIGH_LIMIT 85
#endif

#ifdef PROFILE_HUMID_LOVING
  #define PLANT_NAME "Humid-loving"
  #define SOIL_DRY_LIMIT 55
  #define SOIL_WET_LIMIT 90
  #define TEMP_LOW_LIMIT 15
  #define TEMP_HIGH_LIMIT 28
  #define VPD_LOW_LIMIT 0.2
  #define VPD_HIGH_LIMIT 1.1
  #define LIGHT_DAILY_LOW_LIMIT 20
  #define LIGHT_DAILY_HIGH_LIMIT 70
#endif

#ifdef PROFILE_ORCHID_AIRY
  #define PLANT_NAME "Orchid"
  #define SOIL_DRY_LIMIT 35
  #define SOIL_WET_LIMIT 75
  #define TEMP_LOW_LIMIT 18
  #define TEMP_HIGH_LIMIT 30
  #define VPD_LOW_LIMIT 0.4
  #define VPD_HIGH_LIMIT 1.4
  #define LIGHT_DAILY_LOW_LIMIT 30
  #define LIGHT_DAILY_HIGH_LIMIT 75
#endif

#ifdef PROFILE_FLOWERING
  #define PLANT_NAME "Flowering"
  #define SOIL_DRY_LIMIT 40
  #define SOIL_WET_LIMIT 82
  #define TEMP_LOW_LIMIT 16
  #define TEMP_HIGH_LIMIT 30
  #define VPD_LOW_LIMIT 0.4
  #define VPD_HIGH_LIMIT 1.6
  #define LIGHT_DAILY_LOW_LIMIT 40
  #define LIGHT_DAILY_HIGH_LIMIT 90
#endif

#ifdef PROFILE_CUSTOM
  #define PLANT_NAME "Custom"
  #define SOIL_DRY_LIMIT 40
  #define SOIL_WET_LIMIT 80
  #define TEMP_LOW_LIMIT 16
  #define TEMP_HIGH_LIMIT 30
  #define VPD_LOW_LIMIT 0.4
  #define VPD_HIGH_LIMIT 1.6
  #define LIGHT_DAILY_LOW_LIMIT 35
  #define LIGHT_DAILY_HIGH_LIMIT 90
#endif

// ============================================================
// SCREEN CONTROL
// ============================================================

#define SCREEN_MOOD   0
#define SCREEN_INFO   1
#define SCREEN_ADVICE 2
#define TOTAL_SCREENS 3

#define MOOD_SCREEN_TIME   14000UL
#define INFO_SCREEN_TIME   10000UL
#define ADVICE_SCREEN_TIME 10000UL

byte currentScreen = SCREEN_MOOD;
unsigned long lastScreenChange = 0;

// ============================================================
// SENSOR VALUES
// ============================================================

int soilRaw = 0;
int soilPercent = 0;

int lightRaw = 0;
int lightPercent = 0;

float temperatureC = 0.0;
float humidityPercent = 0.0;
float vpd = 0.0;  // Vapour Pressure Deficit

bool ahtFound = false;

// ============================================================
// LIGHT HISTORY VALUES
// ============================================================

int hourlyLightSamples[LIGHT_HISTORY_HOURS];
int currentLightHourIndex = 0;
int recordedLightHours = 0;
int light24hAveragePercent = 0;
bool lightAverageReady = false;

unsigned long lastLightHourUpdate = 0;

// Used to average the light throughout the current hour
long currentHourLightTotal = 0;
int currentHourLightCount = 0;

// ============================================================
// STATUS CODES
// ============================================================

#define STATUS_HAPPY        0
#define STATUS_VERY_DRY     1
#define STATUS_DRY          2
#define STATUS_TOO_WET      3
#define STATUS_TOO_COLD     4
#define STATUS_TOO_HOT      5
#define STATUS_AIR_DRY      6
#define STATUS_LOW_AMBIENT  7
#define STATUS_AHT_MISSING  8

// ============================================================
// SETUP
// ============================================================

void setup() {
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true);
  }

  display.setRotation(OLED_ROTATION);
  display.clearDisplay();
  display.display();

  ahtFound = aht.begin();

  for (int i = 0; i < LIGHT_HISTORY_HOURS; i++) {
    hourlyLightSamples[i] = 0;
  }

  lastLightHourUpdate = millis();
  lastScreenChange = millis();
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  readSensors();
  updateLightHistory();
  updateScreenTimer();
  drawCurrentScreen();

  // Small delay keeps animation smooth without overwhelming the OLED
  delay(120);
}

// ============================================================
// SENSOR READING
// ============================================================

void readSensors() {
  soilRaw = analogRead(SOIL_PIN);

  soilPercent = map(soilRaw, SOIL_RAW_DRY, SOIL_RAW_WET, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);

  lightRaw = analogRead(LIGHT_PIN);

  int rawPercent = map(lightRaw, LIGHT_RAW_LOW, LIGHT_RAW_HIGH, 0, 100);
  rawPercent = constrain(rawPercent, 0, 100);

#if LIGHT_DARK_GIVES_HIGH_RAW
  // Dark gives high raw value, so invert to get "light present".
  lightPercent = 100 - rawPercent;
#else
  // Bright gives high raw value, so use directly.
  lightPercent = rawPercent;
#endif

  lightPercent = constrain(lightPercent, 0, 100);

  if (ahtFound) {
    sensors_event_t humidity;
    sensors_event_t temp;

    aht.getEvent(&humidity, &temp);

    temperatureC = temp.temperature;
    humidityPercent = humidity.relative_humidity;
    vpd = calculateVPD(temperatureC, humidityPercent);
  }
}

// ============================================================
// VPD CALCULATION
// ============================================================
// VPD = air dryness indicator.
// Higher VPD means the air pulls more moisture from the plant.

float calculateVPD(float tempC, float rh) {
  float svp = 0.6108 * exp((17.27 * tempC) / (tempC + 237.3));
  return svp * (1.0 - rh / 100.0);
}

// ============================================================
// 24-HOUR LIGHT AVERAGE
// ============================================================

void updateLightHistory() {
  currentHourLightTotal += lightPercent;
  currentHourLightCount++;

  unsigned long now = millis();

  if (now - lastLightHourUpdate >= ONE_HOUR_MS) {
    lastLightHourUpdate = now;

    int hourAverage = lightPercent;

    if (currentHourLightCount > 0) {
      hourAverage = currentHourLightTotal / currentHourLightCount;
    }

    hourlyLightSamples[currentLightHourIndex] = hourAverage;

    currentHourLightIndexAdvance();

    if (recordedLightHours < LIGHT_HISTORY_HOURS) {
      recordedLightHours++;
    }

    calculateLightAverage();

    if (recordedLightHours >= LIGHT_HISTORY_HOURS) {
      lightAverageReady = true;
    }

    currentHourLightTotal = 0;
    currentHourLightCount = 0;
  }
}

void currentHourLightIndexAdvance() {
  currentLightHourIndex++;

  if (currentLightHourIndex >= LIGHT_HISTORY_HOURS) {
    currentLightHourIndex = 0;
  }
}

void calculateLightAverage() {
  if (recordedLightHours == 0) {
    light24hAveragePercent = 0;
    return;
  }

  long total = 0;

  for (int i = 0; i < recordedLightHours; i++) {
    total += hourlyLightSamples[i];
  }

  light24hAveragePercent = total / recordedLightHours;
}

int getHoursUntilLightReady() {
  int remaining = LIGHT_HISTORY_HOURS - recordedLightHours;

  if (remaining < 0) {
    remaining = 0;
  }

  return remaining;
}

// ============================================================
// STATUS AND SCORING
// ============================================================

byte getMainStatus() {
  if (!ahtFound) {
    return STATUS_AHT_MISSING;
  }

  if (soilPercent < SOIL_DRY_LIMIT - 15) return STATUS_VERY_DRY;
  if (soilPercent < SOIL_DRY_LIMIT) return STATUS_DRY;
  if (soilPercent > SOIL_WET_LIMIT) return STATUS_TOO_WET;

  if (temperatureC < TEMP_LOW_LIMIT) return STATUS_TOO_COLD;
  if (temperatureC > TEMP_HIGH_LIMIT) return STATUS_TOO_HOT;

  if (vpd > VPD_HIGH_LIMIT) return STATUS_AIR_DRY;

  // Ambient light only affects judgement after 24 hours.
  if (lightAverageReady && light24hAveragePercent < LIGHT_DAILY_LOW_LIMIT) {
    return STATUS_LOW_AMBIENT;
  }

  return STATUS_HAPPY;
}

int mapFloatToInt(float x, float inMin, float inMax, int outMin, int outMax) {
  if (inMax == inMin) return outMin;

  float result = (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;

  if (result < outMin && outMin < outMax) result = outMin;
  if (result > outMax && outMin < outMax) result = outMax;

  if (result > outMin && outMin > outMax) result = outMin;
  if (result < outMax && outMin > outMax) result = outMax;

  return (int)result;
}

int scoreRange(float value, float idealLow, float idealHigh, float warnLow, float warnHigh, float badLow, float badHigh) {
  if (value >= idealLow && value <= idealHigh) {
    return 100;
  }

  if (value >= warnLow && value < idealLow) {
    return mapFloatToInt(value, warnLow, idealLow, 70, 100);
  }

  if (value > idealHigh && value <= warnHigh) {
    return mapFloatToInt(value, idealHigh, warnHigh, 100, 70);
  }

  if (value >= badLow && value < warnLow) {
    return mapFloatToInt(value, badLow, warnLow, 0, 70);
  }

  if (value > warnHigh && value <= badHigh) {
    return mapFloatToInt(value, warnHigh, badHigh, 70, 0);
  }

  return 0;
}

int getSoilScore() {
  /*
    Soil scoring:
    - 100% near the middle of the acceptable soil range
    - lower near dry/wet limits
    - very low when far outside
  */

  float idealLow = SOIL_DRY_LIMIT + 10;
  float idealHigh = SOIL_WET_LIMIT - 10;

  float warnLow = SOIL_DRY_LIMIT;
  float warnHigh = SOIL_WET_LIMIT;

  float badLow = SOIL_DRY_LIMIT - 20;
  float badHigh = SOIL_WET_LIMIT + 15;

  return scoreRange(soilPercent, idealLow, idealHigh, warnLow, warnHigh, badLow, badHigh);
}

int getTempScore() {
  if (!ahtFound) return 50;

  /*
    Temperature scoring:
    - 100% comfortably inside the range
    - lower as it approaches too cold / too hot
  */

  float idealLow = TEMP_LOW_LIMIT + 3;
  float idealHigh = TEMP_HIGH_LIMIT - 3;

  float warnLow = TEMP_LOW_LIMIT;
  float warnHigh = TEMP_HIGH_LIMIT;

  float badLow = TEMP_LOW_LIMIT - 6;
  float badHigh = TEMP_HIGH_LIMIT + 6;

  return scoreRange(temperatureC, idealLow, idealHigh, warnLow, warnHigh, badLow, badHigh);
}

int getAirScore() {
  if (!ahtFound) return 50;

  /*
    VPD scoring:
    - 100% in the normal VPD range
    - lower if air becomes too humid or too dry
  */

  float idealLow = VPD_LOW_LIMIT + 0.2;
  float idealHigh = VPD_HIGH_LIMIT - 0.3;

  float warnLow = VPD_LOW_LIMIT;
  float warnHigh = VPD_HIGH_LIMIT;

  float badLow = VPD_LOW_LIMIT - 0.3;
  float badHigh = VPD_HIGH_LIMIT + 0.8;

  return scoreRange(vpd, idealLow, idealHigh, warnLow, warnHigh, badLow, badHigh);
}

int getLightScore() {
  if (!lightAverageReady) {
    return 100;
  }

  /*
    Ambient light scoring:
    - 100% comfortably inside the daily ambient range
    - lower near the low/high limits
    - very low if much too dark
  */

  float idealLow = LIGHT_DAILY_LOW_LIMIT + 10;
  float idealHigh = LIGHT_DAILY_HIGH_LIMIT - 10;

  float warnLow = LIGHT_DAILY_LOW_LIMIT;
  float warnHigh = LIGHT_DAILY_HIGH_LIMIT;

  float badLow = LIGHT_DAILY_LOW_LIMIT - 25;
  float badHigh = LIGHT_DAILY_HIGH_LIMIT + 10;

  return scoreRange(light24hAveragePercent, idealLow, idealHigh, warnLow, warnHigh, badLow, badHigh);
}

int getComfortScore() {
  int soilScore = getSoilScore();
  int tempScore = getTempScore();
  int airScore = getAirScore();
  int lightScore = getLightScore();

  if (!lightAverageReady) {
    // First 24 hours: light is displayed, but not judged.
    return (soilScore * 60 + tempScore * 20 + airScore * 20) / 100;
  }

  return (soilScore * 50 + lightScore * 25 + tempScore * 15 + airScore * 10) / 100;
}

// ============================================================
// SCREEN TIMER
// ============================================================

void updateScreenTimer() {
  unsigned long duration = getCurrentScreenDuration();

  if (millis() - lastScreenChange >= duration) {
    lastScreenChange = millis();

    currentScreen++;

    if (currentScreen >= TOTAL_SCREENS) {
      currentScreen = 0;
    }
  }
}

unsigned long getCurrentScreenDuration() {
  if (currentScreen == SCREEN_MOOD) return MOOD_SCREEN_TIME;
  if (currentScreen == SCREEN_INFO) return INFO_SCREEN_TIME;
  return ADVICE_SCREEN_TIME;
}

// ============================================================
// DISPLAY SCREENS
// ============================================================

void drawCurrentScreen() {
  if (currentScreen == SCREEN_MOOD) {
    drawMoodScreen();
  }
  else if (currentScreen == SCREEN_INFO) {
    drawInfoScreen();
  }
  else {
    drawAdviceScreen();
  }
}

// ============================================================
// SCREEN 1: BIG MOOD FACE
// ============================================================

void drawMoodScreen() {
  int comfort = getComfortScore();
  byte status = getMainStatus();

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  drawCuteAnimatedFace(64, 24, comfort);

  display.setTextSize(1);

  display.setCursor(0, 52);
  display.print(F("Mood "));
  display.print(comfort);
  display.print(F("%  "));

  printStatus(status);

  display.display();
}

// ============================================================
// SCREEN 2: CLEAN SENSOR INFO SCREEN
// ============================================================

void drawInfoScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Row 1: soil
  display.setCursor(0, 0);
  display.print(F("Soil: "));
  display.print(soilPercent);
  display.print(F("% "));
  printSoilStatus();

  // Row 2: live ambient light
  display.setCursor(0, 12);
  display.print(F("Light: "));
  display.print(lightPercent);
  display.print(F("% live"));

  // Row 3: 24h ambient light
  display.setCursor(0, 24);
  if (lightAverageReady) {
    display.print(F("24h: "));
    display.print(light24hAveragePercent);
    display.print(F("% "));
    printLightStatus();
  } else {
    display.print(F("24h: learning "));
    display.print(getHoursUntilLightReady());
    display.print(F("h"));
  }

  // Row 4: temperature and humidity
  display.setCursor(0, 36);
  if (ahtFound) {
    display.print(F("Air: "));
    display.print(temperatureC, 1);
    display.print(F("C  "));
    display.print(humidityPercent, 0);
    display.print(F("% Hum"));
  } else {
    display.print(F("Air: sensor missing"));
  }

  // Row 5: VPD / air dryness
  display.setCursor(0, 48);
  if (ahtFound) {
    display.print(F("VPD: "));
    printAirStatus();
    display.print(F(" "));
    display.print(vpd, 2);
    display.print(F("kPa"));
  } else {
    display.print(F("VPD: unavailable"));
  }

  display.display();
}

// ============================================================
// SCREEN 3: SUGGESTED ACTION
// ============================================================

void drawAdviceScreen() {
  byte status = getMainStatus();

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Big main action at the top
  display.setTextSize(2);
  display.setCursor(0, 0);

  switch (status) {
    case STATUS_VERY_DRY:
      display.println(F("Water"));
      display.println(F("now"));
      break;

    case STATUS_DRY:
      display.println(F("Water"));
      display.println(F("soon"));
      break;

    case STATUS_TOO_WET:
      display.println(F("Too"));
      display.println(F("wet"));
      break;

    case STATUS_TOO_COLD:
      display.println(F("Too"));
      display.println(F("cold"));
      break;

    case STATUS_TOO_HOT:
      display.println(F("Too"));
      display.println(F("hot"));
      break;

    case STATUS_AIR_DRY:
      display.println(F("Dry"));
      display.println(F("air"));
      break;

    case STATUS_LOW_AMBIENT:
      display.println(F("Low"));
      display.println(F("light"));
      break;

    case STATUS_AHT_MISSING:
      display.println(F("Check"));
      display.println(F("AHT10"));
      break;

    default:
      display.println(F("All"));
      display.println(F("good"));
      break;
  }

  // Small advice text safely below the big text
  display.setTextSize(1);
  display.setCursor(0, 36);
  printAdviceLine1(status);

  display.setCursor(0, 48);
  printAdviceLine2(status);

  display.display();
}

// ============================================================
// CUTE ANIMATED FACE
// ============================================================

void drawCuteAnimatedFace(int cx, int cy, int score) {
  bool blink = isBlinkFrame();

  if (blink) {
    drawBlinkingEyes(cx, cy);
  } else {
    drawBigCuteEyes(cx, cy, score);
  }

  drawCuteMouth(cx, cy, score);

  // Tiny cheek pixels when happy
  if (score >= 75 && !blink) {
    display.drawPixel(cx - 30, cy + 8, SSD1306_WHITE);
    display.drawPixel(cx - 28, cy + 10, SSD1306_WHITE);
    display.drawPixel(cx + 30, cy + 8, SSD1306_WHITE);
    display.drawPixel(cx + 28, cy + 10, SSD1306_WHITE);
  }
}

bool isBlinkFrame() {
  // A short blink roughly every 4 seconds
  unsigned long t = millis() % 4200UL;
  return (t > 3950UL);
}

void getEyeLookOffset(int score, int *lookX, int *lookY) {
  /*
    Controls where the pupils look.

    Happy/neutral:
      mostly centred, occasionally left/right/up/down.

    Sad:
      mostly downward, sometimes slightly left/right.
  */

  unsigned long phase = (millis() / 1800UL) % 8;

  *lookX = 0;
  *lookY = 0;

  if (score < 45) {
    // Sad/concerned: mostly looking down
    *lookY = 3;

    if (phase == 2) {
      *lookX = -2;
    } else if (phase == 5) {
      *lookX = 2;
    }

    return;
  }

  // Happy/neutral: mostly centred with occasional glances
  switch (phase) {
    case 1:
      *lookX = -2;  // look left
      *lookY = 0;
      break;

    case 3:
      *lookX = 2;   // look right
      *lookY = 0;
      break;

    case 5:
      *lookX = 0;
      *lookY = -2;  // look up
      break;

    case 7:
      *lookX = 0;
      *lookY = 2;   // look down
      break;

    default:
      *lookX = 0;   // centred
      *lookY = 0;
      break;
  }
}

void drawBigCuteEyes(int cx, int cy, int score) {
  int eyeW = 18;
  int eyeH = 22;

  if (score < 45) {
    eyeH = 19;
  }

  int leftEyeX = cx - 28;
  int rightEyeX = cx + 10;
  int eyeY = cy - 15;

  // Eye whites
  display.fillRoundRect(leftEyeX,  eyeY, eyeW, eyeH, 6, SSD1306_WHITE);
  display.fillRoundRect(rightEyeX, eyeY, eyeW, eyeH, 6, SSD1306_WHITE);

  int lookX = 0;
  int lookY = 0;
  getEyeLookOffset(score, &lookX, &lookY);

  // Pupil centres
  int leftPupilX = leftEyeX + eyeW / 2 + lookX;
  int rightPupilX = rightEyeX + eyeW / 2 + lookX;
  int pupilY = eyeY + eyeH / 2 + lookY;

  // Black pupils
  display.fillCircle(leftPupilX, pupilY, 4, SSD1306_BLACK);
  display.fillCircle(rightPupilX, pupilY, 4, SSD1306_BLACK);

  // Small white sparkle inside each pupil.
  // This now stays relative to the pupil, so the eyes do not always look up-left.
  display.drawPixel(leftPupilX - 1, pupilY - 1, SSD1306_WHITE);
  display.drawPixel(rightPupilX - 1, pupilY - 1, SSD1306_WHITE);

  // Extra lower shine on happy eyes
  if (score >= 75) {
    display.drawPixel(leftPupilX + 2, pupilY + 2, SSD1306_WHITE);
    display.drawPixel(rightPupilX + 2, pupilY + 2, SSD1306_WHITE);
  }

  if (score < 45) {
    // Worried eyebrows
    display.drawLine(cx - 31, cy - 21, cx - 11, cy - 24, SSD1306_WHITE);
    display.drawLine(cx + 11, cy - 24, cx + 31, cy - 21, SSD1306_WHITE);
  }
}

void drawBlinkingEyes(int cx, int cy) {
  display.drawLine(cx - 29, cy - 3, cx - 10, cy - 3, SSD1306_WHITE);
  display.drawLine(cx + 10, cy - 3, cx + 29, cy - 3, SSD1306_WHITE);

  display.drawLine(cx - 26, cy - 2, cx - 13, cy - 2, SSD1306_WHITE);
  display.drawLine(cx + 13, cy - 2, cx + 26, cy - 2, SSD1306_WHITE);
}

void drawCuteMouth(int cx, int cy, int score) {
  if (score >= 75) {
    // Happy small smile
    display.drawLine(cx - 8, cy + 18, cx - 4, cy + 21, SSD1306_WHITE);
    display.drawLine(cx - 4, cy + 21, cx + 4, cy + 21, SSD1306_WHITE);
    display.drawLine(cx + 4, cy + 21, cx + 8, cy + 18, SSD1306_WHITE);
  }
  else if (score >= 45) {
    // Neutral / okay
    display.drawLine(cx - 7, cy + 20, cx + 7, cy + 20, SSD1306_WHITE);
  }
  else {
    // Small frown
    display.drawLine(cx - 8, cy + 22, cx - 4, cy + 19, SSD1306_WHITE);
    display.drawLine(cx - 4, cy + 19, cx + 4, cy + 19, SSD1306_WHITE);
    display.drawLine(cx + 4, cy + 19, cx + 8, cy + 22, SSD1306_WHITE);
  }
}

// ============================================================
// TEXT HELPERS
// ============================================================

void printStatus(byte status) {
  switch (status) {
    case STATUS_HAPPY:
      display.print(F("Happy"));
      break;

    case STATUS_VERY_DRY:
      display.print(F("Very thirsty"));
      break;

    case STATUS_DRY:
      display.print(F("Water soon"));
      break;

    case STATUS_TOO_WET:
      display.print(F("Too wet"));
      break;

    case STATUS_TOO_COLD:
      display.print(F("Too cold"));
      break;

    case STATUS_TOO_HOT:
      display.print(F("Too hot"));
      break;

    case STATUS_AIR_DRY:
      display.print(F("Air dry"));
      break;

    case STATUS_LOW_AMBIENT:
      display.print(F("Low light"));
      break;

    case STATUS_AHT_MISSING:
      display.print(F("AHT missing"));
      break;
  }
}

void printSoilStatus() {
  if (soilPercent < SOIL_DRY_LIMIT - 15) {
    display.print(F("very dry"));
  }
  else if (soilPercent < SOIL_DRY_LIMIT) {
    display.print(F("dry"));
  }
  else if (soilPercent > SOIL_WET_LIMIT) {
    display.print(F("wet"));
  }
  else {
    display.print(F("good"));
  }
}

void printAirStatus() {
  if (!ahtFound) {
    display.print(F("no sensor"));
  }
  else if (vpd < VPD_LOW_LIMIT) {
    display.print(F("humid"));
  }
  else if (vpd > VPD_HIGH_LIMIT) {
    display.print(F("dry"));
  }
  else {
    display.print(F("normal"));
  }
}

void printLightStatus() {
  if (!lightAverageReady) {
    display.print(F("learning"));
  }
  else if (light24hAveragePercent < LIGHT_DAILY_LOW_LIMIT) {
    display.print(F("low"));
  }
  else if (light24hAveragePercent > LIGHT_DAILY_HIGH_LIMIT) {
    display.print(F("high"));
  }
  else {
    display.print(F("good"));
  }
}

void printAdviceLine1(byte status) {
  switch (status) {
    case STATUS_VERY_DRY:
      display.print(F("Soil very dry"));
      break;

    case STATUS_DRY:
      display.print(F("Soil drying"));
      break;

    case STATUS_TOO_WET:
      display.print(F("Skip watering"));
      break;

    case STATUS_TOO_COLD:
      display.print(F("Find warmer spot"));
      break;

    case STATUS_TOO_HOT:
      display.print(F("Avoid strong heat"));
      break;

    case STATUS_AIR_DRY:
      display.print(F("Air is too dry"));
      break;

    case STATUS_LOW_AMBIENT:
      display.print(F("Ambient light low"));
      break;

    case STATUS_AHT_MISSING:
      display.print(F("Check wiring"));
      break;

    default:
      display.print(F("No action needed"));
      break;
  }
}

void printAdviceLine2(byte status) {
  switch (status) {
    case STATUS_VERY_DRY:
    case STATUS_DRY:
      display.print(F("Water gently"));
      break;

    case STATUS_TOO_WET:
      display.print(F("Let soil breathe"));
      break;

    case STATUS_TOO_COLD:
      display.print(F("Check room temp"));
      break;

    case STATUS_TOO_HOT:
      display.print(F("Check sunlight"));
      break;

    case STATUS_AIR_DRY:
      display.print(F("Humidity low"));
      break;

    case STATUS_LOW_AMBIENT:
      display.print(F("Try window area"));
      break;

    case STATUS_AHT_MISSING:
      display.print(F("AHT10 not found"));
      break;

    default:
      if (!lightAverageReady) {
        display.print(F("Light learning"));
      } else {
        display.print(F("Plant looks happy"));
      }
      break;
  }
}