#include <Wire.h>
#include <IRremote.hpp>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int QUEUE_SIZE = 8;
int irQueue[QUEUE_SIZE];
int queueHead = 0, queueTail = 0, queueCount = 0;
volatile bool newStateReceived = false;
volatile byte receivedState;
volatile int receivedValue;
volatile bool showGas = false;

void setup() {
  Wire.begin(8); // slave address = 8
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  IrReceiver.begin(7);
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
}

void updateLCD(byte state, int value, bool isGas) {
  static byte lastState = 255; // sentinel, forces first draw
  static bool lastIsGas = !false; // arbitrary, forces first draw

  if (state != lastState || (state == 1 && isGas != lastIsGas)) {
    lcd.clear(); // only clear when the LABEL actually needs to change
    lcd.setCursor(0, 0);
    switch (state) {
      case 0: lcd.print("AWAITING RITUAL"); break;
      case 1: lcd.print(isGas ? "Gas: " : "Light: "); break;
      case 2: lcd.print("TOXIC PURGE"); break;
      case 3: lcd.print("NOCTIS PROTOCOL"); break;
      case 4: lcd.print("COOKED"); break;
      case 5:
        lcd.print("MULTIPLE");
        lcd.setCursor(0, 1);
        lcd.print("PROBLEMS");
        break;
    }
    lastState = state;
    lastIsGas = isGas;
  }

  if (state == 1) { // only the number needs refreshing every cycle
    lcd.setCursor(7, 0); // right after "Light: " or "Gas: " ends
    lcd.print("    ");   // blank out old number first (handles shrinking digit count, e.g. 1000 -> 54)
    lcd.setCursor(7, 0);
    lcd.print(value);
  }
}

void loop() {
  if (IrReceiver.decode()) {
      unsigned long raw = IrReceiver.decodedIRData.decodedRawData;
      byte cmd = 0;
      if (raw == 0xFF00BF00) cmd = 1;
      else if (raw == 0xF30CBF00) showGas = !showGas;
      else if (raw == 0xEF10BF00) cmd = 3;
      if (cmd != 0) enqueueIR(cmd);
      Serial.println(raw);
      IrReceiver.resume();
  }

  if (newStateReceived) {
      updateLCD(receivedState, receivedValue, showGas);
      newStateReceived = false;
  }

  Serial.println(receivedState);
	
  delay(100);
}

void enqueueIR(byte code) {
  if (queueCount < QUEUE_SIZE) {
    irQueue[queueTail] = code;
    queueTail = (queueTail + 1) % QUEUE_SIZE;
    queueCount++;
  }
}

byte dequeueIR() {
  if (queueCount == 0) return 0;
  byte code = irQueue[queueHead];
  queueHead = (queueHead + 1) % QUEUE_SIZE;
  queueCount--;
  return code;
}

void receiveEvent(int howMany) {
    if (howMany >= 5) {
        receivedState = Wire.read();
        byte hi = Wire.read();
        byte lo = Wire.read(); 
        int light = (hi << 8) | lo;
        hi = Wire.read();
        lo = Wire.read(); 
        int gas = (hi << 8) | lo;

        receivedValue = showGas ? gas : light;
        newStateReceived = true;
    }
}

void requestEvent() {
    byte cmd = dequeueIR();
    Wire.write(cmd);
}
