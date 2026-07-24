/*
  ============================================================
  Kypruino Mini ATM / NFC Bank Machine
  Student-Friendly Version
  ============================================================

  What this project does:
  - The user taps an RFID/NFC card.
  - If the card is new, it is assigned to the next account name.
  - If the card is already known, the account is opened.
  - The user can withdraw, deposit, transfer money, or exit.
  - Account data is saved in EEPROM, so it remains after power-off.
  - The OLED shows the menu, balances, and amount editor.
  - LEDs and buzzer give feedback.

  ------------------------------------------------------------
  Button Controls
  ------------------------------------------------------------

  Account Menu:
    A = Withdraw
    B = Deposit
    C = Transfer
    D = Exit

  Amount Editor:
    D = move selected digit left
    B = move selected digit right
    A = increase selected digit
    C = confirm
    Hold C = cancel

  ------------------------------------------------------------
  Wiring
  ------------------------------------------------------------

  RC522 RFID/NFC Reader:
    SDA   -> D10
    SCK   -> D13
    MOSI  -> D11
    MISO  -> D12
    RST   -> D5
    3.3V  -> 3.3V
    GND   -> GND

  OLED 128x32:
    SDA -> SDA (dedicated port)
    SCL -> SCL
    VCC -> VCC
    GND -> GND

  Built-in Kypruino Buttons:
    A -> D7
    B -> D6
    C -> D4
    D -> D2

  Built-in Kypruino:
    Buzzer -> D9
    NeoPixels -> D8
*/

#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <stdint.h>

// ============================================================
// Pin Settings
// ============================================================

#define BUTTON_A_PIN 7
#define BUTTON_B_PIN 6
#define BUTTON_C_PIN 4
#define BUTTON_D_PIN 2

#define LED_PIN 8
#define LED_COUNT 3

#define BUZZER_PIN 9

#define RFID_SS_PIN 10
#define RFID_RST_PIN 5

// ============================================================
// OLED Settings
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================================
// RFID and LED Objects
// ============================================================

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
Adafruit_NeoPixel leds(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ============================================================
// Money Settings
// ============================================================

// Money is stored in cents.
// Example:
// EUR 1.00 = 100 cents
// EUR 10.50 = 1050 cents
typedef int64_t Money;

// Starting balance for every new account.
const Money STARTING_BALANCE_CENTS = 0;

// Maximum balance:
// EUR 9999999999.99 = 999999999999 cents (9.99 Billion)
const Money MAX_BALANCE_CENTS = 999999999999LL;

// ============================================================
// Account Settings
// ============================================================

// Add or change account names here.
const char* accountNames[] = {
  "Andreas",
  "Maria",
  "George",
  "Kostas"
};

const byte ACCOUNT_COUNT = sizeof(accountNames) / sizeof(accountNames[0]);

// Set this to true, upload once, then set it back to false and upload again.
// This clears all saved cards and balances.
const bool RESET_ALL_ACCOUNTS = false;

// ============================================================
// EEPROM Settings
// ============================================================

const int EEPROM_MAGIC_ADDR = 0;
const byte EEPROM_MAGIC_VALUE = 123;
const int EEPROM_START_ADDR = 4;

const byte MAX_UID_LENGTH = 10;

// This is what gets saved for each account.
struct AccountRecord {
  byte registered;
  byte uidLength;
  byte uid[MAX_UID_LENGTH];
  Money balanceCents;
};

const int RECORD_SIZE = sizeof(AccountRecord);

// ============================================================
// Button Settings
// ============================================================

struct ButtonData {
  byte pin;
  bool stableState;
  bool lastReading;
  unsigned long lastChangeTime;
  unsigned long pressStartTime;
  bool longPressSent;
};

ButtonData buttonA = {BUTTON_A_PIN, HIGH, HIGH, 0, 0, false};
ButtonData buttonB = {BUTTON_B_PIN, HIGH, HIGH, 0, 0, false};
ButtonData buttonC = {BUTTON_C_PIN, HIGH, HIGH, 0, 0, false};
ButtonData buttonD = {BUTTON_D_PIN, HIGH, HIGH, 0, 0, false};

const unsigned long DEBOUNCE_MS = 35;
const unsigned long LONG_PRESS_MS = 800;

// ============================================================
// Program State
// ============================================================

// -1 means no account is currently open.
int currentAccount = -1;

// ============================================================
// EEPROM Helper Functions
// ============================================================

int getAccountAddress(byte accountIndex) {
  return EEPROM_START_ADDR + accountIndex * RECORD_SIZE;
}

void readAccount(byte accountIndex, AccountRecord &account) {
  EEPROM.get(getAccountAddress(accountIndex), account);
}

void saveAccount(byte accountIndex, AccountRecord &account) {
  EEPROM.put(getAccountAddress(accountIndex), account);
}

void clearAllAccounts() {
  AccountRecord emptyAccount;

  emptyAccount.registered = 0;
  emptyAccount.uidLength = 0;
  emptyAccount.balanceCents = 0;

  for (byte i = 0; i < MAX_UID_LENGTH; i++) {
    emptyAccount.uid[i] = 0;
  }

  for (byte i = 0; i < ACCOUNT_COUNT; i++) {
    saveAccount(i, emptyAccount);
  }

  EEPROM.update(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
}

void setupEEPROM() {
  if (RESET_ALL_ACCOUNTS) {
    clearAllAccounts();
  }

  if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_VALUE) {
    clearAllAccounts();
  }
}

// ============================================================
// Account Helper Functions
// ============================================================

bool sameUID(byte* uidA, byte lengthA, byte* uidB, byte lengthB) {
  if (lengthA != lengthB) {
    return false;
  }

  for (byte i = 0; i < lengthA; i++) {
    if (uidA[i] != uidB[i]) {
      return false;
    }
  }

  return true;
}

int findAccountByUID(byte* uid, byte uidLength) {
  AccountRecord account;

  for (byte i = 0; i < ACCOUNT_COUNT; i++) {
    readAccount(i, account);

    if (account.registered == 1) {
      if (sameUID(uid, uidLength, account.uid, account.uidLength)) {
        return i;
      }
    }
  }

  return -1;
}

int findFreeAccountSlot() {
  AccountRecord account;

  for (byte i = 0; i < ACCOUNT_COUNT; i++) {
    readAccount(i, account);

    if (account.registered != 1) {
      return i;
    }
  }

  return -1;
}

int registerNewCard(byte* uid, byte uidLength) {
  int freeSlot = findFreeAccountSlot();

  if (freeSlot < 0) {
    return -1;
  }

  AccountRecord newAccount;

  newAccount.registered = 1;
  newAccount.uidLength = uidLength;
  newAccount.balanceCents = STARTING_BALANCE_CENTS;

  for (byte i = 0; i < MAX_UID_LENGTH; i++) {
    newAccount.uid[i] = 0;
  }

  for (byte i = 0; i < uidLength && i < MAX_UID_LENGTH; i++) {
    newAccount.uid[i] = uid[i];
  }

  saveAccount(freeSlot, newAccount);

  return freeSlot;
}

Money getBalance(byte accountIndex) {
  AccountRecord account;
  readAccount(accountIndex, account);

  return account.balanceCents;
}

void setBalance(byte accountIndex, Money newBalance) {
  AccountRecord account;
  readAccount(accountIndex, account);

  account.balanceCents = newBalance;

  saveAccount(accountIndex, account);
}

// ============================================================
// Money Display Functions
// ============================================================

void moneyToText(Money cents, char* text, int textSize) {
  if (cents < 0) {
    cents = 0;
  }

  Money euros = cents / 100;
  int centPart = cents % 100;

  char euroDigits[20];
  int digitCount = 0;

  if (euros == 0) {
    euroDigits[digitCount++] = '0';
  } else {
    char reversedDigits[20];
    int reversedCount = 0;

    while (euros > 0 && reversedCount < 19) {
      reversedDigits[reversedCount++] = '0' + (euros % 10);
      euros = euros / 10;
    }

    while (reversedCount > 0) {
      euroDigits[digitCount++] = reversedDigits[--reversedCount];
    }
  }

  euroDigits[digitCount] = '\0';

  snprintf(text, textSize, "EUR %s.%02d", euroDigits, centPart);
}

void printMoney(int x, int y, Money cents, byte size) {
  char moneyText[24];

  moneyToText(cents, moneyText, sizeof(moneyText));

  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, y);
  display.print(moneyText);
}

// ============================================================
// OLED Screen Functions
// ============================================================

void showATM() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(3);
  display.setCursor(36, 0);
  display.print("ATM");

  display.setTextSize(1);
  display.setCursor(40, 25);
  display.print("TAP CARD");

  display.display();
}

void showMessage(const char* line1, const char* line2, int delayTime) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 4);
  display.print(line1);

  display.setCursor(0, 18);
  display.print(line2);

  display.display();

  if (delayTime > 0) {
    delay(delayTime);
  }
}

void showAccountMenu(byte accountIndex) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Line 1: account name
  display.setCursor(0, 0);
  display.print(accountNames[accountIndex]);

  // Line 2: account balance
  printMoney(0, 8, getBalance(accountIndex), 1);

  // Line 3 and 4: menu options
  display.setCursor(0, 16);
  display.print("A:OUT      B:IN");

  display.setCursor(0, 24);
  display.print("C:SEND     D:EXIT");

  display.display();
}

void showSecondCardScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 4);
  display.print("TAP SECOND CARD");

  display.setCursor(0, 20);
  display.print("C HOLD: CANCEL");

  display.display();
}

void showTwoBalances(byte firstAccount, byte secondAccount) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print(accountNames[firstAccount]);

  printMoney(0, 8, getBalance(firstAccount), 1);

  display.setCursor(0, 16);
  display.print(accountNames[secondAccount]);

  printMoney(0, 24, getBalance(secondAccount), 1);

  display.display();

  delay(5000);
}

// ============================================================
// LED and Buzzer Feedback
// ============================================================

void turnOffLEDs() {
  leds.clear();
  leds.show();
}

void cardReadAnimation() {
  turnOffLEDs();

  leds.setPixelColor(2, leds.Color(255, 0, 0));
  leds.show();
  delay(120);

  leds.setPixelColor(1, leds.Color(255, 180, 0));
  leds.show();
  delay(120);

  leds.setPixelColor(0, leds.Color(0, 255, 0));
  leds.show();
  delay(120);

  tone(BUZZER_PIN, 1200, 90);
  delay(120);

  turnOffLEDs();
}

void successAnimation() {
  for (int repeat = 0; repeat < 2; repeat++) {
    for (int i = 0; i < LED_COUNT; i++) {
      leds.clear();
      leds.setPixelColor(i, leds.Color(0, 255, 0));
      leds.show();
      delay(55);
    }
  }

  turnOffLEDs();

  tone(BUZZER_PIN, 1200, 80);
  delay(120);

  tone(BUZZER_PIN, 1600, 80);
  delay(120);

  noTone(BUZZER_PIN);
}

void declineAnimation() {
  for (int repeat = 0; repeat < 2; repeat++) {
    for (int i = 0; i < LED_COUNT; i++) {
      leds.setPixelColor(i, leds.Color(255, 0, 0));
    }

    leds.show();

    tone(BUZZER_PIN, 250, 120);
    delay(180);

    turnOffLEDs();
    delay(100);
  }

  noTone(BUZZER_PIN);
}

void lowBeep() {
  tone(BUZZER_PIN, 250, 120);
  delay(150);
  noTone(BUZZER_PIN);
}

void clickBeep() {
  tone(BUZZER_PIN, 1000, 40);
  delay(50);
  noTone(BUZZER_PIN);
}

// ============================================================
// Button Reading
// ============================================================

char updateButton(ButtonData &button, char shortPress, char longPress) {
  bool reading = digitalRead(button.pin);

  // If the reading changed, restart the debounce timer.
  if (reading != button.lastReading) {
    button.lastChangeTime = millis();
    button.lastReading = reading;
  }

  // Accept a change only if it stays stable for a short time.
  if (millis() - button.lastChangeTime > DEBOUNCE_MS) {
    if (reading != button.stableState) {
      button.stableState = reading;

      // Button pressed.
      if (button.stableState == LOW) {
        button.pressStartTime = millis();
        button.longPressSent = false;
      }

      // Button released.
      else {
        if (!button.longPressSent) {
          return shortPress;
        }
      }
    }
  }

  // Detect long press while the button is still held down.
  if (button.stableState == LOW && !button.longPressSent && longPress != 0) {
    if (millis() - button.pressStartTime >= LONG_PRESS_MS) {
      button.longPressSent = true;
      return longPress;
    }
  }

  return 0;
}

char getButtonEvent() {
  char event;

  event = updateButton(buttonA, 'A', 0);
  if (event) return event;

  event = updateButton(buttonB, 'B', 0);
  if (event) return event;

  // X means C was held down.
  event = updateButton(buttonC, 'C', 'X');
  if (event) return event;

  event = updateButton(buttonD, 'D', 0);
  if (event) return event;

  return 0;
}

// ============================================================
// RFID Card Reading
// ============================================================

bool readCard(byte* uid, byte &uidLength) {
  if (!rfid.PICC_IsNewCardPresent()) {
    return false;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    return false;
  }

  uidLength = rfid.uid.size;

  for (byte i = 0; i < uidLength && i < MAX_UID_LENGTH; i++) {
    uid[i] = rfid.uid.uidByte[i];
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  // Small delay so one card tap is not read many times.
  delay(500);

  return true;
}

// ============================================================
// Amount Editor
// ============================================================

// The amount editor stores digits like this:
// 0000000000.00
// 12 digits total, with the decimal point before digit 10.
Money digitsToMoney(byte digits[12]) {
  Money cents = 0;

  for (byte i = 0; i < 12; i++) {
    cents = cents * 10 + digits[i];
  }

  return cents;
}

void drawAmountEditor(byte digits[12], byte firstVisibleDigit, byte selectedDigit, const char* title) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Top line: title and OK instruction.
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print(title);

  display.setCursor(92, 0);
  display.print("C:OK");

  // Second line: movement instructions.
  display.setCursor(0, 9);
  display.print("<D      B>      A^");

  // Count how many characters need to be shown.
  int characterCount = 12 - firstVisibleDigit;

  // Add one character for the decimal point.
  if (firstVisibleDigit <= 9) {
    characterCount++;
  }

  // Use bigger numbers when there is enough space.
  byte numberSize;

  if (characterCount <= 7) {
    numberSize = 2;
  } else {
    numberSize = 1;
  }

  int characterWidth = 6 * numberSize;
  int totalWidth = characterCount * characterWidth;
  int x = (128 - totalWidth) / 2;

  int y;

  if (numberSize == 2) {
    y = 16;
  } else {
    y = 22;
  }

  display.setTextSize(numberSize);

  // Print all visible digits.
  for (byte i = firstVisibleDigit; i < 12; i++) {
    // Add the decimal point before digit 10.
    if (i == 10) {
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(x, y);
      display.print(".");
      x += characterWidth;
    }

    // Highlight the selected digit.
    if (i == selectedDigit) {
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }

    display.setCursor(x, y);
    display.print(digits[i]);

    x += characterWidth;
  }

  display.setTextColor(SSD1306_WHITE);
  display.display();
}

Money enterAmount(const char* title) {
  byte digits[12] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };

  // Start by showing 0.00.
  byte firstVisibleDigit = 9;
  byte selectedDigit = 11;

  drawAmountEditor(digits, firstVisibleDigit, selectedDigit, title);

  while (true) {
    char button = getButtonEvent();

    // D moves left.
    if (button == 'D') {
      clickBeep();

      if (selectedDigit > firstVisibleDigit) {
        selectedDigit--;
      } else {
        if (firstVisibleDigit > 0) {
          firstVisibleDigit--;
          selectedDigit = firstVisibleDigit;
        } else {
          lowBeep();
        }
      }

      drawAmountEditor(digits, firstVisibleDigit, selectedDigit, title);
    }

    // B moves right.
    if (button == 'B') {
      clickBeep();

      if (selectedDigit < 11) {
        selectedDigit++;
      } else {
        lowBeep();
      }

      // Hide unnecessary leading zeros when possible.
      while (firstVisibleDigit < 9 &&
             digits[firstVisibleDigit] == 0 &&
             selectedDigit > firstVisibleDigit) {
        firstVisibleDigit++;
      }

      drawAmountEditor(digits, firstVisibleDigit, selectedDigit, title);
    }

    // A increases the selected digit.
    if (button == 'A') {
      clickBeep();

      digits[selectedDigit]++;

      if (digits[selectedDigit] > 9) {
        digits[selectedDigit] = 0;
      }

      drawAmountEditor(digits, firstVisibleDigit, selectedDigit, title);
    }

    // C confirms the amount.
    if (button == 'C') {
      Money amount = digitsToMoney(digits);

      if (amount == 0) {
        showMessage("INVALID AMOUNT", "Value cannot be 0", 900);
        drawAmountEditor(digits, firstVisibleDigit, selectedDigit, title);
      } else {
        return amount;
      }
    }

    // Holding C cancels the amount entry.
    if (button == 'X') {
      lowBeep();
      return -1;
    }
  }
}

// ============================================================
// Bank Operations
// ============================================================

void depositMoney(byte accountIndex) {
  clickBeep();

  Money amount = enterAmount("DEPOSIT");

  if (amount < 0) {
    showAccountMenu(accountIndex);
    return;
  }

  Money balance = getBalance(accountIndex);

  // Check if the deposit would pass the maximum allowed balance.
  if (amount > MAX_BALANCE_CENTS - balance) {
    declineAnimation();
    showMessage("DECLINED", "Balance limit", 1300);
    showAccountMenu(accountIndex);
    return;
  }

  balance = balance + amount;
  setBalance(accountIndex, balance);

  successAnimation();
  showMessage("DEPOSITED", "Operation complete", 900);
  showAccountMenu(accountIndex);
}

void withdrawMoney(byte accountIndex) {
  clickBeep();

  Money amount = enterAmount("WITHDRAW");

  if (amount < 0) {
    showAccountMenu(accountIndex);
    return;
  }

  Money balance = getBalance(accountIndex);

  // The user cannot withdraw more than they have.
  if (amount > balance) {
    declineAnimation();
    showMessage("DECLINED", "Insufficient funds", 1300);
    showAccountMenu(accountIndex);
    return;
  }

  balance = balance - amount;
  setBalance(accountIndex, balance);

  successAnimation();
  showMessage("WITHDRAWN", "Operation complete", 900);
  showAccountMenu(accountIndex);
}

void transferMoney(byte senderIndex) {
  clickBeep();

  byte uid[MAX_UID_LENGTH];
  byte uidLength = 0;

  showSecondCardScreen();

  int receiverIndex = -1;

  // Wait until a valid second card is tapped.
  while (receiverIndex < 0) {
    char button = getButtonEvent();

    if (button == 'X') {
      lowBeep();
      showAccountMenu(senderIndex);
      return;
    }

    if (readCard(uid, uidLength)) {
      receiverIndex = findAccountByUID(uid, uidLength);

      if (receiverIndex < 0) {
        declineAnimation();
        showMessage("UNKNOWN CARD", "Tap valid card", 1000);
        showSecondCardScreen();
      } else if (receiverIndex == senderIndex) {
        receiverIndex = -1;
        declineAnimation();
        showMessage("INVALID", "Same card", 1000);
        showSecondCardScreen();
      } else {
        cardReadAnimation();
      }
    }
  }

  Money amount = enterAmount("TRANSFER");

  if (amount < 0) {
    showAccountMenu(senderIndex);
    return;
  }

  Money senderBalance = getBalance(senderIndex);
  Money receiverBalance = getBalance(receiverIndex);

  // Sender must have enough money.
  if (amount > senderBalance) {
    declineAnimation();
    showMessage("DECLINED", "Insufficient funds", 1300);
    showAccountMenu(senderIndex);
    return;
  }

  // Receiver must not go over the maximum balance.
  if (amount > MAX_BALANCE_CENTS - receiverBalance) {
    declineAnimation();
    showMessage("DECLINED", "Receiver limit", 1300);
    showAccountMenu(senderIndex);
    return;
  }

  senderBalance = senderBalance - amount;
  receiverBalance = receiverBalance + amount;

  setBalance(senderIndex, senderBalance);
  setBalance(receiverIndex, receiverBalance);

  successAnimation();
  showMessage("TRANSFER", "Operation complete", 900);

  showTwoBalances(senderIndex, receiverIndex);
  showAccountMenu(senderIndex);
}

// ============================================================
// Opening a Card Account
// ============================================================

void openCard(byte* uid, byte uidLength) {
  int accountIndex = findAccountByUID(uid, uidLength);

  // If the card is not known, register it.
  if (accountIndex < 0) {
    accountIndex = registerNewCard(uid, uidLength);

    if (accountIndex < 0) {
      declineAnimation();
      showMessage("NO SLOTS LEFT", "Cannot register", 1400);
      showATM();
      return;
    }

    cardReadAnimation();

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("CARD ASSIGNED");

    display.setTextSize(2);
    display.setCursor(0, 10);
    display.print(accountNames[accountIndex]);

    display.setTextSize(1);
    printMoney(70, 22, getBalance(accountIndex), 1);

    display.display();
    delay(1500);
  }

  // If the card is already known, simply open it.
  else {
    cardReadAnimation();

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print(accountNames[accountIndex]);

    printMoney(0, 22, getBalance(accountIndex), 1);

    display.display();
    delay(1200);
  }

  currentAccount = accountIndex;
  showAccountMenu(currentAccount);
}

// ============================================================
// Setup
// ============================================================

void setup() {
  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  pinMode(BUTTON_B_PIN, INPUT_PULLUP);
  pinMode(BUTTON_C_PIN, INPUT_PULLUP);
  pinMode(BUTTON_D_PIN, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);

  leds.begin();
  leds.setBrightness(20);
  turnOffLEDs();

  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.display();

  SPI.begin();
  rfid.PCD_Init();

  setupEEPROM();

  showATM();
}

// ============================================================
// Main Loop
// ============================================================

void loop() {
  byte uid[MAX_UID_LENGTH];
  byte uidLength = 0;

  // If no account is open, wait for a card.
  if (currentAccount < 0) {
    if (readCard(uid, uidLength)) {
      openCard(uid, uidLength);
    }

    return;
  }

  // If an account is open, listen for menu button presses.
  char button = getButtonEvent();

  if (button == 'A') {
    withdrawMoney(currentAccount);
  }

  if (button == 'B') {
    depositMoney(currentAccount);
  }

  if (button == 'C') {
    transferMoney(currentAccount);
  }

  if (button == 'D') {
    clickBeep();
    currentAccount = -1;
    showATM();
  }
}