/*
  ARC Inductions 2026 - Electrical State Machine
  MASTER ARDUINO (Sensors + Actuators)
*/

#include <Wire.h>
#include <Servo.h>

// Pins
const int LDR_PIN    = A0;
const int GAS_PIN    = A1;
const int TEMP_PIN   = A2;
const int SERVO_PIN  = 9;
const int BUZZER_PIN = 8;

const uint8_t SLAVE_ADDR = 0x08;

// Thresholds
const int   GAS_HIGH_THRESHOLD  = 180;
const int   GAS_LOW_THRESHOLD   = 130;
const float TEMP_EMERGENCY_C    = 45.0;
const int   LIGHT_DROP_DELTA    = 200;
const int   LIGHT_DARK_LEVEL    = 150;
const int   LIGHT_RESTORE_LEVEL = 300;

// IR Commands
#define CMD_NONE     0
#define CMD_ACTIVATE 1
#define CMD_TOGGLE   2
#define CMD_RESET    3

// System States
#define STATE_STANDBY     0
#define STATE_ACTIVE      1
#define STATE_GAS_ALERT   2
#define STATE_BLACKOUT    3
#define STATE_TEMP_EMERG  4
#define STATE_MULTI_FAULT 5

uint8_t currentState = STATE_STANDBY;

bool systemActivated    = false;
bool gasAlertLatched    = false;
bool blackoutLatched    = false;
bool tempEmergencyLatch = false;
bool showGasOnDisplay   = false;

int lastLightReading = 1023;

Servo ventServo;

// Explicit Prototypes
uint8_t requestCommandFromSlave();
void sendDataToSlave(uint8_t state, int lightPct, int gasPct, int tempC, bool toggleFlag);

void setup() {
  Serial.begin(9600);
  Wire.begin();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  ventServo.attach(SERVO_PIN);
  ventServo.write(0);

  lastLightReading = analogRead(LDR_PIN);
}

void loop() {
  // Read Sensors
  int lightRaw = analogRead(LDR_PIN);
  int gasRaw   = analogRead(GAS_PIN);
  int tempRaw  = analogRead(TEMP_PIN);

  float voltage = tempRaw * (5.0 / 1023.0);
  float tempC   = (voltage - 0.5) * 100.0;

  // Pull Latest IR Command
  uint8_t cmd = requestCommandFromSlave();
  if (cmd == CMD_ACTIVATE) systemActivated = true;
  if (cmd == CMD_TOGGLE)   showGasOnDisplay = !showGasOnDisplay;
  if (cmd == CMD_RESET)    tempEmergencyLatch = false;

  // Evaluate Fault Flags
  if (!gasAlertLatched && gasRaw > GAS_HIGH_THRESHOLD) gasAlertLatched = true;
  if (gasAlertLatched && gasRaw < GAS_LOW_THRESHOLD)   gasAlertLatched = false;

  int delta = lastLightReading - lightRaw;
  if (!blackoutLatched && delta > LIGHT_DROP_DELTA && lightRaw < LIGHT_DARK_LEVEL) {
    blackoutLatched = true;
  }
  if (blackoutLatched && lightRaw > LIGHT_RESTORE_LEVEL) {
    blackoutLatched = false;
  }
  lastLightReading = lightRaw;

  if (!tempEmergencyLatch && tempC > TEMP_EMERGENCY_C) tempEmergencyLatch = true;

  // Resolve State Priorities
  if (tempEmergencyLatch) {
    currentState = STATE_TEMP_EMERG;
  } else if (gasAlertLatched && blackoutLatched) {
    currentState = STATE_MULTI_FAULT;
  } else if (gasAlertLatched) {
    currentState = STATE_GAS_ALERT;
  } else if (blackoutLatched) {
    currentState = STATE_BLACKOUT;
  } else if (systemActivated) {
    currentState = STATE_ACTIVE;
  } else {
    currentState = STATE_STANDBY;
  }

  // Drive Actuators
  ventServo.write(currentState == STATE_TEMP_EMERG ? 180 : 0);
  digitalWrite(BUZZER_PIN, currentState == STATE_MULTI_FAULT ? HIGH : LOW);

  // Map Telemetry Values
  int gasPurityPct = constrain(map(gasRaw, 0, 400, 100, 0), 0, 100);
  int lightPct     = constrain(map(lightRaw, 0, 1023, 0, 100), 0, 100);

  // Send Data to Slave
  sendDataToSlave(currentState, lightPct, gasPurityPct, (int)tempC, showGasOnDisplay);

  delay(150);
}

// Request IR Command
uint8_t requestCommandFromSlave() {
  Wire.requestFrom(SLAVE_ADDR, (uint8_t)1);
  if (Wire.available()) {
    return Wire.read();
  }
  return CMD_NONE;
}

// Send Telemetry
void sendDataToSlave(uint8_t state, int lightPct, int gasPct, int tempC, bool toggleFlag) {
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(state);
  Wire.write((uint8_t)lightPct);
  Wire.write((uint8_t)gasPct);
  Wire.write((uint8_t)constrain(tempC, 0, 255));
  Wire.write((uint8_t)toggleFlag);
  Wire.endTransmission();
}