/*
  Halloween Pumpkin
  Description:
  Hardware: Lightup using PIR sensor and on-board NeoPixels and Buzzer
  Library: Adafruit NeoPixel library
  Author: ROBO
  Date: October 2025 
*/

#include <Adafruit_NeoPixel.h>

#define PIXELS_PIN 8        // Define the pin where our NeoPixels are connected
#define NUMPIXELS 3  // Define the number of NeoPixels in our strip
#define BUZZER_PIN 9  // Define the pin where the Buzzer is connected

#define RED_PIXEL 0
#define ORANGE_PIXEL 1
#define LIGHTORANGE_PIXEL 2

#define MOTION_PIN 7  // Motion sensor input pin

#define WAIT 500

// Colors to be used defined in RGB values (0-255)
uint32_t redColor = Adafruit_NeoPixel::Color(255, 0, 0);
uint32_t orangeColor = Adafruit_NeoPixel::Color(255, 55, 0);
uint32_t lightorangeColor = Adafruit_NeoPixel::Color(255, 110, 0);
uint32_t offColor = Adafruit_NeoPixel::Color(0, 0, 0);

// Frequencies of the notes used in the song [Hz]
int cs6 = 1109;  
int fs5 = 740;
int d6 = 1175;
int c6 = 1047;
int f5 = 698;
int b5 = 988;
int e5 = 659;
int bf5 = 932;
int ef5 = 622;
int b4 = 494;
int g5 = 784;

int desired_duration = 4000;  // Desired melody duration [ms]
int note_duration = 200;  // [ms] tailored for specific song
int numTones = round(desired_duration/note_duration); // Number of notes to be played
int tones[] = { cs6, fs5, fs5, cs6, fs5, fs5, cs6, fs5, d6, fs5, // The full song's notes in order 
  cs6, fs5, fs5, cs6, fs5, fs5, cs6, fs5,  d6, fs5,  
  cs6, fs5, fs5, cs6, fs5, fs5, cs6, fs5,  d6, fs5,
 	cs6, fs5, fs5, cs6, fs5, fs5, cs6, fs5,  d6, fs5,
 	cs6,  fs5, fs5, cs6, fs5, fs5, cs6, fs5,  d6, fs5,
	cs6, fs5, fs5, cs6,  fs5, fs5, cs6, fs5,  d6, fs5,
  c6,  f5,  f5,  c6,  f5,  f5,  c6,  f5, cs6,  f5,
  c6,  f5,  f5,  c6,  f5,  f5,  c6,  f5, cs6,  f5,
  cs6, fs5, fs5, cs6, fs5, fs5, cs6, fs5,  d6, fs5,
  cs6, fs5, fs5, cs6, fs5, fs5, cs6, fs5,  d6, fs5,
  c6,  f5,  f5,  c6,  f5,  f5,  c6,  f5, cs6,  f5,
  c6,  f5,  f5,  c6,  f5,  f5,  c6,  f5, cs6,  f5,
  b5,  e5,  e5,  b5,  e5,  e5,  b5,  e5,  c6,  e5,
  b5,  e5,  e5,  b5,  e5,  e5,  b5,  e5,  c6,  e5,
  bf5, ef5, ef5, bf5, ef5, ef5, bf5, ef5,  b5, ef5,
  bf5, ef5, ef5, bf5, ef5, ef5, bf5, ef5,  b5, ef5,
  b5,  e5,  e5,  b5,  e5,  e5,  b5,  e5,  c6,  e5,
  b5,  e5,  e5,  b5,  e5,  e5,  b5,  e5,  c6,  e5,
  bf5, ef5,  ef5, bf5, ef5, ef5, bf5, ef5,  b5, ef5,
  bf5, ef5, ef5, bf5,  ef5, ef5, bf5, ef5,  b5, ef5,
  fs5,  b4,  b4, fs5,  b4,  b4, fs5,  b4,  g5,  b4,
  fs5,  b4,  b4, fs5,  b4,  b4, fs5,  b4,  g5,  b4,
  fs5,  b4,  b4, fs5,  b4,  b4, fs5,  b4,  g5,  b4,
  fs5,  b4,  b4, fs5,  b4,  b4, fs5,  b4,  g5,  b4,
  fs5,  b4,  b4, fs5,  b4,  b4, fs5,  b4,  g5,  b4,
  fs5,  b4,  b4,  fs5,  b4,  b4, fs5,  b4,  g5,  b4,
  fs5,  b4,  b4, fs5,  b4,  b4,  fs5,  b4,  g5,  b4,
  fs5,  b4,  b4, fs5,  b4,  b4, fs5,  b4,  g5,  b4,
  fs5,  b4,  b4, fs5,  b4,  b4, fs5,  b4,  g5,  b4};

// Initialize the NeoPixels
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIXELS_PIN, NEO_GRB + NEO_KHZ800);

int val = LOW;

void setup() {
  Serial.begin(9600); // initialize serial communication at 9600 bits per second

  pinMode(MOTION_PIN, INPUT); 
  pinMode(BUZZER_PIN, OUTPUT);

  pixels.begin();            // Initialize the NeoPixel strip
  pixels.setBrightness(255);   // Set an initial brightness (0-255)
  pixels.setPixelColor(RED_PIXEL, offColor); // Set to no color initially
  pixels.setPixelColor(ORANGE_PIXEL, offColor); 
  pixels.setPixelColor(LIGHTORANGE_PIXEL, offColor);
}

void loop() {
  val = digitalRead(MOTION_PIN);
  if (val == HIGH) {  // When motion sensor sends HIGH signal
    
    // Assign a color to each pixel
    pixels.setPixelColor(RED_PIXEL, redColor); 
    pixels.setPixelColor(ORANGE_PIXEL, orangeColor); 
    pixels.setPixelColor(LIGHTORANGE_PIXEL, lightorangeColor);

    for (int i = 0; i < numTones; i++){  // For each note (from start to numTones)
      tone(BUZZER_PIN, tones[i]); // Play note "i"

      // Sequence to flicker twice during each note
      pixels.setBrightness(random(75, 255));  // Randomly varied brightness for a flickering effect
      pixels.show();
      delay(0.8*note_duration);

      noTone(BUZZER_PIN);

      pixels.setBrightness(random(75, 255));
      pixels.show();
      delay(0.2*note_duration); // Total duration of each iteration = note_duration
    }

    //Turn off buzzer
    noTone(BUZZER_PIN); 

    // Turn off pixels
    pixels.setPixelColor(RED_PIXEL, offColor); 
    pixels.setPixelColor(ORANGE_PIXEL, offColor); 
    pixels.setPixelColor(LIGHTORANGE_PIXEL, offColor);
    pixels.show();

    delay(WAIT);
  }
}
