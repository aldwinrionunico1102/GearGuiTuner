// EmbeMetre A5 - Automatic Acoustic Guitar Tuner
// Final Code for Testing: Using Serial as Input

#include "arduinoFFT.h"     // FFT Library (still included for compatibility)
#include <Stepper.h>        // Stepper Library
#include <LCD_I2C.h>        // LCD Library

// MICROPHONE SENSOR (Not used in test mode)
const uint16_t samples = 512;
const double samplingFrequency = 4000.0;
double vReal[samples];
double vImag[samples];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, samples, samplingFrequency);
const uint8_t micPin = A0;

// STEPPER Initializations
const int stepsPerRevolution = 2048;
const int stepSpeed = 10;
Stepper Stepper1(stepsPerRevolution, 30, 32, 31, 33);
Stepper Stepper2(stepsPerRevolution, 38, 40, 39, 41);
Stepper Stepper3(stepsPerRevolution, 46, 48, 47, 49);

// LCD Initializations
LCD_I2C lcd(0x27, 16, 2);

// TOUCH Initializations
const int touchPin = 2;           // System Switch
bool lastTouchState = LOW;
bool currentTouchState = LOW;

// BUTTONS Initializations
const int button1Pin = 4;         // Mode Switch
const int button2Pin = 5;         // Next String / Reset
bool lastButton1State = LOW;
bool currentButton1State = LOW;
bool lastButton2State = LOW;
bool currentButton2State = LOW;

// LED Initialization
const int ledPin = 7;

// Other Initializations
bool systemOn = false;
bool justTurnedOn = false;
bool justTurnedOff = false;

int modeMenu = 0;     // 0 - Auto(Batch1), 1 - Auto(Batch2), 2 - Manual(Batch1), 3 - Manual(Batch2)
int selectedString = 0;     // String 0-5 = LowE-HighE
double targetFrequency[6] = {82.41, 110.00, 146.83, 196.00, 246.94, 329.63};
const char* stringName[6] = {"LOW E", "A", "D", "G", "B", "HIGH E"};

enum Phase { phaseDetecting, phaseResult };
Phase currentPhase = phaseDetecting;

const unsigned long detectDuration = 3000UL;
const unsigned long displayDuration = 3000UL;
unsigned long phaseStartMillis = 0UL;

double freqTolerance = 1.0;
double showTolerance = 3.0;

double lastDetectedFreq = 0.0;
bool isTuned = false;
bool tuningAdjustedForCurrentDetection = false;

void setup() {
  Serial.begin(115200);

  analogReference(DEFAULT);

  Stepper1.setSpeed(stepSpeed);
  Stepper2.setSpeed(stepSpeed);
  Stepper3.setSpeed(stepSpeed);

  lcd.begin();
  lcd.clear();
  lcd.noBacklight();

  pinMode(touchPin, INPUT);
  pinMode(button1Pin, INPUT);
  pinMode(button2Pin, INPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  systemOn = false;
  justTurnedOn = false;
  justTurnedOff = false;
  modeMenu = 0;
  selectedString = 0;
  currentPhase = phaseDetecting;
  tuningAdjustedForCurrentDetection = false;
  isTuned = false;
  lastDetectedFreq = 0.0;
  phaseStartMillis = millis();
}

void loop() {
  currentTouchState = digitalRead(touchPin);
  currentButton1State = digitalRead(button1Pin);
  currentButton2State = digitalRead(button2Pin);

  if (currentTouchState == HIGH && lastTouchState == LOW) {
    systemOn = !systemOn;
    if (systemOn) {
      justTurnedOn = true;
    } else {
      justTurnedOff = true;
    }
    delay(100);
  }
  lastTouchState = currentTouchState;

  if (justTurnedOn) {
    digitalWrite(ledPin, LOW);
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ACOUSTIC GUITAR ");
    lcd.setCursor(0, 1);
    lcd.print(" AUTOMATIC TUNER");
    delay(3000);

    displayMode(modeMenu);

    if (modeMenu % 2 == 0)
      selectedString = 0;
    else
      selectedString = 3;

    currentPhase = phaseDetecting;
    phaseStartMillis = millis();
    tuningAdjustedForCurrentDetection = false;
    isTuned = false;
    justTurnedOn = false;
  }

  if (justTurnedOff) {
    digitalWrite(ledPin, LOW);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Tuning system is");
    lcd.setCursor(0, 1);
    lcd.print("shutting down...");
    delay(3000);

    lcd.clear();
    lcd.noBacklight();

    modeMenu = 0;
    selectedString = 0;
    currentPhase = phaseDetecting;
    isTuned = false;
    tuningAdjustedForCurrentDetection = false;
    justTurnedOff = false;
  }

  if (!systemOn) {
    digitalWrite(ledPin, LOW);
    lastButton1State = currentButton1State;
    lastButton2State = currentButton2State;
    return;
  }

  bool button1Pressed = (currentButton1State == HIGH && lastButton1State == LOW);
  bool button2Pressed = (currentButton2State == HIGH && lastButton2State == LOW);

  if (currentPhase == phaseResult) {
    if (button1Pressed) {
      digitalWrite(ledPin, LOW);

      modeMenu++;
      if (modeMenu > 3) modeMenu = 0;
      if (modeMenu % 2 == 0)
        selectedString = 0;
      else
        selectedString = 3;

      displayMode(modeMenu);

      lcd.clear();
      currentPhase = phaseDetecting;
      phaseStartMillis = millis();
      tuningAdjustedForCurrentDetection = false;
      isTuned = false;
      delay(200);
    }

    if (button2Pressed) {
      if (modeMenu == 0)
        selectedString = 0;
      else if (modeMenu == 1)
        selectedString = 3;
      else if (modeMenu == 2) {
        selectedString++;
        if (selectedString > 2) selectedString = 0;
      } else if (modeMenu == 3) {
        selectedString++;
        if (selectedString > 5) selectedString = 3;
      }

      currentPhase = phaseDetecting;
      phaseStartMillis = millis();
      tuningAdjustedForCurrentDetection = false;
      isTuned = false;
      lcd.clear();
      delay(200);
    }
  }

  if (currentPhase == phaseDetecting) {
    digitalWrite(ledPin, LOW);
    displayStatusLine1(selectedString);
    lcd.setCursor(0, 1);
    lcd.print("Detecting now...   ");

    lastDetectedFreq = getUserFrequency(selectedString);    // Instead of FFT, wait for user input frequency
    double diff = lastDetectedFreq - targetFrequency[selectedString];
    isTuned = (abs(diff) < showTolerance);
    
    tuningAdjustedForCurrentDetection = false;

    displayStatusLine2(selectedString, lastDetectedFreq);

    currentPhase = phaseResult;
    phaseStartMillis = millis();
  }
  else if (currentPhase == phaseResult) {
    digitalWrite(ledPin, HIGH);

    if (!isTuned && !tuningAdjustedForCurrentDetection) {     // Adjusts stepper motor since not in tune
      if (lastDetectedFreq > 0.0 && lastDetectedFreq != 62.5) {    // Prevents reacting to idle / unstable sound
        double diff = lastDetectedFreq - targetFrequency[selectedString];
        adjustTuning(selectedString, diff);
      }
      tuningAdjustedForCurrentDetection = true;
    }

    if ((modeMenu == 0 || modeMenu == 1) && isTuned) {
      if (millis() - phaseStartMillis >= displayDuration) {
        if (modeMenu == 0) {
          selectedString++;
          if (selectedString > 2) selectedString = 0;
        } else {
          selectedString++;
          if (selectedString > 5) selectedString = 3;
        }

        currentPhase = phaseDetecting;
        phaseStartMillis = millis();
        tuningAdjustedForCurrentDetection = false;
        isTuned = false;
        lcd.clear();
      }
    } else {
      if (millis() - phaseStartMillis >= displayDuration) {
        currentPhase = phaseDetecting;
        phaseStartMillis = millis();
        tuningAdjustedForCurrentDetection = false;
        lcd.clear();
      }
    }
  }

  lastButton1State = currentButton1State;
  lastButton2State = currentButton2State;
}

// FUNCTIONS --------------------------------------------------------
void displayMode(int mode) {
  lcd.clear();
  lcd.setCursor(0, 0);
  switch (mode) {
    case 0:
      lcd.print(" AUTOMATIC MODE ");
      lcd.setCursor(0, 1);
      lcd.print(" Low E - A - D  ");
      break;
    case 1:
      lcd.print(" AUTOMATIC MODE ");
      lcd.setCursor(0, 1);
      lcd.print(" G - B - High E ");
      break;
    case 2:
      lcd.print("  MANUAL MODE   ");
      lcd.setCursor(0, 1);
      lcd.print(" Low E - A - D  ");
      break;
    case 3:
      lcd.print("  MANUAL MODE   ");
      lcd.setCursor(0, 1);
      lcd.print(" G - B - High E ");
      break;
  }
  delay(1500);
}

void displayStatusLine1(int stringIndex) {
  lcd.setCursor(0, 0);
  lcd.print("String: ");
  lcd.print(stringName[stringIndex]);
  lcd.print("       ");
}

void displayStatusLine2(int stringIndex, double frequency) {
  double diff = frequency - targetFrequency[stringIndex];
  lcd.setCursor(0, 1);

  if (frequency == 0.0) {
    lcd.print(" Unstable Sound ");                // Sound is not strong enough to count as valid frequency
    isTuned = false;
  } else if (abs(diff) < showTolerance) {
    lcd.print(" STRING IN TUNE ");                // Close enough to target frequency
    isTuned = true;
  } else if (diff > 0) {
    lcd.print("Too High +");                      // More than the target frequency
    lcd.print((int)round(diff));
    lcd.print("Hz    ");
    isTuned = false;
  } else {
    lcd.print("Too Low -");                       // Less than the target frequency
    lcd.print((int)round(-diff));
    lcd.print("Hz     ");
    isTuned = false;
  }
}

// REPLACEMENT FUNCTION: User enters frequency manually
double getUserFrequency(int stringIndex) {
  Serial.print("\nEnter detected frequency for string ");
  Serial.print(stringName[stringIndex]);
  Serial.print(" (Hz): ");

  while (Serial.available() == 0) {
    // Wait for user input
  }

  double userFreq = Serial.parseFloat();  // Read input
  while (Serial.available() > 0) Serial.read(); // Clear buffer

  Serial.print("Received frequency: ");
  Serial.print(userFreq);
  Serial.println(" Hz");

  return userFreq;
}

void adjustTuning(int stringIndex, double diffHz) {
  digitalWrite(ledPin, LOW);
  double absDiff = abs(diffHz);
  int stepsToMove = 0;

  if (absDiff < 3.0) {
    stepsToMove = 0;             // Too close or invalid reading — don't move
  } else if (absDiff > 20.0) {
    stepsToMove = 300;           // Coarse adjustment
  } else if (absDiff > 10.0) {
    stepsToMove = 150;           // Medium adjustment
  } else if (absDiff > 5.0) {
    stepsToMove = 50;            // Fine adjustment
  }

  if (stepsToMove == 0) return;

  Stepper* activeStepper = nullptr;
  switch (stringIndex) {
    case 0: activeStepper = &Stepper1; break;
    case 1: activeStepper = &Stepper2; break;
    case 2: activeStepper = &Stepper3; break;
    case 3: activeStepper = &Stepper1; break;
    case 4: activeStepper = &Stepper2; break;
    case 5: activeStepper = &Stepper3; break;
    default: return;
  }

  bool tighten = (diffHz < -0.5);
  bool loosen  = (diffHz > 0.5);
  if (!tighten && !loosen) return;

  int stepDir = 0;
  if (tighten) {
    stepDir = 1;          // Clockwise (tighten)
  } else {
    stepDir = -1;         // Counterclockwise (loosen)
  }

  Serial.print("String: ");
  Serial.print(stringName[stringIndex]);
  Serial.print(" | Diff: ");
  Serial.print(diffHz);
  Serial.print(" | Moving ");
  Serial.print(tighten ? "tighten (CW)" : "loosen (CCW)");
  Serial.print(" by ");
  Serial.print(stepsToMove);
  Serial.println(" steps");

  activeStepper->step(stepDir * stepsToMove);
  digitalWrite(ledPin, HIGH);
}