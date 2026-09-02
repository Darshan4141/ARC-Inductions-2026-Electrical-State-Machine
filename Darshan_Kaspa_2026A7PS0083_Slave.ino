/*
  ARC Inductions 2026 - Electrical State Machine
  SLAVE ARDUINO (LCD Display + IR Remote)
*/

#include <Wire.h>
#include <LiquidCrystal.h>
#include <IRremote.hpp>

// Pins
const int IR_RECV_PIN = 7;

// Parallel LCD Interface: (RS, E, D4, D5, D6, D7)
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// I2C Address
const uint8_t MY_ADDRESS = 0x08;

// Mapped IR Remote Codes
#define IR_BTN_ACTIVATE 0x10  // 1 button to Activate
#define IR_BTN_TOGGLE   0x11  // 2 button to Toggle display
#define IR_BTN_RESET    0x12  // 3 button to Reset temp emergency

#define CMD_NONE     0
#define CMD_ACTIVATE 1
#define CMD_TOGGLE   2
#define CMD_RESET    3

#define STATE_STANDBY     0
#define STATE_ACTIVE      1
#define STATE_GAS_ALERT   2
#define STATE_BLACKOUT    3
#define STATE_TEMP_EMERG  4
#define STATE_MULTI_FAULT 5

volatile uint8_t pendingCommand = CMD_NONE;

volatile uint8_t rxState = STATE_STANDBY, rxLight = 0, rxGas = 0, rxTemp = 0, rxToggle = 0;
volatile bool newData = false;

// Explicit prototypes
void updateDisplay(uint8_t state, uint8_t light, uint8_t gas, uint8_t temp, uint8_t toggle);
void onReceiveFromMaster(int numBytes);
void onRequestFromMaster();

void setup() {
  Serial.begin(9600);
  
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("AWAITING RITUAL");

  IrReceiver.begin(IR_RECV_PIN, DISABLE_LED_FEEDBACK);

  Wire.begin(MY_ADDRESS);
  Wire.onReceive(onReceiveFromMaster);
  Wire.onRequest(onRequestFromMaster);
}

void loop() {
  // Read IR Remote
  if (IrReceiver.decode()) {
    uint16_t command = IrReceiver.decodedIRData.command;
    
    if (command == IR_BTN_ACTIVATE)      pendingCommand = CMD_ACTIVATE;
    else if (command == IR_BTN_TOGGLE)   pendingCommand = CMD_TOGGLE;
    else if (command == IR_BTN_RESET)    pendingCommand = CMD_RESET;
    
    IrReceiver.resume();
  }

  // Refresh Display
  if (newData) {
    updateDisplay(rxState, rxLight, rxGas, rxTemp, rxToggle);
    newData = false;
  }
}

void updateDisplay(uint8_t state, uint8_t light, uint8_t gas, uint8_t temp, uint8_t toggle) {
  lcd.clear();
  lcd.setCursor(0, 0);

  switch (state) {
    case STATE_STANDBY:
      lcd.print("AWAITING RITUAL");
      break;

    case STATE_ACTIVE:
      if (toggle) {
        lcd.print("AIR PURITY:");
        lcd.setCursor(0, 1);
        lcd.print(gas); lcd.print(" %");
      } else {
        lcd.print("LIGHT LEVEL:");
        lcd.setCursor(0, 1);
        lcd.print(light); lcd.print(" %");
      }
      break;

    case STATE_GAS_ALERT:
      lcd.print("TOXIC PURGE");
      break;

    case STATE_BLACKOUT:
      lcd.print("NOCTIS PROTOCOL");
      break;

    case STATE_TEMP_EMERG:
      lcd.print("COOKED");
      lcd.setCursor(0, 1);
      lcd.print(temp); lcd.print((char)223); lcd.print("C");
      break;

    case STATE_MULTI_FAULT:
      lcd.print("MULTIPLE");
      lcd.setCursor(0, 1);
      lcd.print("PROBLEMS DETECTD");
      break;
  }
}

void onReceiveFromMaster(int numBytes) {
  if (numBytes < 5) return;
  rxState  = Wire.read();
  rxLight  = Wire.read();
  rxGas    = Wire.read();
  rxTemp   = Wire.read();
  rxToggle = Wire.read();
  newData = true;
}

void onRequestFromMaster() {
  Wire.write(pendingCommand);
  pendingCommand = CMD_NONE;
}