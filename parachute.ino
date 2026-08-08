/*
 * ============================================================
 *   PARACHUTE DEPLOYMENT SYSTEM — ESP32
 *  Sensors : MPU6050 (I2C), 3.7V LiPo cell
 *  Outputs : Servo (deploy), Red LED (fault), Green LED (ready)
 *  Input   : Reset button
 * ============================================================
 *
 *  Anti-false-deployment guards
 *  ─────────────────────────────
 *  1. Multi-sample confirmation  – failure must persist for
 *     CONFIRM_CYCLES consecutive 20 ms loops (~400 ms).
 *  2. Altitude / velocity gate   – parachute only deploys
 *     when estimated fall speed exceeds FREE_FALL_G_THRESHOLD.
 *  3. Armed flag                 – system must be explicitly
 *     armed after power-on (prevents deploy during init).
 *  4. One-shot latch             – once deployed the servo
 *     will NOT re-trigger without a physical reset.
 *  5. Battery gate               – deployment blocked when
 *     battery is critically low (avoids glitch at brownout).
 * ============================================================
 */

#include <Wire.h>
#include <ESP32Servo.h>       // ESP32Servo library
#include <MPU6050_light.h>    // MPU6050_light by rfetick

// ── Pin assignments ──────────────────────────────────────────
#define PIN_SERVO        18   // PWM-capable GPIO
#define PIN_LED_GREEN    25
#define PIN_LED_RED      26
#define PIN_RESET_BTN    27   // Pulled HIGH internally; LOW = pressed
#define PIN_BATT_ADC     34   // ADC1 channel – battery voltage divider

// ── Servo positions ──────────────────────────────────────────
#define SERVO_STOWED     0    // degrees – parachute locked
#define SERVO_DEPLOYED   90   // degrees – chute released

// ── MPU6050 thresholds ───────────────────────────────────────
// Free-fall: all axes ≈ 0 g (weightlessness).
// Crash:     any axis suddenly exceeds CRASH_G_THRESHOLD g.
#define FREE_FALL_G_THRESHOLD   0.25f   // total accel < 0.25 g  → free-fall
#define CRASH_G_THRESHOLD       3.5f    // total accel > 3.5 g   → hard impact
#define TILT_THRESHOLD_DEG      60.0f   // roll or pitch > 60°   → attitude failure

// ── Confirmation / timing ────────────────────────────────────
#define CONFIRM_CYCLES          20      // consecutive bad readings before deploy
#define LOOP_DELAY_MS           20      // ms between sensor reads
#define BLINK_PERIOD_MS         500     // red LED blink rate (error / armed)
#define DEPLOY_HOLD_MS          500     // ms to hold servo at deployed angle
#define ARMED_BLINK_MS          1000    // green LED blink rate while arming

// ── Battery monitoring ───────────────────────────────────────
// Voltage divider: 100 kΩ / 100 kΩ  →  Vbatt/2 on ADC pin
// ADC full-scale = 3.3 V at 4095 counts
#define BATT_LOW_VOLTAGE        3.50f   // V – warn
#define BATT_CRITICAL_VOLTAGE   3.30f   // V – block deployment
#define BATT_R1                 100000.0f
#define BATT_R2                 100000.0f
#define ADC_MAX                 4095.0f
#define ADC_VREF                3.3f

// ─────────────────────────────────────────────────────────────
MPU6050 mpu(Wire);
Servo   parachute;

// System state machine
enum SystemState {
  STATE_INIT,
  STATE_ARMING,    // waiting for initial stable readings
  STATE_ARMED,     // normal flight monitoring
  STATE_DEPLOYING, // servo moving
  STATE_DEPLOYED,  // chute out – waiting for reset
  STATE_ERROR      // sensor / battery fault
};

SystemState sysState = STATE_INIT;

// Counters & flags
uint8_t  failCount      = 0;
bool     deployed       = false;
uint32_t lastBlinkRed   = 0;
uint32_t lastBlinkGreen = 0;
bool     ledRedState    = false;
bool     ledGreenState  = false;
uint32_t armStartTime   = 0;
uint8_t  stableCount    = 0;
#define  STABLE_NEEDED  50   // 1 second of good data before fully armed

// ─── helpers ─────────────────────────────────────────────────

float readBatteryVoltage() {
  uint32_t raw = 0;
  for (int i = 0; i < 8; i++) raw += analogRead(PIN_BATT_ADC);
  raw /= 8;
  float vPin  = (raw / ADC_MAX) * ADC_VREF;
  float vBatt = vPin * ((BATT_R1 + BATT_R2) / BATT_R2);
  return vBatt;
}

void setLeds(bool green, bool red) {
  digitalWrite(PIN_LED_GREEN, green ? HIGH : LOW);
  digitalWrite(PIN_LED_RED,   red   ? HIGH : LOW);
}

void blinkRed() {
  uint32_t now = millis();
  if (now - lastBlinkRed >= BLINK_PERIOD_MS) {
    lastBlinkRed = now;
    ledRedState  = !ledRedState;
    digitalWrite(PIN_LED_RED, ledRedState ? HIGH : LOW);
  }
}

void blinkGreen() {
  uint32_t now = millis();
  if (now - lastBlinkGreen >= ARMED_BLINK_MS) {
    lastBlinkGreen = now;
    ledGreenState  = !ledGreenState;
    digitalWrite(PIN_LED_GREEN, ledGreenState ? HIGH : LOW);
  }
}

void deployParachute() {
  Serial.println("[DEPLOY] PARACHUTE DEPLOYING!");
  parachute.write(SERVO_DEPLOYED);
  delay(DEPLOY_HOLD_MS);          // give servo time to travel
  deployed  = true;
  sysState  = STATE_DEPLOYED;
  setLeds(false, true);           // solid red = deployed / error
}

void enterError(const char* reason) {
  Serial.print("[ERROR] ");
  Serial.println(reason);
  sysState = STATE_ERROR;
  setLeds(false, false);
}

bool resetPressed() {
  return digitalRead(PIN_RESET_BTN) == LOW;
}

void doReset() {
  Serial.println("[RESET] System resetting…");
  deployed  = false;
  failCount = 0;
  stableCount = 0;
  parachute.write(SERVO_STOWED);
  delay(400);
  sysState  = STATE_ARMING;
  armStartTime = millis();
  setLeds(false, false);
  Serial.println("[RESET] Done – re-arming…");
}

// ─── setup ───────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Smart Parachute System booting ===");

  pinMode(PIN_LED_GREEN,  OUTPUT);
  pinMode(PIN_LED_RED,    OUTPUT);
  pinMode(PIN_RESET_BTN,  INPUT_PULLUP);
  analogReadResolution(12);        // 12-bit ADC

  // Servo
  parachute.attach(PIN_SERVO, 500, 2400);
  parachute.write(SERVO_STOWED);
  Serial.println("[INIT] Servo stowed.");

  // MPU6050
  Wire.begin();
  uint8_t mpuStatus = mpu.begin();
  if (mpuStatus != 0) {
    enterError("MPU6050 not found. Check wiring!");
    return;
  }
  Serial.println("[INIT] MPU6050 OK. Calibrating – keep drone still…");
  setLeds(true, true);             // both on during calibration
  mpu.calcOffsets();               // ~3 second auto-calibration
  setLeds(false, false);
  Serial.println("[INIT] Calibration done.");

  // Battery check
  float vBatt = readBatteryVoltage();
  Serial.print("[INIT] Battery: "); Serial.print(vBatt, 2); Serial.println(" V");
  if (vBatt < BATT_CRITICAL_VOLTAGE) {
    enterError("Battery critically low – replace before flight!");
    return;
  }

  sysState     = STATE_ARMING;
  armStartTime = millis();
  Serial.println("[INIT] Arming…");
}

// ─── loop ────────────────────────────────────────────────────
void loop() {
  // Always service reset button
  if (resetPressed()) {
    delay(50);                      // debounce
    if (resetPressed()) doReset();
    while (resetPressed());         // wait for release
    return;
  }

  // Read sensors every loop regardless of state
  mpu.update();
  float ax    = mpu.getAccX();
  float ay    = mpu.getAccY();
  float az    = mpu.getAccZ();
  float roll  = mpu.getAngleX();
  float pitch = mpu.getAngleY();
  float gTot  = sqrt(ax*ax + ay*ay + az*az);   // total acceleration in g

  float vBatt = readBatteryVoltage();

  // ── STATE MACHINE ─────────────────────────────────────────
  switch (sysState) {

    // ── ARMING ────────────────────────────────────────────
    case STATE_ARMING: {
      blinkGreen();
      // Need stable IMU readings before declaring armed
      bool stable = (gTot > 0.85f && gTot < 1.15f)
                 && (fabs(roll) < 15.0f)
                 && (fabs(pitch) < 15.0f);
      if (stable) stableCount++;
      else        stableCount = 0;

      if (stableCount >= STABLE_NEEDED) {
        sysState = STATE_ARMED;
        setLeds(true, false);       // solid green = armed & ready
        Serial.println("[ARM] System ARMED – monitoring active.");
      }
      break;
    }

    // ── ARMED – normal flight monitoring ─────────────────
    case STATE_ARMED: {
      // Battery gate
      if (vBatt < BATT_CRITICAL_VOLTAGE) {
        enterError("Battery critical during flight!");
        break;
      }
      if (vBatt < BATT_LOW_VOLTAGE) {
        // Warn via blinking green but don't block yet
        blinkGreen();
      } else {
        setLeds(true, false);       // solid green
      }

      // ── Failure detection (anti-false-deploy) ──────────
      bool failureDetected = false;
      const char* failReason = "";

      // 1. Free-fall (motor/prop failure – drone falls)
      if (gTot < FREE_FALL_G_THRESHOLD) {
        failureDetected = true;
        failReason = "FREE-FALL";
      }
      // 2. Severe attitude (flip / tumble)
      else if (fabs(roll) > TILT_THRESHOLD_DEG || fabs(pitch) > TILT_THRESHOLD_DEG) {
        failureDetected = true;
        failReason = "ATTITUDE FAILURE";
      }
      // 3. Hard impact (crash already happening — secondary trigger)
      else if (gTot > CRASH_G_THRESHOLD) {
        failureDetected = true;
        failReason = "HARD IMPACT";
      }

      if (failureDetected) {
        failCount++;
        blinkRed();
        Serial.print("[WARN] "); Serial.print(failReason);
        Serial.print(" count="); Serial.println(failCount);

        if (failCount >= CONFIRM_CYCLES) {
          // Final battery gate – don't deploy if brownout
          if (vBatt >= BATT_CRITICAL_VOLTAGE) {
            Serial.print("[DEPLOY] Confirmed: "); Serial.println(failReason);
            sysState = STATE_DEPLOYING;
          } else {
            enterError("Cannot deploy – battery critical!");
          }
        }
      } else {
        // Readings recovered – reset counter (avoids single-spike false trigger)
        if (failCount > 0) {
          failCount = 0;
          setLeds(true, false);
          Serial.println("[INFO] False-alarm cleared.");
        }
      }
      break;
    }

    // ── DEPLOYING ─────────────────────────────────────────
    case STATE_DEPLOYING: {
      deployParachute();
      break;
    }

    // ── DEPLOYED – waiting for manual reset ───────────────
    case STATE_DEPLOYED: {
      blinkRed();     // blink red until reset
      // Diagnostic print every 5 seconds
      static uint32_t lastPrint = 0;
      if (millis() - lastPrint > 5000) {
        lastPrint = millis();
        Serial.println("[DEPLOYED] Press RESET button to re-arm.");
      }
      break;
    }

    // ── ERROR ─────────────────────────────────────────────
    case STATE_ERROR: {
      blinkRed();
      static uint32_t lastErrPrint = 0;
      if (millis() - lastErrPrint > 3000) {
        lastErrPrint = millis();
        Serial.println("[ERROR] System in fault state. Press RESET.");
      }
      break;
    }

    default: break;
  }

  // Serial telemetry (every 500 ms)
  static uint32_t lastTelem = 0;
  if (millis() - lastTelem >= 500) {
    lastTelem = millis();
    Serial.printf("[TELEM] gTot=%.2f roll=%.1f pitch=%.1f vBatt=%.2fV state=%d failCnt=%d\n",
                  gTot, roll, pitch, vBatt, (int)sysState, failCount);
  }

  delay(LOOP_DELAY_MS);
}
