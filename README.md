# REFLEX — Parachute Deployment System

ESP32 + MPU6050 based auto-deploy parachute for drones. Detects free-fall, tumble, or hard impact and fires a servo to release the chute, with guards against false triggers.

## Hardware
- ESP32 DevKit V1
- MPU6050 (GY-521, I2C)
- SG90 / MG90S servo
- Green LED (ready) + Red LED (fault/deploy)
- 3.7V LiPo + TP4056 + voltage divider (100kΩ/100kΩ) for battery sense
- Reset push-button

**Pins:** Servo `GPIO18` · Green LED `GPIO25` · Red LED `GPIO26` · Reset `GPIO27` · Battery ADC `GPIO34` · MPU6050 SDA `GPIO21` / SCL `GPIO22`

## How it works
1. **INIT** — both LEDs on, MPU auto-calibrates (~3s, keep drone still).
2. **ARMING** — green blinks until 1s of stable IMU readings.
3. **ARMED** — green solid, monitoring every 20ms for:
   - Free-fall (`<0.25g` total accel)
   - Attitude failure (`>60°` roll/pitch)
   - Hard impact (`>3.5g` total accel)
4. A failure must persist for 20 consecutive cycles (~400ms) before it's confirmed — filters out spikes/vibration.
5. **DEPLOYING → DEPLOYED** — servo fires to 90°, red LED blinks. Deployment is blocked if battery is below 3.30V.
6. Press the reset button to stow the servo and re-arm.

## Setup
- Arduino IDE, board: **ESP32 Dev Module**
- Libraries: `ESP32Servo`, `MPU6050_light` (by rfetick)
- Tune `FREE_FALL_G_THRESHOLD`, `CRASH_G_THRESHOLD`, `TILT_THRESHOLD_DEG`, `CONFIRM_CYCLES` at the top of the sketch for your airframe.

## Safety
- Bench test with the parachute mechanism disconnected before first flight.
- Never power on with the servo horn already in the deployed position.
- Keep the LiPo above 3.6V before flight.

Full details: `Parachute_Documentation.md`.
