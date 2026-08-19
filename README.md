# REFLEX — Parachute Deployment System

**REFLEX** is an ESP32 + MPU6050 based automatic parachute deployment system designed for drones. It detects critical flight conditions such as **free-fall, excessive attitude deviation, or hard impact** and automatically triggers a servo-driven parachute release mechanism. Multiple confirmation checks help prevent false deployments caused by short-duration spikes or vibration.

## Hardware

* **ESP32 DevKit V1**
* **MPU6050 (GY-521)** — I²C motion sensor
* **SG90 / MG90S Servo** — parachute release mechanism
* **Green LED** — system ready/status indicator
* **Red LED** — fault/deployment indicator
* **3.7V LiPo Battery**
* **TP4056** — LiPo charging module
* **100kΩ + 100kΩ voltage divider** — battery voltage sensing
* **Reset Push-Button**

### Pin Configuration

| Component    | ESP32 GPIO |
| ------------ | ---------: |
| Servo        |   `GPIO18` |
| Green LED    |   `GPIO25` |
| Red LED      |   `GPIO26` |
| Reset Button |   `GPIO27` |
| Battery ADC  |   `GPIO34` |
| MPU6050 SDA  |   `GPIO21` |
| MPU6050 SCL  |   `GPIO22` |

## How It Works

### 1. INIT — System Initialization

When powered on:

* Both LEDs turn **ON**.
* The MPU6050 automatically calibrates for approximately **3 seconds**.
* The drone must remain **completely still during calibration**.

### 2. ARMING — Stability Check

After initialization:

* The **green LED blinks**.
* The system checks for approximately **1 second** of stable IMU readings.
* Once stable readings are confirmed, the system enters the **ARMED** state.

### 3. ARMED — Flight Monitoring

When armed, REFLEX continuously monitors the drone's motion every **20 ms**.

It looks for three critical conditions:

* **Free-Fall:** Total acceleration `< 0.25g`
* **Attitude Failure:** Roll or pitch `> 60°`
* **Hard Impact:** Total acceleration `> 3.5g`

These conditions are continuously evaluated while the system is armed.

### 4. Failure Confirmation

A detected failure is **not immediately treated as a deployment event**.

The detected condition must remain present for **20 consecutive monitoring cycles**.

At a 20 ms monitoring interval:

**20 × 20 ms ≈ 400 ms**

This confirmation period helps filter out short-duration acceleration spikes, vibration, and other temporary disturbances that could otherwise cause a false deployment.

### 5. DEPLOYING → DEPLOYED

Once a failure is confirmed:

* The system enters the **DEPLOYING** state.
* Before deployment, the battery voltage is checked.
* Deployment is **blocked if the battery voltage is below 3.30V**.
* If the battery level is sufficient, the servo moves to **90°** to release the parachute.
* The system then enters the **DEPLOYED** state.
* The **red LED blinks** to indicate deployment/fault status.

### 6. Reset and Re-arming

After deployment:

* Press the **reset button**.
* The servo returns to its stowed position.
* The system starts the initialization and arming process again.

## Software Setup

Use the following development environment:

* **Arduino IDE**
* Board: **ESP32 Dev Module**

### Required Libraries

* `ESP32Servo`
* `MPU6050_light` by **rfetick**

## Adjustable Parameters

The following parameters can be tuned at the top of the sketch according to the airframe and flight characteristics:

* `FREE_FALL_G_THRESHOLD`
* `CRASH_G_THRESHOLD`
* `TILT_THRESHOLD_DEG`
* `CONFIRM_CYCLES`

These allow the detection sensitivity and confirmation behavior to be adjusted without changing the overall system architecture.

## Safety & Testing

Before using REFLEX on an actual drone:

* **Bench-test the electronics and detection logic with the parachute mechanism disconnected.**
* Verify the servo movement and sensor readings before connecting the deployment mechanism.
* **Never power on the system with the servo horn already in the deployed position.**
* Keep the LiPo battery **above 3.6V before flight**.
* Perform controlled testing before attempting actual flight deployment.

---

### Documentation

For complete implementation details, configuration, and operation, refer to:

**`Parachute_Documentation.md`**

---

### Reference Images

### Image 1
<img width="800" alt="Image 1" src="https://github.com/user-attachments/assets/b5a1c037-145f-4249-9975-6022778a7818">

### Image 2
<img width="800" alt="Image 2" src="https://github.com/user-attachments/assets/b5fd5bc5-785b-4f8e-ae9f-eec071d88f56">

### Image 3
<img width="800" alt="Image 3" src="https://github.com/user-attachments/assets/dce06352-d882-440b-852b-943728b8a35f">

### Image 4
<img width="800" alt="Image 4" src="https://github.com/user-attachments/assets/d912ba1c-d201-4ec2-84e3-0cdfed12ca81">
