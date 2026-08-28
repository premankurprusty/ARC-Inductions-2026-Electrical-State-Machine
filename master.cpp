#include <Wire.h>
#include <Servo.h>

Servo myServo;

void setup() {
  Wire.begin();
  Serial.begin(9600);
  myServo.attach(9);
}

enum State {
  STANDBY,
  ACTIVE,
  GAS_ALERT,
  BLACKOUT,
  TEMP_EMERGENCY,
  MULTI_FAULT,
};


void updateState(int lightLevel, int gasLevel, float tempC);
State currentState = STANDBY;

int previousLight = -1; // sentinel (no reading yet)
bool systemActivated = false; // set by remote
bool tempEmergencyLatched = false; // cleared by remote
bool blackoutLatched = false; // cleared when light comes back
bool gasAlertLatched = false; //

void loop() {
  int lightLevel = analogRead(A2);
  int gasLevel = analogRead(A1);
  int rawTemp = analogRead(A3);

  float voltage = rawTemp * (5.0/1023.0);
  float tempC = (voltage - 0.5)*100;

  Serial.print("Light: "); Serial.print(lightLevel);
  Serial.print(" | Gas: "); Serial.print(gasLevel);
  Serial.print(" | Temp: "); Serial.println(tempC);
  Serial.print(" | State: "); Serial.println(currentState);

  updateState(lightLevel, gasLevel, tempC);

  // Master's own outputs, driven by state
  if (currentState == TEMP_EMERGENCY) {
    myServo.write(180);
  } else {
    myServo.write(0); // or whatever "normal" position is
  }

  if (currentState == MULTI_FAULT) {
    tone(2, 1000);
  } else {
    noTone(2);
  }

  Wire.beginTransmission(8);
  Wire.write((byte)currentState);
  Wire.write(highByte(lightLevel));
  Wire.write(lowByte(lightLevel));
  Wire.write(highByte(gasLevel));
  Wire.write(lowByte(gasLevel));
  Wire.endTransmission();

  Wire.requestFrom(8, 1);
  if (Wire.available()) {
      byte irCmd = Wire.read();
      if (irCmd == 1) systemActivated = true;
    else if (irCmd == 3) { 
      tempEmergencyLatched = false;
      if (!systemActivated) {
        currentState = STANDBY;
      }
    }
  }
  delay(500);
}

void updateState(int lightLevel, int gasLevel, float tempC) {
  bool blackout = false;
  if (previousLight != -1) {
    blackout = ((previousLight - lightLevel) > 300);
  }
  if (blackout) blackoutLatched = true;
  if (lightLevel > 400) blackoutLatched = false;

  if (gasLevel > 180) gasAlertLatched = true;
  if (gasLevel < 130) gasAlertLatched = false;

  if (tempC > 45) {
    tempEmergencyLatched = true;
  }

  if (tempEmergencyLatched) {
    currentState = TEMP_EMERGENCY;
    previousLight = lightLevel;
    return;
  }

  if (systemActivated) {
      if (gasAlertLatched && blackoutLatched) {
          currentState = MULTI_FAULT;
      }

    else if (gasAlertLatched) {
        currentState = GAS_ALERT;
    }

    else if (blackoutLatched) {
        currentState = BLACKOUT;
    }

    else {
        currentState = ACTIVE;
    }

      previousLight = lightLevel;
  }
}
