/*
 * Woffy 2.0 — L293D Motor Control
 * Smooth ramping, non-blocking
 */

#ifndef MOTORS_H
#define MOTORS_H

// ===== Pins =====
#define FL1  26   // Front-Left  A1 (swapped)
#define FL2  25   // Front-Left  A2 (swapped)
#define FR1  14   // Front-Right B1 (swapped)
#define FR2  27   // Front-Right B2 (swapped)
#define RL1  18   // Rear-Left   A1
#define RL2  19   // Rear-Left   A2
#define RR1  21   // Rear-Right  B1
#define RR2  22   // Rear-Right  B2

#define DEFAULT_SPEED  180

// ===== Ramping config =====
#define RAMP_STEP     25    // PWM change per tick
#define RAMP_INTERVAL 8     // ms between ticks (~70ms 0→180)

// ===== Motor state =====
int targetFL = 0, targetFR = 0, targetRL = 0, targetRR = 0;
int actualFL = 0, actualFR = 0, actualRL = 0, actualRR = 0;
unsigned long lastRampTick = 0;

// ===== Init =====
inline void setupMotors() {
  ledcAttach(FL1, 1000, 8);
  ledcAttach(FL2, 1000, 8);
  ledcAttach(FR1, 1000, 8);
  ledcAttach(FR2, 1000, 8);
  ledcAttach(RL1, 1000, 8);
  ledcAttach(RL2, 1000, 8);
  ledcAttach(RR1, 1000, 8);
  ledcAttach(RR2, 1000, 8);
  Serial.println("Motors: 8 pins, smooth ramping");
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
  targetFL = -speed; targetFR = speed;
  targetRL = -speed; targetRR = speed;
}

inline void turnRight(int speed) {
  targetFL = speed;  targetFR = -speed;
  targetRL = speed;  targetRR = -speed;
}

inline void stopAll() {
  targetFL = 0; targetFR = 0;
  targetRL = 0; targetRR = 0;
}

#endif
