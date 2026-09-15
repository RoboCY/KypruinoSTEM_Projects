/*
  ============================================================
  MiniGreenhouse — Kypruino Smart Mini Greenhouse
  Part of the Kypruino STEM Projects by ROBO (robo.com.cy)
  Guide: https://robo.com.cy/blogs/blog/kypruino-mini-smart-greenhouse
  ============================================================

  Humidity-Based Watering Version - AHT10 Sensor

  Features:
  - Reads temperature and humidity every few minutes.
  - If air humidity is too low, the pump runs briefly to fill the water channel.
  - If temperature or humidity is too high, the fan turns on for ventilation.
  - No soil moisture sensor is used.

  Sensor:
  - AHT10 temperature/humidity sensor using I2C

  Outputs:
  - Pump driver on D10
  - Fan driver on D11

  IMPORTANT:
  Do not power the pump or fan directly from Kypruino I/O pins.
  Use a MOSFET/transistor driver and an external power supply if needed.
*/

// -------------------- Libraries --------------------

#include <Wire.h>
#include <Adafruit_AHTX0.h>

// -------------------- Pin Settings --------------------

#define PUMP_PIN 10
#define FAN_PIN 11

// -------------------- Sensor Object --------------------

Adafruit_AHTX0 aht;

// -------------------- Timing Settings --------------------

// Check sensors every 5 minutes
const unsigned long CHECK_INTERVAL_MS = 5UL * 60UL * 1000UL;

// How long the pump runs when watering is needed
const unsigned long PUMP_TIME_MS = 1500;  // start with 1.5 seconds

// -------------------- Humidity-Based Watering --------------------

// If humidity drops below this value, the system adds water to the channel.
// Tune this after testing inside the actual greenhouse.
const float HUMIDITY_WATER_THRESHOLD = 55.0;

// -------------------- Fan Thresholds --------------------

// Fan turns ON if temperature is too high
const float TEMP_FAN_ON = 30.0;
const float TEMP_FAN_OFF = 28.0;

// Fan turns ON if humidity is too high
const float HUMIDITY_FAN_ON = 85.0;
const float HUMIDITY_FAN_OFF = 80.0;

// -------------------- Variables --------------------

unsigned long lastCheckTime = 0;
bool fanOn = false;

// -------------------- Setup --------------------

void setup() {
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);

  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);

  Serial.begin(9600);

  Serial.println("Smart Mini Greenhouse Started");
  Serial.println("Humidity-based watering mode");
  Serial.println("Sensor: AHT10");

  if (!aht.begin()) {
    Serial.println("Could not find AHT10 sensor. Check wiring.");
    while (1) {
      delay(10);
    }
  }

  Serial.println("AHT10 sensor found.");

  // Force first check immediately
  lastCheckTime = millis() - CHECK_INTERVAL_MS;
}

// -------------------- Main Loop --------------------

void loop() {
  if (millis() - lastCheckTime >= CHECK_INTERVAL_MS) {
    lastCheckTime = millis();

    sensors_event_t humidityEvent;
    sensors_event_t temperatureEvent;

    aht.getEvent(&humidityEvent, &temperatureEvent);

    float humidity = humidityEvent.relative_humidity;
    float temperature = temperatureEvent.temperature;

    Serial.println("--------------------");

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" C");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    // -------------------- Fan Control --------------------

    if (temperature > TEMP_FAN_ON || humidity > HUMIDITY_FAN_ON) {
      fanOn = true;
    }

    if (temperature < TEMP_FAN_OFF && humidity < HUMIDITY_FAN_OFF) {
      fanOn = false;
    }

    digitalWrite(FAN_PIN, fanOn ? HIGH : LOW);

    Serial.print("Fan: ");
    Serial.println(fanOn ? "ON" : "OFF");

    // -------------------- Watering Control --------------------

    if (humidity < HUMIDITY_WATER_THRESHOLD) {
      Serial.println("Humidity is low. Pumping water into the channel...");

      digitalWrite(PUMP_PIN, HIGH);
      delay(PUMP_TIME_MS);
      digitalWrite(PUMP_PIN, LOW);

      Serial.println("Pump stopped. Waiting until next check.");
    } else {
      Serial.println("Humidity is OK. No watering needed.");
    }
  }
}
