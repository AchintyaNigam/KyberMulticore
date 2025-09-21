#include <Wire.h>
#include <Adafruit_INA219.h>

Adafruit_INA219 ina219;

const int SIGNAL_PIN = 2; // Arduino pin connected to Pico GP2
const int LED_PIN = 13;   // Built-in LED

bool measuring = false;
float totalEnergy_mJ = 0.0;
float totalEnergy_mWh = 0.0;
unsigned long lastTime = 0;
int lastSignalState = LOW;

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }  // wait for serial

  pinMode(SIGNAL_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  if (!ina219.begin()) {
    Serial.println("Failed to find INA219 chip");
    while (1) { delay(10); }
  }

}

void loop() {
  int signalState = digitalRead(SIGNAL_PIN);

  // Detect rising edge → start measuring
  if (signalState == HIGH && lastSignalState == LOW) {
    measuring = true;
    totalEnergy_mJ = 0.0;
    lastTime = millis();
    digitalWrite(LED_PIN, HIGH);  // Turn on LED
    Serial.println("Measurement started");
  Serial.println("Voltage(V)\tCurrent(mA)\tPower(mW)");
  }

  // Detect falling edge → stop measuring
  if (signalState == LOW && lastSignalState == HIGH && measuring) {
    measuring = false;
    digitalWrite(LED_PIN, LOW);   // Turn off LED
    Serial.print("Measurement stopped. Total energy (mWh): ");
    Serial.println(totalEnergy_mWh,3);
  }

  lastSignalState = signalState;

  if (measuring) {
    unsigned long currentTime = millis();
    float dt = (currentTime - lastTime) / 1000.0; // seconds
    lastTime = currentTime;

    float busVoltage = ina219.getBusVoltage_V();   // V
    float current_mA = ina219.getCurrent_mA();     // mA
    float power_mW = ina219.getPower_mW();         // mW

    // Optional: ignore small negative noise
    if (current_mA < 0) current_mA = 0;
    if (power_mW < 0) power_mW = 0;

    totalEnergy_mJ += power_mW * dt; // accumulate energy in mJ
    totalEnergy_mWh = totalEnergy_mJ / 3600;

    
      Serial.print(busVoltage, 3);
      Serial.print("\t\t");
      Serial.print(current_mA, 3);
      Serial.print("\t\t");
      Serial.println(power_mW, 3);
      //Serial.print("\t\t");
      //Serial.println(totalEnergy_mWh, 3);
    

    delay(100); // update every 100ms
  }
}