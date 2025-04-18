/*********************************************************************
                      -- NOMO v.1 --            
              Interactive Media Capstone 2025  

Authors          : Liza Datsenko and Sri Pranav Srivatsavai
Capstone Advisor : Prof. Nimrah Syed
Last updated     : 17 April 2025
Description      : Nomo is an interactive desk companion robot built to 
                   encourage healthier work habits through computer 
                   vision, gesture recognition, and playful nudges.

Special thanks to Prof. Michael Shiloh and Prof. Aya Riad
for their valuable contributions.

* NOTES
- The Work mode is referred to as the Study mode in some cases.
**********************************************************************/

#include <Wire.h>
#include <Adafruit_MPR121.h>
#include <SPI.h>
#include <MFRC522.h>
#include <FastLED.h>
#include <Servo.h>
#include <Adafruit_VS1053.h>
#include <SD.h>

//////////////////////////////////////////////////////////
// PIN SELECTION - Servo, RFID, Vibration Motor
//////////////////////////////////////////////////////////

#define MPR121_INT 2   // Interrupt pin for MPR121
#define RST_PIN 5      // Reset pin for RFID
#define SS_PIN 53      // Slave select pin for RFID
#define MOTOR_PIN 47    // Pin for vibration motor
#define MOTOR_PIN2 48    // Pin for vibration motor

#define BOTTOM_SERVO_PIN 45 // Pin for Servo 1
#define UP_SERVO_PIN 46 // Pin for Servo 2

//////////////////////////////////////////////////////////
// MUSIC MAKER SHIELD
//////////////////////////////////////////////////////////
#define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
#define SHIELD_CS     7      // VS1053 chip select pin (output)
#define SHIELD_DCS    6      // VS1053 Data/command select pin (output)

#define CARDCS 4     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin

Adafruit_VS1053_FilePlayer musicPlayer = 
  // create breakout-example object!
  // Adafruit_VS1053_FilePlayer(BREAKOUT_RESET, BREAKOUT_CS, BREAKOUT_DCS, DREQ, CARDCS);
  // create shield-example object!
  Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

//////////////////////////////////////////////////////////
// NEOPIXEL
//////////////////////////////////////////////////////////
#define DATA_PIN    49
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
#define NUM_LEDS    16
CRGB leds[NUM_LEDS];

#define LED_PIN 3
#define BRIGHTNESS 155
#define FAST_BLINK_INTERVAL 600  // Fast blink speed
#define SLOW_BLINK_INTERVAL 1000  // Slow blink speed
#define LOADING_INTERVAL 100     // Loading effect speed
#define TRAIL_LENGTH 4           // Number of LEDs that stay on

bool isBlinking = false;
bool ledsOn = false;
unsigned long previousMillis = 0;
int blinkInterval = FAST_BLINK_INTERVAL;
CRGB blinkColor;

bool effectRunning = false;
int effectCycles = 0;
int currentLED = 0;
CRGB currentColor;
CRGB fadeColor;
unsigned long fadeInterval;
int brightness = 0;
int fadeDirection = 1; // 1 for increasing, -1 for decreasing
bool isFading = false;
int fadeBrightness;

bool effectCompleted = false;  // Flag to track when the effect is done
unsigned long effectEndTime = 0;  // Timestamp for when the effect ends
bool fadeRunning = false;  // New flag to track if a fade is active
unsigned long fadeMillis = 0;
bool BACK_TO_STUDY = false;
bool EXTEND_BREAK = false;

const int debounceCount = 10;
int touchCounter = 0;

boolean STUDY_MODE = false;
String BREAK_LOCATION = "";
String CURRENT_LOCATION = "";

unsigned long smileStartMillis = 0; // Variable to store the time when the smile was shown
bool smileActive = false; // Flag to track if the smile is active

//////////////////////////////////////////////////////////
// Initialization of Capacitive Touch Sensor, Servo, RFID
//////////////////////////////////////////////////////////

Adafruit_MPR121 capTouch = Adafruit_MPR121();
MFRC522 rfid(SS_PIN, RST_PIN); // RFID reader

Servo bottomServo;
Servo upServo;
// Global variable to store the UID of the last detected card
byte lastCardUID[4] = {0x00, 0x00, 0x00, 0x00};

// Predefined UIDs for each track
const byte trackUIDs[3][4] = {
    {0x2A, 0xCF, 0xC8, 0x5F},  // YELLOW
    {0x6D, 0xBC, 0xC8, 0x5F},  // PURPLE
    {0x03, 0x3B, 0x20, 0xDA}   // BLUE
};

//////////////////////////////////////////////////////////
// Interrupt
//////////////////////////////////////////////////////////
volatile bool touchFlag = false;

void mpr121Interrupt() {
  touchFlag = true;
}


void setup() {
    // Serial to communicate with the Pi
    Serial.begin(115200);

    // Initialize Capacitive Touch Sensor
    if (!capTouch.begin(0x5A)) {
        //Serial.println("MPR121 not found, check connections!");
        while (1);
    }
    pinMode(MPR121_INT, INPUT_PULLUP);
    capTouch.setThresholds(10,6);

    SPI.begin();
    rfid.PCD_Init();

    pinMode(MOTOR_PIN, OUTPUT);
    digitalWrite(MOTOR_PIN, LOW);
    pinMode(MOTOR_PIN2, OUTPUT);
    digitalWrite(MOTOR_PIN2, LOW);

    // Initialize Music Player
    if (! musicPlayer.begin()) {
      Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
      while (1);
    }
    Serial.println(F("VS1053 found"));

    // printDirectory(SD.open("/"), 0);

    musicPlayer.setVolume(0,0);

    musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int

    // musicPlayer.startPlayingFile("/001.mp3");
    
    if (!SD.begin(CARDCS)) {
      Serial.println(F("SD failed, or not present"));
      while (1);  // don't do anything more
    } 

    // Initialize Neo Pixel Ring
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear();
    FastLED.show();
    startFadeSlow(CRGB::White);
    FastLED.show();

    // Initialize Servos
    bottomServo.attach(BOTTOM_SERVO_PIN); // Attach first servo to pin 45
    upServo.attach(UP_SERVO_PIN); // Attach second servo to pin 46
    
    bottomServo.write(70);
    upServo.write(80);
    delay(500);
  
    detachAllServos();

    // From handleRFID()
    attachInterrupt(digitalPinToInterrupt(MPR121_INT), mpr121Interrupt, FALLING);
}

void loop() {
  handleRFID();
  if (touchFlag) {
    touchFlag = false;
    handleTouchSensor();  // Called in main thread, for intterupt work as expected!
  }

  updateSmile();
  if(!smileActive){
    updateLEDs();     // Handle loading effect
    updateBlinking(); // Run blinking effect
    updateFadeEffect(); // Run fading effect
  }
  
  // Read and interpret incoming data from Pi
  String command = "";
  if (Serial.available() > 0) {
    command = Serial.readStringUntil('\n');  // Read full command until newline
    command.trim();  // Remove extra whitespace or newline characters
  }

  // Evaluate commands and perform:
  if (command == "SHY"){
    lookDown();
    musicPlayer.stopPlaying();
    musicPlayer.startPlayingFile("005.mp3");

  } 
  else if (command == "NEUTRAL"){
    // Neutral position of servos
    lookNeutral();
  } 
  else if (command == "EXTEND_STUDY"){
    // Turn solid blue colour, servo knod
    knod();
    musicPlayer.stopPlaying();
    musicPlayer.startPlayingFile("009.mp3");
    turnOnAllLEDs(CRGB::Blue);
    lookDown();
  } 
  else if (command == "EXTEND_BREAK"){
    // Turn solid whatever break colour, servo knod
    knod();
    musicPlayer.stopPlaying();
    musicPlayer.startPlayingFile("009.mp3");
    EXTEND_BREAK = true;
    BACK_TO_STUDY = false;
    if (BREAK_LOCATION == "YELLOW"){
      // solid yellow
      turnOnAllLEDs(CRGB::Yellow);
    } 
    else if (BREAK_LOCATION == "PURPLE"){
      // solid purple
      turnOnAllLEDs(CRGB::Purple);
    }
  } 
  else if (command == "MOVE_TO_DESK"){
    // LED fast blinking blue
    musicPlayer.stopPlaying();
    musicPlayer.startPlayingFile("011.mp3");
    startBlinkFast(CRGB::Blue);
  } 
  else if (command == "BREAK_YELLOW"){
    // LED fast blinking yellow 
    musicPlayer.stopPlaying();
    musicPlayer.startPlayingFile("011.mp3");
    startBlinkFast(CRGB::Yellow);
    BREAK_LOCATION = "YELLOW";
    lookNeutral();
  } 
  else if (command == "BREAK_PURPLE"){
    // LED fast blinking purple
    musicPlayer.stopPlaying();
    musicPlayer.startPlayingFile("011.mp3");
    startBlinkFast(CRGB::Purple);
    BREAK_LOCATION = "PURPLE";
    lookNeutral();
  } 
  else if (command == "CHECK_LOCATION"){
    Serial.println(CURRENT_LOCATION);
  } 
  else if (command == "SECOND_BREAK_NUDGE"){
    // Play a sound
    musicPlayer.stopPlaying();
    musicPlayer.startPlayingFile("001.mp3");
  } 
  else if (command == "BACK_TO_STUDY"){
    // LED fast blinking blue and sound
    musicPlayer.stopPlaying();
    musicPlayer.startPlayingFile("011.mp3");
    BACK_TO_STUDY = true;
    EXTEND_BREAK = false;
    startBlinkFast(CRGB::Blue);
  } 
  else if (command == "STUDY_RESTART"){
    // LED solid blue and sound
    musicPlayer.stopPlaying();
    musicPlayer.startPlayingFile("013.mp3");
    stopAllEffects();
    turnOnAllLEDs(CRGB::Blue);
    BACK_TO_STUDY = false;
    BREAK_LOCATION = "";
    lookDown();
  }
  else if (command == "SECOND_STUDY_NUDGE"){
    // Play a sound
    musicPlayer.stopPlaying();
    musicPlayer.startPlayingFile("001.mp3");
  } 
  else if (command == "STANDBY"){
    // Study mode - OFF; slow blinking of the respective location, knod
    // If it is not at any location - slow blinking white
    musicPlayer.stopPlaying();
    musicPlayer.startPlayingFile("019.mp3");
    stopAllEffects();
    STUDY_MODE = false;
    // isBlinking = false;
    BREAK_LOCATION = "";
    BACK_TO_STUDY = false;
    startFadeSlow(CRGB::White);
    lookNeutral();
  } 
  else if (command == "LOOK_LEFT"){
    lookLeft();
  } 
  else if (command == "LOOK_RIGHT"){
    lookRight();
  }
  else if (command == "DETECTED"){
    // Play sound, Wave for a Hi
    musicPlayer.stopPlaying();
    musicPlayer.startPlayingFile("017.mp3");
    turnOffAllLEDs();
    faceWave();
    showSmile(CRGB::White);
  } 
  else if (command == "WAVE"){
    // Play sound, Wave for a Hi
    musicPlayer.stopPlaying();
    musicPlayer.startPlayingFile("003.mp3");
    turnOffAllLEDs();
    faceWave();
    showSmile(CRGB::White);
  } 
  else if (command == "STUDY"){
    // Solid blue, Servo knod, STUDY MODE ON
    musicPlayer.stopPlaying();
    musicPlayer.startPlayingFile("013.mp3");
    knod();
    turnOnAllLEDs(CRGB::Blue);
    STUDY_MODE = true;
    lookDown();
  }
}


//////////////////////////////////////////////////////////
// CODE FOR TOUCH SENSOR
//////////////////////////////////////////////////////////

void handleTouchSensor() {
  if (digitalRead(MPR121_INT) == LOW) {  
    uint16_t touched = capTouch.touched();

    if (touched) {
      //Serial.print("Touched electrodes: ");
      for (uint8_t i = 0; i < 12; i++) {
          if (touched & (1 << i)) {
              //Serial.print(i);
              //Serial.print(" ");
          }
      }

      // Turn on vibration motors when touched
      digitalWrite(MOTOR_PIN, HIGH);
      digitalWrite(MOTOR_PIN2, HIGH);
    } 
    else {
      // Turn off vibration motors when not touched
      digitalWrite(MOTOR_PIN, LOW);
      digitalWrite(MOTOR_PIN2, LOW);
    }
  }
}

// void handleTouchSensor() {
//   if (digitalRead(MPR121_INT) == LOW) {
//     uint16_t touched = capTouch.touched();
//     if (touched) {
//       touchCounter++;
//       if (touchCounter >= debounceCount) {
//         digitalWrite(MOTOR_PIN, HIGH);
//         digitalWrite(MOTOR_PIN2, HIGH);
//       }
//     } else {
//       touchCounter = 0;
//       digitalWrite(MOTOR_PIN, LOW);
//       digitalWrite(MOTOR_PIN2, LOW);
//     }
//   }
// }

//////////////////////////////////////////////////////////
// CODE FOR RFID
//////////////////////////////////////////////////////////

void handleRFID() {
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  // Prevent reading invalid UID (all zero)
  bool isValidUID = false;
  for (int i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] != 0x00) {
        isValidUID = true;
        break;
    }
  }
  if (!isValidUID) return;  // Skip processing if UID is invalid

  // detachInterrupt(digitalPinToInterrupt(MPR121_INT));
  FastLED.show(); // New location effect


  if (compareUID(rfid.uid.uidByte, rfid.uid.size, lastCardUID, 4)) {
    return; // Same card detected, ignore
  }

  // Stop all running effects (immediately clears previous fading)
  stopAllEffects();

  if (compareUID(rfid.uid.uidByte, rfid.uid.size, trackUIDs[0], 4)) {
    musicPlayer.stopPlaying();
    musicPlayer.startPlayingFile("007.mp3");
    CURRENT_LOCATION = "YELLOW";
    Serial.println("YELLOW");

    startEffect(CRGB::Yellow, 1);
    effectCompleted = false;
    effectEndTime = millis() + (LOADING_INTERVAL * TRAIL_LENGTH * 2);
  } 
  else if (compareUID(rfid.uid.uidByte, rfid.uid.size, trackUIDs[1], 4)) {
    musicPlayer.stopPlaying();
    musicPlayer.startPlayingFile("007.mp3");
    CURRENT_LOCATION = "PURPLE";
    Serial.println("PURPLE");

    startEffect(CRGB::Purple, 1);
    effectCompleted = false;
    effectEndTime = millis() + (LOADING_INTERVAL * TRAIL_LENGTH * 2);
  } 
  else if (compareUID(rfid.uid.uidByte, rfid.uid.size, trackUIDs[2], 4)) {
    musicPlayer.stopPlaying();
    musicPlayer.startPlayingFile("007.mp3");
    CURRENT_LOCATION = "BLUE";
    Serial.println("BLUE");

    startEffect(CRGB::Blue, 1);
    effectCompleted = false;
    effectEndTime = millis() + (LOADING_INTERVAL * TRAIL_LENGTH * 2);      
  } 
  else {
    CURRENT_LOCATION = "NONE";
  }

  memcpy(lastCardUID, rfid.uid.uidByte, 4);
  // attachInterrupt(digitalPinToInterrupt(MPR121_INT), handleTouchSensor, FALLING); // Moved to setup
}

// Compare two UIDs
bool compareUID(byte* uid, byte size, const byte* compareUID, byte compareSize) {
  if (size != compareSize) return false;
  for (byte i = 0; i < size; i++) {
      if (uid[i] != compareUID[i]) return false;
  }
  return true;
}

// Print UID in HEX format
void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
      //Serial.print(buffer[i] < 0x10 ? " 0" : " ");
      //Serial.print(buffer[i], HEX);
  }
}

//////////////////////////////////////////////////////////
// CODE FOR NEOPIXEL
//////////////////////////////////////////////////////////

void startFadeSlow(CRGB color) {
  fadeColor = color;
  fadeInterval = SLOW_BLINK_INTERVAL / 50; // Adjust for slower fade
  isFading = true;
  fadeBrightness = 0;
  fadeDirection = 1;
  fadeRunning = true;
}

void stopFading() {
  isFading = false;
  turnOffAllLEDs();
  FastLED.show();
}

// **Call this in loop() to update fade animation**
void updateFadeEffect() {
  if (!fadeRunning) return;

  unsigned long currentMillis = millis();
  if (currentMillis - fadeMillis >= fadeInterval) {  // Adjust fading speed here
    fadeMillis = currentMillis;

    // Adjust brightness based on direction
    fadeBrightness += fadeDirection * 5; // Adjust step size for smooth fading
    if (fadeBrightness >= 255) {
        fadeBrightness = 255;
        fadeDirection = -1; // Start fading out
    } 
    else if (fadeBrightness <= 0) {
        fadeBrightness = 0;
        fadeDirection = 1; // Start fading in
    }

    FastLED.clear();
    fill_solid(leds, NUM_LEDS, fadeColor);
    FastLED.setBrightness(fadeBrightness);
    FastLED.show();
  }
}

// Start the loading effect
void startEffect(CRGB color, int times) {
  currentColor = color;
  effectCycles = times * NUM_LEDS;
  currentLED = 0;
  effectRunning = true;
}

// Function to stop all effects (loading and fading)
void stopAllEffects() {
  effectRunning = false;
  fadeRunning = false;  
  effectCompleted = true; 
  turnOffAllLEDs();
  FastLED.show();
}

// Start fast blinking
void startBlinkFast(CRGB color) {
  // turnOffAllLEDs();
  stopAllEffects();
  blinkColor = color;
  blinkInterval = FAST_BLINK_INTERVAL;
  isBlinking = true;
  ledsOn = false;
  previousMillis = millis();
  // fadeRunning = false;
  // isFading = false;
}

// Stop blinking and turn LEDs off
void stopBlinking() {
    isBlinking = false;
    turnOffAllLEDs();
    FastLED.show();
}

// Keep blinking running
void updateBlinking() {
  if (!isBlinking) return;

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= blinkInterval) {
    previousMillis = currentMillis;
    ledsOn = !ledsOn;

    if (ledsOn) {
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = blinkColor;
        }
    } else {
        FastLED.clear();
    }
    FastLED.setBrightness(155);
    FastLED.show();
  }
}

void updateLEDs() {
  if (!effectRunning) {
    if (!effectCompleted && millis() >= effectEndTime) {
        effectCompleted = true;  

        // Start fade effect after the loading completes
        if(!STUDY_MODE){
            startFadeSlow(
              CURRENT_LOCATION == "YELLOW" ? CRGB::Yellow : 
              CURRENT_LOCATION == "PURPLE" ? CRGB::Purple : 
              CURRENT_LOCATION == "BLUE" ? CRGB::Blue :
              CRGB::White // Default case
          );
          return;
        } 

        else if(CURRENT_LOCATION == "YELLOW"){
          if (EXTEND_BREAK) { 
                if (BREAK_LOCATION == "YELLOW") {
                turnOnAllLEDs(CRGB::Yellow);
              } else if (BREAK_LOCATION == "PURPLE") {
                turnOnAllLEDs(CRGB::Purple);
              }
            } else if (BACK_TO_STUDY) {
              startBlinkFast(CRGB::Blue);    
            } else if (BREAK_LOCATION == "YELLOW") {
              turnOnAllLEDs(CRGB::Yellow);
            } else if (BREAK_LOCATION == "PURPLE") {
              startBlinkFast(CRGB::Purple); 
            } else if (BREAK_LOCATION == ""){
              startBlinkFast(CRGB::Blue); 
            }
            return;
        }
        
        else if (CURRENT_LOCATION == "PURPLE"){
            if (EXTEND_BREAK) {
                if (BREAK_LOCATION == "YELLOW") {
                turnOnAllLEDs(CRGB::Yellow);
              } else if (BREAK_LOCATION == "PURPLE") {
                turnOnAllLEDs(CRGB::Purple);
              }
            } else if (BACK_TO_STUDY) {
              startBlinkFast(CRGB::Blue);    
            } else if (BREAK_LOCATION == "YELLOW") {
              startBlinkFast(CRGB::Yellow);
            } else if (BREAK_LOCATION == "PURPLE") {
              turnOnAllLEDs(CRGB::Purple);
            } else if (BREAK_LOCATION == ""){
              startBlinkFast(CRGB::Blue); 
            }
            return;
        }

        else if (CURRENT_LOCATION == "BLUE"){
          if ((BREAK_LOCATION == "PURPLE") && !BACK_TO_STUDY) {
              startBlinkFast(CRGB::Purple);
          } else if ((BREAK_LOCATION == "YELLOW") && !BACK_TO_STUDY) {
              startBlinkFast(CRGB::Yellow);
          } else {
            turnOnAllLEDs(CRGB::Blue);
          }
          return;
        }   
    }
    return;
  }


  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= LOADING_INTERVAL) {
    previousMillis = currentMillis;

    FastLED.clear();
    for (int i = 0; i < TRAIL_LENGTH; i++) {
        int ledIndex = (currentLED - i + NUM_LEDS) % NUM_LEDS;
        leds[ledIndex] = currentColor;
    }
    FastLED.setBrightness(155);
    FastLED.show();

    currentLED = (currentLED + 1) % NUM_LEDS;
    effectCycles--;

    if (effectCycles <= 0) {
        effectRunning = false;
        turnOffAllLEDs();
    }
  }
}


// Turn on all LEDs with a specific color
void turnOnAllLEDs(CRGB color) {
  isBlinking = false;
  effectRunning = false;
  fadeRunning = false;
  isFading = false;
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = color;
  }
  FastLED.setBrightness(30);
  FastLED.show();
}

// Turn off all LEDs
void turnOffAllLEDs() {
  isBlinking = false;
  effectRunning = false;
  fadeRunning = false;
  isFading = false;
  FastLED.clear();
  FastLED.show();
}

// Turn on half of the LEDs to create a smile effect, then turn off after 3 seconds
void showSmile(CRGB color) {
  isBlinking = false;
  effectRunning = false;
  FastLED.clear();

  int halfLEDs = NUM_LEDS / 2;
  int startLED = (NUM_LEDS - halfLEDs) / 2; // Center the smile

  for (int i = 0; i < halfLEDs; i++) {
      leds[(startLED + i) % NUM_LEDS] = color;
  }

  FastLED.setBrightness(30);
  FastLED.show();

  // Start the timer for 3 seconds
  smileStartMillis = millis();
  smileActive = true;
}

void updateSmile() {
  if (smileActive) {
    // Check if 3 seconds have passed
    if (millis() - smileStartMillis >= 3000) {
      turnOffAllLEDs(); // Turn off the LEDs after 3 seconds
      smileActive = false; // Reset the smile state

      // <--- Add this condition
      if (!STUDY_MODE) {
        // If not in study mode, fade to the location color
        startFadeSlow(
          CURRENT_LOCATION == "YELLOW" ? CRGB::Yellow : 
          CURRENT_LOCATION == "PURPLE" ? CRGB::Purple : 
          CURRENT_LOCATION == "BLUE"   ? CRGB::Blue   :
          CRGB::White
        );
      } else {
        // If we are in study mode, restore our "study" color
        if (CURRENT_LOCATION == "BLUE") {
          turnOnAllLEDs(CRGB::Blue);
        } else if (CURRENT_LOCATION == "PURPLE") {
          turnOnAllLEDs(CRGB::Purple);
        } else if (CURRENT_LOCATION == "YELLOW") {
          turnOnAllLEDs(CRGB::Yellow);
        } else {
          // If we truly have no recognized location, pick some default,
          // or do nothing, or do a slow fade, etc.
        }
      }
    }
  }
}

//////////////////////////////////////////////////////////
// CODE FOR SERVOS
//////////////////////////////////////////////////////////
void servoResting() { // NOT USING FOR NOW
  static int step = 0;
  static unsigned long lastMoveTime = 0;  // Tracks time for each step
  static int currentBottomAngle = -1;
  static int currentUpAngle = -1;

  // If less than 300ms has passed since last step, do nothing
  if (millis() - lastMoveTime < 400) return;

  // Update the timer
  lastMoveTime = millis();

  // Advance one step
  switch (step) {
      case 0:
        if (currentBottomAngle != 70) {
            // bottomServo.write(70); // Move bottom servo to position 50
            moveBottomServo(70);
            currentBottomAngle = 70;
        }
        break;
      case 1:
        if (currentUpAngle != 90) {
            // upServo.write(90);     // Move top servo to position 5
            moveUpServo(90);
            currentUpAngle = 90;
        }
        break;
  }

  step++;
  if (step >= 2) step = 0; // Reset step when sequence completes
}

void lookUp() {
  unsigned long currentMillis = millis();
  static unsigned long lastMoveTime = millis(); // Start timer
  int step = 0;

  while (step < 3) { // Loop through all steps of the sequence
    currentMillis = millis();
    // Wait until 400ms has passed for each step
    if (currentMillis - lastMoveTime >= 400) {
      lastMoveTime = currentMillis; // Update the timer

      switch (step) {
        case 0:
          moveBottomServo(70);
          break;
        case 1:
          moveUpServo(90);
          break;
        case 2:
          detachAllServos();
          break;
      }
      step++;
    }
  }
}

void lookDown() {
  unsigned long currentMillis = millis();
  static unsigned long lastMoveTime = millis(); // Start timer
  int step = 0;

  while (step < 3) { // Loop through all steps of the sequence
    currentMillis = millis();
    // Wait until 400ms has passed for each step
    if (currentMillis - lastMoveTime >= 400) {
      lastMoveTime = currentMillis; // Update the timer

      switch (step) {
        case 0:
          moveBottomServo(70);
          break;
        case 1:
          moveUpServo(70);
          break;
        case 2:
          detachAllServos();
          break;
      }
      step++;
    }
  }
}

void lookNeutral() {
  unsigned long currentMillis = millis();
  static unsigned long lastMoveTime = millis(); // Start timer
  int step = 0;

  while (step < 3) { // Loop through all steps of the sequence
    currentMillis = millis();
    // Wait until 400ms has passed for each step
    if (currentMillis - lastMoveTime >= 400) {
      lastMoveTime = currentMillis; // Update the timer

      switch (step) {
        case 0:
          moveBottomServo(70);
          break;
        case 1:
          moveUpServo(80);
          break;
        case 2:
          detachAllServos();
          break;
      }
      step++;
    }
  }
}

void lookLeft() {
  unsigned long currentMillis = millis();
  static unsigned long lastMoveTime = millis(); // Start timer
  int step = 0;

  while (step < 3) { // Loop through all steps of the sequence
    currentMillis = millis();
    // Wait until 400ms has passed for each step
    if (currentMillis - lastMoveTime >= 400) {
      lastMoveTime = currentMillis; // Update the timer

      switch (step) {
        case 0:
          moveUpServo(80);
          break;
        case 1:
          moveBottomServo(95);
          break;
        case 2:
          detachAllServos();
          break;
      }
      step++;
    }
  }
}

void lookRight() {
  unsigned long currentMillis = millis();
  static unsigned long lastMoveTime = millis(); // Start timer
  int step = 0;

  while (step < 3) { // Loop through all steps of the sequence
    currentMillis = millis();
    // Wait until 400ms has passed for each step
    if (currentMillis - lastMoveTime >= 400) {
      lastMoveTime = currentMillis; // Update the timer

      switch (step) {
        case 0:
          moveUpServo(80);
          break;
        case 1:
          moveBottomServo(55);
          break;
        case 2:
          detachAllServos();
          break;
      }
      step++;
    }
  }
}

void faceWave() {
  unsigned long currentMillis = millis();
  static unsigned long lastMoveTime = millis(); // Start timer
  int step = 0; // Initialize the step counter

  while (step < 6) { // Loop through all steps of the sequence
    currentMillis = millis();
    // Wait until 300ms has passed for each step
    if (currentMillis - lastMoveTime >= 300) {
      lastMoveTime = currentMillis; // Update the timer

      switch (step) {
        case 0:
          moveUpServo(90); //Default Look Up
          break;

        case 1:
          // bottomServo.write(55);      // Move servo to position 35
          moveBottomServo(55);
          break;

        case 2:
          // bottomServo.write(85);      // Move servo to position 65
          moveBottomServo(85);
          break;

        case 3:
          // bottomServo.write(70);      // Move servo back to neutral
          moveBottomServo(70);
          break;
        
        case 4:
          moveUpServo(80); //Make Nomo look down again
          break;
        
        case 5:
          detachAllServos();
          break;
      }
      step++; // Move to the next step
    }
  }
}

void knod() {
  unsigned long currentMillis = millis();
  static unsigned long lastMoveTime = millis(); // Start timer
  int step = 0; // Initialize the step counter

  while (step < 3) { // Loop through all steps of the sequence
    currentMillis = millis();
    // Wait until 300ms has passed for each step
    if (currentMillis - lastMoveTime >= 400) {
      lastMoveTime = currentMillis; // Update the timer

      switch (step) {
      // case 0: upServo.write(80); break;
      // case 1: upServo.write(90); break;
      case 0: moveUpServo(70); break;
      case 1: moveUpServo(80); break;
      case 2:
        detachAllServos();
        break;
    }
    step++;
    }
  }
}

// Helpers for the attach - detach logic 
/* This is a step to prevent the constant twitching of the servo motor - which happens 
due to the NeoPixel LED often disabling the interrupt. Servo needs the interrupt enabled
to maintain the angle and prevent twitching */
void moveBottomServo(int angle) {
  if (!bottomServo.attached()) {
    bottomServo.attach(BOTTOM_SERVO_PIN);  // Replace with your actual pin
  }
  bottomServo.write(angle);
}

void moveUpServo(int angle) {
  if (!upServo.attached()) {
    upServo.attach(UP_SERVO_PIN);  // Replace with your actual pin
  }
  upServo.write(angle);
}

void detachAllServos() {
  if (bottomServo.attached()) {
    bottomServo.detach();
  }
  if (upServo.attached()) {
    upServo.detach();
  }
}
