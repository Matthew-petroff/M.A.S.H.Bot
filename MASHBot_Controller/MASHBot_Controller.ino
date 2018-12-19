#include <Servo.h>

// PARAMETERS - Calibrations
const bool DEBUG_SERIAL = true; // Set DEBUG Mode for Serial Communications
int termDelay = 15; // Stepper Motor Start/Stop Terminals Delay
int transDelay = 1; // Stepper Motor Lowest Transmission Delay
float maxAccel = 5; // Acceleration of rampDelay to Fastest State
// unsigned int lenScale[] = {51, 34}; // Steps per Pixel (X, Y)
unsigned int lenScale[] = {230, 230}; // Steps per Pixel (X, Y)
unsigned int endLoc[] = {0, 192}; // Endstop Locations (X, Y)
bool homeDir[] = {HIGH, HIGH}; // Direction to Endstops (X, Y)
bool posDir[] = {LOW, HIGH}; // Direction to +Axis Movement (X, Y)
byte zBounds[] = {100, 90}; // Z Axis Servo Boundaries (OFF, ON)
byte powBounds[] = {93, 102}; // Power Servo Boundaries (OFF, ON)

// PARAMETERS - Pinouts
byte enPin = 8; // Stepper Enable, OUTPUT
byte motDir[] = {5, 6}; // Stepper Directional Pins, OUTPUT (X, Y)
byte motStep[] = {2, 3}; // Stepper Stepping Pins, OUTPUT (X, Y)
byte endPins[] = {9, 10}; // Axis Endstop Pins, INPUT (X, Y)
byte zPin = 4; // Z Servo Motor, OUTPUT
byte pPin = 7; // Power Servo Motor, OUTPUT
Servo zServ; // Z-axis Servo Object, OUTPUT
Servo powServ; // Power Servo Object, OUTPUT

// PARAMETERS - Global Variables
unsigned int curPos[] = {0, 0}; // Current Axis Pixel Position (X, Y)
byte zPrev = 0; // Previous Z-Axis Movement

// INITIALIZATION - Pinouts
void _INT_Pins()
{
  for (int i = 0; i < 2; i++)
  {
    pinMode(motDir[i], OUTPUT); // Pin Map Stepper Motor Axis Direction
    pinMode(motStep[i], OUTPUT); // Pin Map Stepper Motor Axis Stepping
    pinMode(endPins[i], INPUT); // Pin Map Axis Endstops
    digitalWrite(endPins[i], HIGH); // Pullup Resistor for Endstops
  }
  pinMode(enPin, OUTPUT); // Pin Map Enable Pin
  digitalWrite(enPin, HIGH); // Disable Enable (Re-enabled After Sync)
  zServ.attach(zPin); // Pin Map Z-Axis Servo
  powServ.attach(pPin); // Pin Map Power Servo
}

// INITIALIZATION - Axis Homing
void _INT_Homing()
{
  zServ.write(zBounds[LOW]); // Reset Z Axis Servo to OFF
  powServ.write(powBounds[LOW]); // Reset Power Servo to OFF

  digitalWrite(enPin, LOW); // Enable Stepper Motors
  for (int i = 0; i < 2; i++)
  {
    digitalWrite(motDir[i], homeDir[i]); // Set Axis Direction to Home Direction
    while (digitalRead(endPins[i])) // Home until endstop reached
    {
      digitalWrite(motStep[i], HIGH);
      delayMicroseconds(transDelay);
      digitalWrite(motStep[i], LOW);
      delayMicroseconds(transDelay);
    }
    curPos[i] = endLoc[i]; // Set Current Location to Endstop Location
  }
  digitalWrite(enPin, HIGH); // Disable Stepper Motors
}

// FUNCTIONS - Toggle DS Power
void PowerToggle()
{
  powServ.write(powBounds[HIGH]); // Power Pushed: ON
  delay(250);
  powServ.write(powBounds[LOW]); // Power Released: OFF
  delay(250);
}

// FUNCTIONS - Stepper Motor Delay Ramping
void rampDelay(unsigned int currentPosition, unsigned int maxPosition)
{
  unsigned int modDelay;
  unsigned int scalarDelay;
  if (currentPosition < (maxPosition / 2))
  {
    scalarDelay = (int)(maxAccel * (float)(currentPosition));
  } else
  {
    scalarDelay = (int)(maxAccel * (float)(maxPosition - currentPosition));
  }
  
  if ((termDelay - transDelay) > scalarDelay)
  {
    modDelay = termDelay - scalarDelay;
  } else
  {
    modDelay = transDelay;
  }
  delayMicroseconds(modDelay);
}

// FUNCTIONS - Stepper Movement Controller
void stepperMovement(unsigned int xDesPos, unsigned int yDesPos)
{
   unsigned int desPos[] = { xDesPos, yDesPos };
   unsigned int quePos[] = { 0, 0 };
   unsigned int small, large;
   int delta = 0;

   for (int i = 0; i < 2; i++)
   {
      if (desPos[i] > curPos[i])
      {
         digitalWrite(motPins[i][0], actDir[i]);
         quePos[i] = scalarLength[i] * (desPos[i] - curPos[i]);
      }
      else
      {
         digitalWrite(motPins[i][0], !actDir[i]);
         quePos[i] = scalarLength[i] * (curPos[i] - desPos[i]);
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

   delta = (2 * quePos[small]) - quePos[large];

   for (int i = 0; i < quePos[large]; i++)
   {
      if (delta > 0)
      {
         digitalWrite(motPins[small][1], HIGH);
         delta -= 2 * quePos[large];
      }

      delta += 2 * quePos[small];
      
      digitalWrite(motPins[large][1], HIGH);

      delayMicroseconds(stepDelay);
      digitalWrite(motPins[0][1], LOW);
      digitalWrite(motPins[1][1], LOW);
      delayMicroseconds(stepDelay);
   }
}

// FUNCTIONS - Move DS Stylus
void moveCoordinates(byte x, byte y, byte z)
{
  if (z == 1)
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
    zServ.write(zBounds[LOW]); // Z Axis Pushed: ON
    zPrev = z; // Save Z State for Next Loop Iteration
    delay(100);
  }
}

void setup()
{
  Serial.begin(115200); // Initialize Serial Communications
  _INT_Pins(); // Initialize All Pins
  _INT_Homing(); // Home Axis
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
      digitalWrite(enPin, LOW); // Power ON Motors
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
          moveCoordinates(xCoord, yCoord, zState); // Move to Designated Position
          Serial.write(0xfa); // Send completion flag
        }
      }
      else if (infoByte == 0xfd) // Serial System Power Command
      {
        PowerToggle(); // Toggle System Power
        Serial.write(0xfa); // Send completion flag
      }
      else if (infoByte == 0xfe) // Serial Kill Sequence Command
      {
        digitalWrite(enPin, HIGH); // Power Off Motors
        PowerToggle(); // Toggle System Power
        Serial.write(0xfa); // Send completion flag

        startFlag = false; // Terminate Serial Handshake
      }
    }
  }
}
