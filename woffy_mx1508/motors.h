/*
 * Woffy 2.0 — L293D Motor Control + Servo + Ultrasonic
 * Smooth ramping, non-blocking
 */

#ifndef MOTORS_H
#define MOTORS_H

#include <ESP32Servo.h>

// ===== Motor Pins (L293D) =====
// Front Driver (A=Right, B=Left on this L293D)
#define FL1  26   // Front-Left  B1
#define FL2  25   // Front-Left  B2
#define FR1  14   // Front-Right A1
#define FR2  27   // Front-Right A2
// Rear Driver
#define RL1  18   // Rear-Left   A1
#define RL2  19   // Rear-Left   A2
#define RR1  21   // Rear-Right  B1
#define RR2  22   // Rear-Right  B2

// ===== Servo =====
#define SERVO_PIN 13

// ===== Ultrasonic (HC-SR04) =====
#define TRIG_PIN 4
#define ECHO_PIN 15

#define DEFAULT_SPEED  180

// ===== Ramping config =====
#define RAMP_STEP     15    // PWM change per tick (smoother)
#define RAMP_INTERVAL 4     // ms between ticks (~48ms 0→180)

// ===== PWM config =====
#define PWM_FREQ      20000 // 20kHz — silent, smooth at low speeds
#define PWM_RESOLUTION 8    // 0-255

// ===== Motor state =====
int targetFL = 0, targetFR = 0, targetRL = 0, targetRR = 0;
int actualFL = 0, actualFR = 0, actualRL = 0, actualRR = 0;
unsigned long lastRampTick = 0;

// ===== Init =====
inline void setupMotors() {
  ledcAttach(FL1, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(FL2, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(FR1, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(FR2, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(RL1, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(RL2, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(RR1, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(RR2, PWM_FREQ, PWM_RESOLUTION);
  Serial.println("Motors: 8 pins, 20kHz PWM, smooth ramping");
}

// ===== Low-level: apply signed speed to a motor pair =====
inline void applyMotor(int pin1, int pin2, int s) {
  if (s > 0) {
    ledcWrite(pin1, s);
    ledcWrite(pin2, 0);
  } else if (s < 0) {
    ledcWrite(pin1, 0);
    ledcWrite(pin2, -s);
  } else {
    ledcWrite(pin1, 0);
    ledcWrite(pin2, 0);
  }
}

// ===== Ramp a single value toward target =====
inline int rampToward(int current, int target) {
  if (current == target) return current;

  // Reversing direction: decelerate to 0 first
  if ((current > 0 && target < 0) || (current < 0 && target > 0)) {
    if (current > RAMP_STEP) return current - RAMP_STEP;
    if (current < -RAMP_STEP) return current + RAMP_STEP;
    return 0; // hit zero, next tick will accelerate in new direction
  }

  // Same direction or from zero: ramp toward target
  int diff = target - current;
  if (abs(diff) <= RAMP_STEP) return target;
  return current + (diff > 0 ? RAMP_STEP : -RAMP_STEP);
}

// ===== Call from loop() — non-blocking =====
inline void updateMotors() {
  if (millis() - lastRampTick < RAMP_INTERVAL) return;
  lastRampTick = millis();

  actualFL = rampToward(actualFL, targetFL);
  actualFR = rampToward(actualFR, targetFR);
  actualRL = rampToward(actualRL, targetRL);
  actualRR = rampToward(actualRR, targetRR);

  applyMotor(FL1, FL2, actualFL);
  applyMotor(FR1, FR2, actualFR);
  applyMotor(RL1, RL2, actualRL);
  applyMotor(RR1, RR2, actualRR);
}

// ===== Movement commands (set targets) =====
inline void moveForward(int speed) {
  targetFL = speed;  targetFR = speed;
  targetRL = speed;  targetRR = speed;
}

inline void moveBackward(int speed) {
  targetFL = -speed; targetFR = -speed;
  targetRL = -speed; targetRR = -speed;
}

inline void turnLeft(int speed) {
  targetFL = speed;  targetFR = -speed;
  targetRL = speed;  targetRR = -speed;
}

inline void turnRight(int speed) {
  targetFL = -speed; targetFR = speed;
  targetRL = -speed; targetRR = speed;
}

inline void stopAll() {
  targetFL = 0; targetFR = 0;
  targetRL = 0; targetRR = 0;
}

// ===== Servo =====
Servo headServo;
int servoAngle = 90;  // center

inline void setupServo() {
  headServo.setPeriodHertz(50);           // Standard 50Hz servo signal
  headServo.attach(SERVO_PIN, 500, 2400); // min/max pulse width in µs
  headServo.write(90);
  Serial.println("Servo: GPIO 13, 50Hz, center");
}

inline void setServo(int angle) {
  if (angle < 0) angle = 0;
  if (angle > 180) angle = 180;
  servoAngle = angle;
  headServo.write(angle);
}

// ===== Ultrasonic =====
inline void setupUltrasonic() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial.println("Ultrasonic: TRIG=4, ECHO=2");
}

// Returns distance in cm, 0 if no echo
inline float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);  // 30ms timeout (~5m max)
  if (duration == 0) return 0;
  return duration * 0.0343 / 2.0;
}

// Sweep servo and scan distances (blocking — for serial/simple use)
inline void scanSweep() {
  Serial.println("Scan: sweeping...");
  for (int angle = 0; angle <= 180; angle += 30) {
    setServo(angle);
    delay(200);
    float dist = getDistance();
    Serial.print("  ");
    Serial.print(angle);
    Serial.print("° → ");
    Serial.print(dist, 1);
    Serial.println(" cm");
  }
  setServo(90);
  Serial.println("Scan: done");
}

// ===== Non-blocking Radar Sweep =====
#define RADAR_STEP       3     // degrees per step
#define RADAR_SETTLE_MS  25    // ms to let servo settle before reading
#define RADAR_MAX_RANGE  200   // cm — clamp readings beyond this

enum RadarState { RADAR_OFF, RADAR_MOVING, RADAR_READING };
RadarState radarState = RADAR_OFF;
int radarAngle = 0;
int radarDirection = 1;         // 1 = sweeping up (0→180), -1 = sweeping down
bool radarContinuous = false;   // keep sweeping until stopped
unsigned long radarStepTime = 0;

// Called externally to publish each radar data point
typedef void (*RadarPublishFunc)(int angle, float distance);
RadarPublishFunc radarPublishFn = nullptr;

inline void setRadarPublisher(RadarPublishFunc fn) {
  radarPublishFn = fn;
}

inline void startRadar(bool continuous) {
  radarContinuous = continuous;
  radarAngle = 0;
  radarDirection = 1;
  radarState = RADAR_MOVING;
  radarStepTime = millis();
  setServo(0);
  Serial.println("Radar: started");
}

inline void stopRadar() {
  radarState = RADAR_OFF;
  radarContinuous = false;
  setServo(90);
  Serial.println("Radar: stopped");
}

inline bool isRadarActive() {
  return radarState != RADAR_OFF;
}

// Call from loop() — non-blocking
inline void updateRadar() {
  if (radarState == RADAR_OFF) return;

  unsigned long now = millis();

  if (radarState == RADAR_MOVING) {
    // Wait for servo to settle
    if (now - radarStepTime >= RADAR_SETTLE_MS) {
      radarState = RADAR_READING;
    }
    return;
  }

  if (radarState == RADAR_READING) {
    // Take distance reading
    float dist = getDistance();
    if (dist <= 0 || dist > RADAR_MAX_RANGE) dist = RADAR_MAX_RANGE;

    // Publish data point
    if (radarPublishFn) {
      radarPublishFn(radarAngle, dist);
    }

    // Move to next angle
    radarAngle += radarDirection * RADAR_STEP;

    // Check bounds
    if (radarAngle > 180) {
      radarAngle = 180;
      radarDirection = -1;
      if (!radarContinuous) {
        // Single sweep done
        radarState = RADAR_OFF;
        setServo(90);
        if (radarPublishFn) radarPublishFn(-1, 0); // -1 = sweep complete signal
        Serial.println("Radar: sweep done");
        return;
      }
    } else if (radarAngle < 0) {
      radarAngle = 0;
      radarDirection = 1;
      if (!radarContinuous) {
        radarState = RADAR_OFF;
        setServo(90);
        if (radarPublishFn) radarPublishFn(-1, 0);
        Serial.println("Radar: sweep done");
        return;
      }
    }

    setServo(radarAngle);
    radarStepTime = now;
    radarState = RADAR_MOVING;
  }
}

#endif
