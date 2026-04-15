#define BLYNK_TEMPLATE_ID    "TMPL3clVIW9Hp"
#define BLYNK_TEMPLATE_NAME  "water pipe leakage monitoring system"
#define BLYNK_AUTH_TOKEN     "3dT7_sDlclNretj251qbm4CPAUIa7rSg"
#define BLYNK_PRINT          Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---- Pin Definitions ----
#define RELAY    5
#define SENSOR1  25
#define SENSOR2  26
#define LED_PIN  12

// ---- LCD Setup ----
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---- WiFi & Blynk ----
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Prrr";
char pass[] = "87654321";

// ---- Sensor Variables ----
long previousMillis1 = 0;
long previousMillis2 = 0;
int interval = 1000;

float calibrationFactor1 = 6;
float calibrationFactor2 = 6;

volatile byte pulseCount1 = 0;
volatile byte pulseCount2 = 0;

float flowRate1 = 0.0, flowRate2 = 0.0;

// ---- NEW: Manual Override Variables ----
bool manualOverride = false;   // true = user is controlling motor via Blynk
bool manualRelayState = false; // desired relay state from Blynk button

// ---- Interrupts ----
void IRAM_ATTR pulseCounter1() { pulseCount1++; }
void IRAM_ATTR pulseCounter2() { pulseCount2++; }

// ============================================================
// NEW: Blynk Button on V2 — Press to manually control motor
// In Blynk app: add a Button widget on V2 (Switch mode)
// V3 is used to toggle manual/auto mode
// ============================================================

// V2 — Motor ON/OFF button (only works when manual mode is ON)
BLYNK_WRITE(V2)
{
  manualRelayState = param.asInt(); // 1 = ON, 0 = OFF

  if (manualOverride)
  {
    digitalWrite(RELAY, manualRelayState ? HIGH : LOW);
    digitalWrite(LED_PIN, manualRelayState ? HIGH : LOW);

    lcd.setCursor(0, 0);
    lcd.print(manualRelayState ? "Motor: ON (Man) " : "Motor: OFF(Man) ");
  }
}

// V3 — Manual/Auto mode toggle button (Switch mode in Blynk)
BLYNK_WRITE(V3)
{
  manualOverride = param.asInt(); // 1 = manual, 0 = auto

  lcd.setCursor(0, 1);
  lcd.print(manualOverride ? "Mode: MANUAL    " : "Mode: AUTO      ");

  // When switching back to auto, reset relay to LOW
  // (auto logic will take over in loop)
  if (!manualOverride)
  {
    digitalWrite(RELAY, LOW);
    digitalWrite(LED_PIN, LOW);
  }

  Serial.print("Manual Override: ");
  Serial.println(manualOverride ? "ON" : "OFF");
}

// ============================================================
void setup()
{
  Serial.begin(115200);

  pinMode(SENSOR1, INPUT_PULLUP);
  pinMode(SENSOR2, INPUT_PULLUP);
  pinMode(RELAY, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(RELAY, LOW);
  digitalWrite(LED_PIN, LOW);

  attachInterrupt(digitalPinToInterrupt(SENSOR1), pulseCounter1, FALLING);
  attachInterrupt(digitalPinToInterrupt(SENSOR2), pulseCounter2, FALLING);

  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print(" IoT Water Leak ");
  lcd.setCursor(0, 1);
  lcd.print("   Monitoring   ");
  delay(2000);
  lcd.clear();

  Blynk.begin(auth, ssid, pass);
}

// ============================================================
void loop()
{
  Blynk.run();

  unsigned long currentMillis = millis();

  // ---- Sensor 1 ----
  if (currentMillis - previousMillis1 >= interval)
  {
    noInterrupts();
    byte pulses = pulseCount1;
    pulseCount1 = 0;
    interrupts();

    flowRate1 = (pulses / calibrationFactor1);
    previousMillis1 = currentMillis;

    Serial.print("F1: ");
    Serial.println(flowRate1);

    Blynk.virtualWrite(V0, flowRate1);

    // Only update LCD flow display when in AUTO mode
    if (!manualOverride)
    {
      lcd.setCursor(0, 0);
      lcd.print("F1:");
      lcd.print((int)flowRate1);
      lcd.print(" mL/s   ");
    }
  }

  // ---- Sensor 2 ----
  if (currentMillis - previousMillis2 >= interval)
  {
    noInterrupts();
    byte pulses = pulseCount2;
    pulseCount2 = 0;
    interrupts();

    flowRate2 = (pulses / calibrationFactor2);
    previousMillis2 = currentMillis;

    Serial.print("F2: ");
    Serial.println(flowRate2);

    Blynk.virtualWrite(V1, flowRate2);

    if (!manualOverride)
    {
      lcd.setCursor(0, 1);
      lcd.print("F2:");
      lcd.print((int)flowRate2);
      lcd.print(" mL/s   ");
    }
  }

  // ---- Leakage Detection (only runs in AUTO mode) ----
  if (!manualOverride)
  {
    if (flowRate2 < flowRate1 && flowRate2 < 8)
    {
      lcd.setCursor(0, 0);
      lcd.print("Leakage Detected");
      lcd.setCursor(0, 1);
      lcd.print("F1 > F2         ");

      digitalWrite(RELAY, HIGH);
      digitalWrite(LED_PIN, HIGH);

      Blynk.logEvent("flow_notify", "Water Leakage Detected");
    }
    else
    {
      digitalWrite(RELAY, LOW);
      digitalWrite(LED_PIN, LOW);
    }
  }
}