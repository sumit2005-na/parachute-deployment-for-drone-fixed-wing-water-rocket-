# REFLEX  Parachute Deployment System

Technical Documentation | ESP32 + MPU6050
Version 5s.0

## 1. Project Overview

This system automatically deploys a parachute on a drone when it detects a catastrophic failure such as motor loss, tumbling, or structural collapse. It uses an ESP32 microcontroller paired with an MPU6050 six-axis IMU to detect anomalous flight conditions and drives a servo motor to release the parachute. Multiple anti-false-deployment guards prevent unintended activation.

## 2. Bill of Materials

| Component | Qty | Notes |
|---|---|---|
| ESP32 DevKit V1 | 1 | 38-pin or 30-pin; any 240 MHz variant |
| MPU6050 module | 1 | GY-521 breakout; I2C address 0x68 |
| SG90 / MG90S servo | 1 | SG90 for lightweight; MG90S for metal horn |
| Green LED (5 mm) | 1 | Forward voltage ~2.1 V |
| Red LED (5 mm) | 1 | Forward voltage ~2.0 V |
| 220 Ω resistor | 2 | Current limiting for LEDs |
| 100 kΩ resistor | 2 | Voltage divider R1 and R2 |
| 100 nF ceramic cap | 1 | ADC filter on battery sense pin |
| Tactile push-button | 1 | Normally-open, 4-pin PCB type |
| 3.7 V LiPo cell | 1 | ≥ 500 mAh recommended |
| TP4056 module | 1 | With built-in over-discharge protection |
| MT3608 boost module | 1 | Optional: 3.7 V → 5 V for servo power |

## 3. Pin Mapping

| ESP32 GPIO | Connected To / Function | 
|---|---|
| GPIO 18 | Servo signal (PWM output) |
| GPIO 25 | Green LED anode via 220 Ω resistor |
| GPIO 26 | Red LED anode via 220 Ω resistor |
| GPIO 27 | Reset button (active-LOW, internal pull-up) |
| GPIO 34 | Battery ADC — voltage divider mid-point |
| GPIO 21 | MPU6050 SDA (I2C data) |
| GPIO 22 | MPU6050 SCL (I2C clock) |
| 3V3 | MPU6050 VCC |
| GND | Common ground — all components |
| VIN / 5V | Servo VCC (red wire) |

## 4. State Machine

| State | LEDs | Description |
|---|---|---|
| INIT | Both ON briefly | Hardware initialisation and MPU calibration |
| ARMING | Green blinks | Waiting for 1 s of stable IMU readings |
| ARMED | Green solid | Normal flight monitoring — ready to detect failure |
| DEPLOYING | Red solid | Failure confirmed — servo commanded to 90° |
| DEPLOYED | Red blinks | Parachute deployed — awaiting manual reset |
| ERROR | Red blinks | Sensor fault or critically low battery |

Red and green LEDs blink on independent timers, so a low-battery warning (green) and a failure confirmation (red) can blink simultaneously without interfering with each other.

## 5. Failure Detection Logic

The system monitors three failure signatures simultaneously on each 20 ms iteration:

**5.1 Free-Fall Detection**
If the total acceleration magnitude (√(ax² + ay² + az²)) falls below 0.25 g, the drone is in freefall indicating motor or structural failure.

**5.2 Attitude Failure**
If the roll or pitch angle computed by the MPU6050 fusion algorithm exceeds ±60° the drone has flipped or is tumbling uncontrollably.

**5.3 Hard Impact Detection**
If total acceleration exceeds 3.5 g the drone has struck an obstacle. This acts as a secondary trigger to deploy if the chute was not already out.

## 6. Anti-False-Deployment Guards

Six independent guards prevent unintended parachute release:

- **Confirmation counter** — a failure condition must persist for 20 consecutive 20 ms samples (~400 ms) before deployment is triggered. A single spike or vibration will not deploy the chute.
- **Armed flag** — the system enters ARMED state only after 50 consecutive stable IMU readings (~1 second). Deployment is impossible before arming completes.
- **Battery gate** — deployment is blocked if battery voltage is below 3.30 V to prevent servo glitches caused by brownout.
- **One-shot latch** — once deployed, the system enters DEPLOYED state and will not re-trigger the servo without a physical button press and re-arming cycle.
- **Recovery hysteresis** — if sensor readings recover before the confirmation counter reaches the threshold the counter resets to zero. This handles brief vibration or turbulence.
- **Arming stability check** — the system refuses to arm if IMU readings are unstable at boot (e.g. during handling or transport).

## 7. Battery Monitoring

A resistor voltage divider (100 kΩ / 100 kΩ) halves the LiPo voltage onto GPIO 34. Eight ADC samples are averaged to reduce noise. The firmware defines two thresholds:

- **3.50 V** — low battery warning: green LED blinks instead of solid.
- **3.30 V** — critical: deployment is blocked and ERROR state is entered.

Voltage divider formula: Vbatt = Vpin × 2 (with R1 = R2 = 100 kΩ)

## 8. Arduino IDE Setup

**Board Manager URL:**
`https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`

**Board selection:** Tools → Board → ESP32 Dev Module

**Required libraries (Library Manager):**
- ESP32Servo — by Kevin Harrington
- MPU6050_light — by rfetick

**Upload settings:**
- Flash Mode: DIO
- Upload Speed: 115200 or 921600
- Partition Scheme: Default

## 9. Calibration Procedure

On first power-on both LEDs illuminate while the MPU6050 runs its automatic offset calibration (approximately 3 seconds). During this time the drone must be placed on a flat, vibration-free surface. If the drone moves during calibration the system may produce false triggers in flight — press the reset button to recalibrate.

## 10. Operation Guide

- Power on the system. Both LEDs flash briefly during calibration.
- Place drone on a flat surface. Green LED will begin blinking during the 1-second arming period.
- Green LED turns solid — system is ARMED and ready for flight.
- If battery drops below 3.50 V, green LED blinks as a low-battery warning.
- On failure detection, red LED begins blinking while the confirmation counter runs.
- After 400 ms of confirmed failure, servo rotates to 90° releasing the parachute.
- Red LED blinks continuously. Press the reset button to stow the servo and re-arm.

## 11. Safety Notes

- Never power on the system with the servo horn in the deployed position — it will attempt to stow immediately.
- Always perform a bench test with the parachute mechanism disconnected before first flight.
- Keep the LiPo charged above 3.6 V before flight to ensure deployment power is available.
- The thresholds (`FREE_FALL_G_THRESHOLD`, `CRASH_G_THRESHOLD`, `TILT_THRESHOLD_DEG`, `CONFIRM_CYCLES`) are defined as constants at the top of the sketch and should be tuned to your specific drone.
- Do not exceed the servo's rated current draw. SG90 stall current is ~700 mA — ensure your 5 V rail can supply this.

## 12. Troubleshooting

| Symptom | Likely Cause & Fix |
|---|---|
| Red LED on at boot | MPU6050 not found — check SDA/SCL wiring and I2C address (AD0 to GND) |
| Servo twitches at power-on | 5 V supply too weak — add MT3608 boost or use separate BEC |
| Parachute deploys on bench | Vibration during calibration — press reset on flat surface and recalibrate |
| Battery reads 0.0 V | GPIO 34 is floating — verify both resistors and capacitor are soldered |
| Green LED never goes solid | IMU readings unstable — check for magnetic interference or loose connections |
