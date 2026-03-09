// EmbeMetre A5 - Automatic Acoustic Guitar Tuner
// Final Code: Without Serial

#include "arduinoFFT.h"     // FFT Library
#include <Stepper.h>        // Stepper Library
#include <LCD_I2C.h>        // LCD Library

// MICROPHONE SENSOR Initializations
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
bool systemOn = false;      // System ON/OFF
bool justTurnedOn = false;   
bool justTurnedOff = false;

int modeMenu = 0;     // 0 - Auto(Batch1), 1 - Auto(Batch2), 2 - Manual(Batch1), 3 - Manual(Batch2)
int selectedString = 0;     // String 0-5 = LowE-HighE
double targetFrequency[6] = {82.41, 110.00, 146.83, 196.00, 246.94, 329.63};
const char* stringName[6] = {"LOW E", "A", "D", "G", "B", "HIGH E"};

enum Phase { phaseDetecting, phaseResult };     // Program Phase Control
Phase currentPhase = phaseDetecting;

const unsigned long detectDuration = 3000UL;    // Timing Parameters
const unsigned long displayDuration = 3000UL;   
unsigned long phaseStartMillis = 0UL;           

const int numberSamples = 100;                  // Frequency Sampling Data
double frequencySamples[numberSamples];
double freqTolerance = 1.0;       
double showTolerance = 3.0;       

double lastDetectedFreq = 0.0;                  // Last frequency value detected by FFT
bool isTuned = false;                           // True if current string is within acceptable tuning range
bool tuningAdjustedForCurrentDetection = false; // Ensures the motor only adjusts once per detection cycle

// SETUP --------------------------------------------------------
void setup() {
  analogReference(DEFAULT);       // MICROPHONE SENSOR Setup
  
  Stepper1.setSpeed(stepSpeed);   // STEPPER Setup
  Stepper2.setSpeed(stepSpeed);
  Stepper3.setSpeed(stepSpeed);

  lcd.begin();                    // LCD Setup
  lcd.clear();
  lcd.noBacklight();

  pinMode(touchPin, INPUT);       // TOUCH Setup

  pinMode(button1Pin, INPUT);     // BUTTONS Setup
  pinMode(button2Pin, INPUT);

  pinMode(ledPin, OUTPUT);        // LED Setup
  digitalWrite(ledPin, LOW);      

  systemOn = false;               // Initial States
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

// LOOP --------------------------------------------------------
void loop() {
  currentTouchState = digitalRead(touchPin);
  currentButton1State = digitalRead(button1Pin);
  currentButton2State = digitalRead(button2Pin);

  if (currentTouchState == HIGH && lastTouchState == LOW) {     // touch sensor turns system on or off
    systemOn = !systemOn;
    if (systemOn) {
      justTurnedOn = true;
    } else {
      justTurnedOff = true;
    }
    delay(100);
  }
  lastTouchState = currentTouchState;

  if (justTurnedOn) {                 // System is ON
    digitalWrite(ledPin, LOW);
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ACOUSTIC GUITAR ");
    lcd.setCursor(0, 1);
    lcd.print(" AUTOMATIC TUNER");
    delay(3000); 

    displayMode(modeMenu);    // Default: 0 - Automatic Batch1

    if (modeMenu % 2 == 0){   // set selectedString to batch based on mode
      selectedString = 0;     // Batch1 start = Low E (0)
    } else {
      selectedString = 3;     // Batch2 start = G (3)
    }

    currentPhase = phaseDetecting;
    phaseStartMillis = millis();
    tuningAdjustedForCurrentDetection = false;
    isTuned = false;
    justTurnedOn = false;
  }

  if (justTurnedOff) {                // System OFF
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

  if (!systemOn) {                            // Stops operations on all components
    digitalWrite(ledPin, LOW);
    lastButton1State = currentButton1State;
    lastButton2State = currentButton2State;
    return;
  }

  bool button1Pressed = (currentButton1State == HIGH && lastButton1State == LOW);
  bool button2Pressed = (currentButton2State == HIGH && lastButton2State == LOW);

  if (currentPhase == phaseResult) {        // Reading Result Phase
    if (button1Pressed) {
      digitalWrite(ledPin, LOW);            // LED off after button1 pressing
      
      modeMenu++;                           // Set new mode
      if (modeMenu > 3) modeMenu = 0;
      if (modeMenu % 2 == 0) {              // Set selectedString according to batch
        selectedString = 0;     
      } else {
        selectedString = 3;     
      }

      displayMode(modeMenu);

      lcd.clear();
      currentPhase = phaseDetecting;
      phaseStartMillis = millis();
      tuningAdjustedForCurrentDetection = false;
      isTuned = false;
      delay(200);
    }

    if (button2Pressed) {
      if (modeMenu == 0) {           // Auto Batch1 -> reset to Low E
        selectedString = 0;
      } else if (modeMenu == 1) {    // Auto Batch2 -> reset to G
        selectedString = 3;
      } else if (modeMenu == 2) {    // Manual Batch1 -> next string
        selectedString++;
        if (selectedString > 2) selectedString = 0;
      } else if (modeMenu == 3) {    // Manual Batch2 -> next string
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

  if (currentPhase == phaseDetecting) {     // DETECTING PHASE - State that does NOT allow user input
    digitalWrite(ledPin, LOW);
    displayStatusLine1(selectedString);     // Show string and "Detecting now..."
    lcd.setCursor(0, 1);
    lcd.print("Detecting now...   ");

    lastDetectedFreq = getOfficialFrequency(selectedString);                          // Gets the detection
    double diff = lastDetectedFreq - targetFrequency[selectedString];   // Compare with target frequency
    isTuned = (abs(diff) < showTolerance);                              // Determines if string is tuned

    tuningAdjustedForCurrentDetection = false;     // Flag for occurrence of turning of stepper

    displayStatusLine2(selectedString, lastDetectedFreq);     // Display the status of the detection

    currentPhase = phaseResult;
    phaseStartMillis = millis();
  }
  else if (currentPhase == phaseResult) {   // RESULT PHASE - State that does allow user input
    digitalWrite(ledPin, HIGH);

    if (!isTuned && !tuningAdjustedForCurrentDetection) {     // Adjusts stepper motor since not in tune
      if (lastDetectedFreq > 0.0 && lastDetectedFreq != 62.5) {    // Prevents reacting to idle / unstable sound
        double diff = lastDetectedFreq - targetFrequency[selectedString];
        adjustTuning(selectedString, diff);
      }
      tuningAdjustedForCurrentDetection = true;
    }

    if ((modeMenu == 0 || modeMenu == 1) && isTuned) {      // Automatic Mode (0 or 1) - Continuous Process
      if (millis() - phaseStartMillis >= displayDuration) {
        if (modeMenu == 0) {                                // Moves to next string of the batch if tuned
          selectedString++;
          if (selectedString > 2) selectedString = 0;       // loop Batch1
        } else {
          selectedString++;
          if (selectedString > 5) selectedString = 3;       // loop Batch2
        }

        currentPhase = phaseDetecting;
        phaseStartMillis = millis();
        tuningAdjustedForCurrentDetection = false;
        isTuned = false;
        lcd.clear();
      }
    } else {                                                // Manual Mode (2 or 3) - User-Controlled Process
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
void displayMode(int mode){               // Displays the mode currently in
  lcd.clear();
  lcd.setCursor(0, 0);
  switch(mode){
    case 0:                               // Automatic - Batch 1 
      lcd.print(" AUTOMATIC MODE ");
      lcd.setCursor(0, 1);
      lcd.print(" Low E - A - D  ");
      break;
    case 1:                               // Automatic - Batch 2
      lcd.print(" AUTOMATIC MODE ");
      lcd.setCursor(0, 1);
      lcd.print(" G - B - High E ");
      break;
    case 2:                               // Manual - Batch 1
      lcd.print("  MANUAL MODE   ");
      lcd.setCursor(0, 1);
      lcd.print(" Low E - A - D  ");
      break;
    case 3:                               // Manual - Batch 2
      lcd.print("  MANUAL MODE   ");
      lcd.setCursor(0, 1);
      lcd.print(" G - B - High E ");
      break;
  }
  delay(1500);
}

void displayStatusLine1(int stringIndex){ // Displays the string in LCD line 1
  lcd.setCursor(0, 0);
  lcd.print("String: ");
  lcd.print(stringName[stringIndex]);
  lcd.print("       ");
}

void displayStatusLine2(int stringIndex, double frequency){     // Displays the string in LCD line 2
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

double getFrequencySamples(int stringIndex) {          // Detecting FFT samples
  unsigned long samplingPeriod = round(1000000.0 / samplingFrequency);
  unsigned long microsPrevious = micros();
  
  for (uint16_t i = 0; i < samples; i++) {                      // Data Collection
    while (micros() - microsPrevious < samplingPeriod) { }
    microsPrevious += samplingPeriod;
    vReal[i] = analogRead(micPin);
    vImag[i] = 0.0;
  }

  double mean = 0;                                              // DC offset bias removal (centering the signal)
  for (uint16_t i = 0; i < samples; i++) mean += vReal[i];
  mean /= samples;
  for (uint16_t i = 0; i < samples; i++) vReal[i] -= mean;

  FFT.windowing(FFTWindow::Hann, FFTDirection::Forward);        // FFT processing
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();

  double peak = 0;                                              // Finding dominant or peak frequency
  uint16_t index = 0;
  for (uint16_t i = 1; i < samples / 2; i++) {
    double weighted = vReal[i] * (1.0 - (double)i / (samples / 2));
    if (weighted > peak) {
      peak = weighted;
      index = i;
    }
  }

  double frequency = index * (samplingFrequency / samples);     // Frequency calculation
  double targetFreq = 0;
  switch (selectedString) {                                     // Harmonic Correction
    case 0: targetFreq = 82.41; break;   // Low E
    case 1: targetFreq = 110.00; break;  // A
    case 2: targetFreq = 146.83; break;  // D
    case 3: targetFreq = 196.00; break;  // G
    case 4: targetFreq = 246.94; break;  // B
    case 5: targetFreq = 329.63; break;  // High E
  }

  if (frequency > targetFreq * 1.8 && frequency < targetFreq * 2.2) {
    frequency /= 2.0;   // likely harmonic (e.g., 164 instead of 82)
  } 
  else if (frequency > targetFreq * 0.45 && frequency < targetFreq * 0.55) {
    frequency *= 2.0;   // likely subharmonic (e.g., 125 instead of 250)
  }

  return frequency;
}

double getOfficialFrequency(int stringIndex) {            // Get the most frequent frequency detected for the 3-second time
  unsigned long startTime = millis();
  int sampleIndex = 0;
  int validCount = 0;

  while (millis() - startTime < detectDuration && sampleIndex < numberSamples) {
    double freq = getFrequencySamples(stringIndex);       // Stores detected FFT frequencies in array

    if (freq <= 20.0 || freq > 1000.0) continue;      // Out of guitar range
    // if ((freq >= 60.0 && freq <= 63.0) || isnan(freq)) continue;  // ignore idle noise band
    if (freq < 40.0 && stringIndex > 0) continue;     // Avoid low noise on higher strings

    frequencySamples[sampleIndex++] = freq;
    validCount++;
    delay(5);
  }

  if (validCount == 0) {
    return 0.0; // No valid reading detected
  }

  double mostFrequent = frequencySamples[0];
  int maxCount = 0;

  for (int i = 0; i < validCount; i++) {              // Find the most occuring frequency for 3 seconds
    int count = 0;
    for (int j = 0; j < validCount; j++) {
      if (abs(frequencySamples[i] - frequencySamples[j]) < freqTolerance)
        count++;
    }
    if (count > maxCount) {
      maxCount = count;
      mostFrequent = frequencySamples[i];
    }
  }

  return mostFrequent;
}

void adjustTuning(int stringIndex, double diffHz) {       // Adjust the tuning pegs via stepper motors
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

  if (stepsToMove == 0) return;     // Does not proceed to next processes if 0

  Stepper* activeStepper = nullptr;       // Map string index to correct stepper pointer:
  switch (stringIndex) {
    case 0: activeStepper = &Stepper1; break;     // Batch 1: LOW E
    case 1: activeStepper = &Stepper2; break;     // Batch 1: A
    case 2: activeStepper = &Stepper3; break;     // Batch 1: D
    case 3: activeStepper = &Stepper1; break;     // Batch 2: G
    case 4: activeStepper = &Stepper2; break;     // Batch 2: B
    case 5: activeStepper = &Stepper3; break;     // Batch 2: HIGH E
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

  activeStepper->step(stepDir * stepsToMove);     // Perform the step
  digitalWrite(ledPin, HIGH);
}