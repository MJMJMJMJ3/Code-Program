#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL68hIvVOHN"
#define BLYNK_TEMPLATE_NAME "SPP"
#define BLYNK_AUTH_TOKEN "m1uq1QoV-uA4gblfHe9tAdq773kbnKVX"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>

char ssid[] = "iPhone";
char pass[] = "11111111";

const int ACS712_pin = A0, relayPin = D4;
LiquidCrystal_PCF8574 lcd(0x27);

float voltage = 220.0, powerSamples[10] = {0};
unsigned long lastMillis = 0, runtimeSeconds = 0;
int sampleIndex = 0;
float cost = 0.0;

bool relayState = false;
#define NO_LOAD_THRESHOLD 500.0 // ค่ากำลังไฟต่ำสุดที่ถือว่าไม่มีโหลด

BLYNK_WRITE(V4) {
  relayState = param.asInt();
  digitalWrite(relayPin, relayState);

  if (relayState) {
    runtimeSeconds = 0;
    cost = 0.0;
    for (int i = 0; i < 10; i++) powerSamples[i] = 0;
    lcd.display();
    lcd.setBacklight(255);
    Serial.println("Relay ON - Reset all values");
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Power OFF");
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  lcd.begin(20, 4);
  lcd.setBacklight(255);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  lcd.print("System Ready");
  delay(2000);
}

float readRMSCurrent() {
  float sum = 0;
  for (int i = 0; i < 100; i++) {
    float voltageSensor = analogRead(ACS712_pin) * (3.3 / 1024.0);
    float current = (voltageSensor - 2.5) / 0.100;
    sum += sq(current);
    delay(1);
  }
  return sqrt(sum / 100);
}

float calculateMovingAverage(float newPower) {
  powerSamples[sampleIndex] = newPower;
  sampleIndex = (sampleIndex + 1) % 10;
  float sum = 0;
  for (float p : powerSamples) sum += p;
  return sum / 10;
}

void loop() {
  Blynk.run();

  if (!relayState) {
    lcd.setCursor(0, 0);
    lcd.print("Power OFF        "); 
    lcd.setBacklight(255);
    delay(500);
    lcd.clear();
    return;
  }

  lcd.display();
  lcd.setBacklight(255);

  float current = max(readRMSCurrent(), 0.0f);
  float power = constrain(voltage * current, 0.0, 5000.0);
  float smoothPower = calculateMovingAverage(power);
  unsigned long currentMillis = millis();

  if (smoothPower < NO_LOAD_THRESHOLD) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("NO LOAD");
    cost = 0.0; // ไม่คิดค่าใช้จ่ายหากไม่มีโหลด
  } else {
    if (currentMillis - lastMillis >= 1000) {
      lastMillis = currentMillis;
      runtimeSeconds++;
      float energyUsed = (smoothPower * runtimeSeconds) / 3600000.0;
      cost = energyUsed * 4.0;

      lcd.setCursor(0, 0); lcd.print("Power: "); lcd.print(smoothPower, 2); lcd.print(" W");
      lcd.setCursor(0, 1); lcd.print("Current: "); lcd.print(current, 2); lcd.print(" A");
      lcd.setCursor(0, 2); lcd.print("Time: "); lcd.print(runtimeSeconds / 3600); lcd.print("h ");
      lcd.print((runtimeSeconds % 3600) / 60); lcd.print("m "); lcd.print(runtimeSeconds % 60); lcd.print("s");
      lcd.setCursor(0, 3); lcd.print("Cost: "); lcd.print(cost, 2); lcd.print(" B");

      Serial.printf("Time: %lus, Power: %.2f W, Current: %.2f A, Cost: %.2f B\n", runtimeSeconds, smoothPower, current, cost);
    }
  }

  Blynk.virtualWrite(V0, power);
  Blynk.virtualWrite(V3, current);
  Blynk.virtualWrite(V1, cost);
  Blynk.virtualWrite(V2, runtimeSeconds);
}
