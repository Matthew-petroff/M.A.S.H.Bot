#include <Servo.h>

// PARAMETERS - Calibrations
const bool DEBUG_SERIAL = true; // Set DEBUG Mode for Serial Communications
unsigned int lenScale[] = {191, 191}; // Steps per Pixel (X, Y) ***WARNING: DO NOT EXCEED 255, change movement to LONG if you need to***
unsigned int endLoc[] = {255, 192}; // Endstop Locations (X, Y)
unsigned int endOffset[] = {2, 2};  // Endstop Offset
bool homeDir[] = {HIGH, HIGH}; // Direction to Endstops (X, Y)
bool posDir[] = {HIGH, HIGH}; // Direction to +Axis Movement (X, Y)
byte startBounds[] = {102, 91}; // Start Button Servo Boundaries (OFF, ON)
byte zBounds[] = {89, 79}; // Z Axis Servo Boundaries (OFF, ON)
byte powBounds[] = {93, 102}; // Power Servo Boundaries (OFF, ON)

#define BASE_STEP_DELAY 25 // base time between steps (microseconds)
#define MIN_STEP_DELAY 9   // minimum step delay
#define RAMP_DIV 7         // reduce step delay by 1 microsecond every 2^n steps

// PARAMETERS - Pinouts

//# PORTD
#define XSTEP 0b00010100
#define XDIR  0b10100000
#define YSTEP 0b00001000
#define YDIR  0b01000000

//# PORTB
#define ENPIN 0b00000001
#define ENABLE (PORTB &= (~ENPIN))
#define DISABLE (PORTB |= ENPIN)
#define XEND  0b00000010
#define YEND  0b00000100

#define SPIN 11 // Z Servo Motor, OUTPUT
#define ZPIN 12 // Z Servo Motor, OUTPUT
#define PPIN 13 // Power Servo Motor, OUTPUT

byte motDir[] = {XDIR, YDIR}; // Stepper Directional Pins, OUTPUT (X, Y)
byte motStep[] = {XSTEP, YSTEP}; // Stepper Stepping Pins, OUTPUT (X, Y)

Servo startServ; // Z-axis Servo Object, OUTPUT
Servo zServ; // Z-axis Servo Object, OUTPUT
Servo powServ; // Power Servo Object, OUTPUT

// PARAMETERS - Global Variables
unsigned int curPos[] = {0, 0}; // Current Axis Pixel Position (X, Y)
byte zPrev = 0; // Previous Z-Axis Movement


// INITIALIZATION - Axis Homing
void _INT_Homing(void) {
  startServ.write(startBounds[LOW]); // Reset Z Axis Servo to OFF
  zServ.write(zBounds[LOW]); // Reset Z Axis Servo to OFF
  powServ.write(powBounds[LOW]); // Reset Power Servo to OFF

  ENABLE;
  PORTD |= (XDIR | YDIR);
  while (PINB & (XEND | YEND)) {
    if (PINB & XEND) PORTD &= ~XSTEP;
    if (PINB & YEND) PORTD &= ~YSTEP;
    delayMicroseconds(BASE_STEP_DELAY);
    PORTD |= (XSTEP | YSTEP);
    delayMicroseconds(BASE_STEP_DELAY);
  }
  curPos[0] = endLoc[0] - endOffset[0];
  curPos[1] = endLoc[1] - endOffset[1];
  DISABLE;
}

// INITIALIZATION - Pinouts
void _INT_Pins()
{
  DDRD |= (XSTEP | YSTEP | XDIR | YDIR); // Output pins for step/direction
  DDRB |= ENPIN;                         // Output pin for enable
  DDRB &= ~(XEND | YEND);                 // Inputs for end stop switches
  PORTB |= (XEND | YEND);                // Inputs are pulled high
  DISABLE;                               // Steppers will be enabled after sync

  startServ.attach(SPIN); // Pin Map Z-Axis Servo
  zServ.attach(ZPIN); // Pin Map Z-Axis Servo
  powServ.attach(PPIN); // Pin Map Power Servo
}

// FUNCTIONS - DEBUG Axis Movement
void _DEBUG_Movement()
{
  ENABLE;
  delay(5000);
  stepperMovement(255, 192);
  delay(5000);
  stepperMovement(0, 0);
  delay(100);
  DISABLE;
}

// FUNCTIONS - DEBUG Axis Movement
//void _DEBUG_Movement()
//{
//  ENABLE;
//  delay(5000);
//  stepperMovement(255, 192);
//  delay(5000);
//  stepperMovement(0, 0);
//  delay(100);
//  DISABLE;
//}

// FUNCTIONS - Toggle DS Power
void PowerToggle()
{
  powServ.write(powBounds[HIGH]); // Power Pushed: ON
  delay(500);
  powServ.write(powBounds[LOW]); // Power Released: OFF
  delay(100);
}

// FUNCTIONS - Stepper Movement Controller
void stepperMovement(unsigned int xDesPos, unsigned int yDesPos)
{
  unsigned int desPos[] = { xDesPos, yDesPos };
  unsigned int quePos[] = { 0, 0 };
  unsigned int small, large;
  int delta = 0;
  byte step_delay = BASE_STEP_DELAY;
  int ramp = (1 << RAMP_DIV) - 1;

  for (int i = 0; i < 2; i++)
  {
    if (desPos[i] > curPos[i]) {
      quePos[i] = lenScale[i] * (desPos[i] - curPos[i]);
      if (posDir[i] == HIGH) {
        PORTD |= motDir[i];
      } else {
        PORTD &= ~(motDir[i]);
      }
    } else {
      quePos[i] = lenScale[i] * (curPos[i] - desPos[i]);
      if (posDir[i] == LOW) {
        PORTD |= motDir[i];
      } else {
        PORTD &= ~(motDir[i]);
      }
    }
    curPos[i] = desPos[i];
  }

  if (quePos[0] < quePos[1])
  {
    small = 0;
    large = 1;
  }
  else
  {
    small = 1;
    large = 0;
  }

  unsigned int qSmall = quePos[small] / lenScale[small]; // max would be 192
  unsigned int qLarge = quePos[large] / lenScale[large]; // max would be 255

  delta = (2 * qSmall) - qLarge;
  for (unsigned int i = 0, j = quePos[large]; j > 0; i++, j--)
  {
    if (delta > 0)
    {
      PORTD |= motStep[small];
      delta -= 2 * qLarge;
    }

    delta += 2 * qSmall;

    PORTD |= motStep[large];
    delayMicroseconds(step_delay);
    PORTD &= ~(XSTEP | YSTEP);
    delayMicroseconds(step_delay);
    if (i < j) {
      if ((step_delay > MIN_STEP_DELAY) && ((i & ramp) == ramp)) step_delay--;
    } else if (j < ((1 << RAMP_DIV) * (BASE_STEP_DELAY - MIN_STEP_DELAY))) {
      if ((step_delay < BASE_STEP_DELAY) && ((j & ramp) == ramp)) step_delay++;
    }
  }
}

// FUNCTIONS - Move DS Stylus
void moveCoordinates(byte x, byte y, byte z)
{
  if (z != 0)
  {
    stepperMovement(x, y); // Move Axis to Specified Location (X, Y)
    if (zPrev != z)
    {
      zServ.write(zBounds[HIGH]); // Z Axis Pushed: ON
      zPrev = z; // Save Z State for Next Loop Iteration
      delay(100);
    }
  } else if (zPrev != z)
  {
    zServ.write(zBounds[LOW]); // Z Axis Pushed: OFF
    zPrev = z; // Save Z State for Next Loop Iteration
    delay(100);
  }
}

// START MENU
void startMenu()
{
  startServ.write(startBounds[HIGH]); // Power Pushed: ON
  delay(500);
  startServ.write(startBounds[LOW]); // Power Released: OFF
  delay(100);
}

void setup()
{
  Serial.begin(115200); // Initialize Serial Communications
  _INT_Pins(); // Initialize All Pins
  _INT_Homing(); // Home Axis
  //  _DEBUG_Movement();
}

void loop()
{
  bool startFlag = false; // Initalize Disabled Handshake Confrimation
  Serial.write(0xff); // Transmit Serial Handshake Initializer
  delay(1000); // Delay to Avoid Serial Spamming

  while (Serial.available() > 0) // Check Serial Buffer for Handshake Response
  {
    if (Serial.read() == 0xff) // Confirm Serial Handshake
    {
      startFlag = true; // Confirmed Handshake

      _INT_Homing(); // Home Axis
      delay(1); // Delay for Stepper Inhertia
      ENABLE;
    }
  }

  while (startFlag) // Confirmed Handshake Communications Loop
  {
    if (Serial.available() > 0) // Check Serial Buffer for Commands
    {
      byte infoByte = Serial.read(); // Read Serial Command Type

      if (infoByte == 0xfc) // Serial Movement Command (X, Y, Z)
      {
        unsigned int manualTimeout = 0;
        bool errorState = false;
        byte xCoord = 0;
        byte yCoord = 0;
        byte zState = 0;

        while ((Serial.available() < 7) && (manualTimeout < 1000)) // Wait for Serial Data, Timeout (1sec)
        {
          delay(1); // Delay to Indicate 1millis Time Passed
          manualTimeout++; // Increment Timeout Counter
        }

        xCoord = Serial.read(); // Read X Coordinate
        byte xCoordCheck = Serial.read(); // Read X Coordinate Parity
        yCoord = Serial.read(); // Read Y Coordinate
        byte yCoordCheck = Serial.read(); // Read Y Coordinate Parity
        zState = Serial.read(); // Read Z Coordinate
        byte zStateCheck = Serial.read(); // Read Z Coordinate Parity
        byte paritycheck = Serial.read(); // Byte End Parity

        if (DEBUG_SERIAL == true) // Return Received Serial Bytes
        {
          Serial.write(infoByte);
          Serial.write(xCoord);
          Serial.write(xCoordCheck);
          Serial.write(yCoord);
          Serial.write(yCoordCheck);
          Serial.write(zState);
          Serial.write(zStateCheck);
          Serial.write(paritycheck);
        }

        if (xCoord != xCoordCheck || yCoord != yCoordCheck || zState != zStateCheck || paritycheck != 0xfa) // Parity Check
        {
          while (Serial.available() > 0) // Empty Out Serial Buffer
          {
            Serial.read(); // Dispose of Serial Byte
          }
          Serial.write(0xfc); // Send Error Flag
        }
        else // Parity Check Passed
        {
          moveCoordinates(xCoord, yCoord, constrain(zState, 0, 1)); // Move to Designated Position
          Serial.write(0xfa); // Send completion flag
          delay(10);
        }
      }
      else if (infoByte == 0xfd) // Serial System Power Command
      {
        PowerToggle(); // Toggle System Power
        Serial.write(0xfa); // Send completion flag
      }
      else if (infoByte == 0xfe) // Serial Kill Sequence Command
      {
        delay(100);
        DISABLE; // Power Off Motors
        delay(100);
        PowerToggle(); // Toggle System Power
        Serial.write(0xfa); // Send completion flag

        startFlag = false; // Terminate Serial Handshake
      }
      else if (infoByte == 0xde)
      {
        Serial.write(0xde); // Send completion flag
        startMenu(); // Toggle System Power
        Serial.write(0xfa); // Send completion flag
      }
    }
  }
}
