/*
 * Woffy 2.0 — L293D Motor Control + Servo + Ultrasonic
 * Smooth ramping, non-blocking
 */

#ifndef MOTORS_H
#define MOTORS_H

#include <ESP32Servo.h>

// ===== Motor Pins (L293D) =====
// Front Driver
#define FL1  14   // Front-Left  A1
#define FL2  27   // Front-Left  A2
#define FR1  26   // Front-Right B1
#define FR2  25   // Front-Right B2
// Rear Driver
#define RL1  18   // Rear-Left   A1
#define RL2  19   // Rear-Left   A2
#define RR1  32   // Rear-Right  B1
#define RR2  33   // Rear-Right  B2

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
#define RADAR_MAX_RANGE  100   // cm — matches app radar scale

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

// ===== ToF Self-Driving Autopilot =====
// EMA-filtered sensors, adaptive speed, PID wall-follow,
// direction memory, U-trap escape, cliff detection

// --- Thresholds (mm) — tuned for office with chair legs & narrow passages ---
#define AUTO_EMERGENCY_MM   120   // emergency stop (12cm) — tighter for thin chair legs
#define AUTO_OBSTACLE_MM    300   // obstacle threshold (30cm) — earlier reaction for cluttered room
#define AUTO_CLEAR_MM       500   // path clear (50cm) — need more space to be confident
#define AUTO_SIDE_MIN_MM    100   // side clearance (10cm) — chairs are close
#define AUTO_BACKUP_MM      100   // min rear clearance to back up

// --- Speed — slower for cluttered office ---
#define AUTO_SPEED_MAX      140   // max forward PWM (was 170 — safer around furniture)
#define AUTO_SPEED_MIN      80    // min forward PWM in tight space
#define AUTO_TURN_SPEED     166   // turning PWM
#define AUTO_READ_INTERVAL  50    // ms between sensor reads
#define AUTO_TURN_MS        350   // base turn duration
#define AUTO_BACKUP_MS      400   // backup duration

// --- PID wall-following ---
#define WALL_TARGET_MM      150   // ideal distance from wall
#define WALL_KP             0.15f // proportional gain
#define WALL_KD             0.08f // derivative gain

// --- U-trap detection ---
#define UTRAP_HISTORY       8     // track last N turn directions
#define UTRAP_THRESHOLD     6     // if N turns in UTRAP_HISTORY cycles → trapped

// --- Direction memory ---
#define DIR_HISTORY_SIZE    4     // remember last N turn directions
#define DIR_BIAS_WEIGHT     30    // mm bonus for unexplored direction

enum AutoState { AUTO_OFF, AUTO_READING, AUTO_DECIDE,
                 AUTO_DRIVING, AUTO_CURVING, AUTO_TURNING, AUTO_BACKUP };
AutoState autoState = AUTO_OFF;
unsigned long autoStepTime = 0;
unsigned long autoDriveStart = 0;
int autoStuckCounter = 0;
String autoLastTurnDir = "";
String autoLastAction = "";
int autoCycleCount = 0;

// PID wall-following state
int wallLastError = 0;

// Direction memory: track recent turns to avoid revisiting
int dirHistory[DIR_HISTORY_SIZE] = {0};  // +1 = left, -1 = right
int dirHistoryIdx = 0;

// U-trap: track consecutive turn cycles
int turnHistory[UTRAP_HISTORY] = {0};  // 1 = turned this cycle, 0 = drove forward
int turnHistoryIdx = 0;
bool uTrapDetected = false;

// Callback for publishing autopilot status
typedef void (*AutoPublishFunc)(const String& status);
AutoPublishFunc autoPublishFn = nullptr;

inline void setAutoPublisher(AutoPublishFunc fn) { autoPublishFn = fn; }
inline void autoPublish(const String& msg) { if (autoPublishFn) autoPublishFn(msg); }
inline void autoLog(const String& msg) {
  if (autoPublishFn) autoPublishFn("LOG:" + msg);
  Serial.println(msg);
}

inline bool isAutopilotActive() { return autoState != AUTO_OFF; }

inline void startAutopilot() {
  autoState = AUTO_READING;
  autoStuckCounter = 0;
  autoCycleCount = 0;
  autoLastTurnDir = "";
  wallLastError = 0;
  dirHistoryIdx = 0;
  turnHistoryIdx = 0;
  uTrapDetected = false;
  for (int i = 0; i < DIR_HISTORY_SIZE; i++) dirHistory[i] = 0;
  for (int i = 0; i < UTRAP_HISTORY; i++) turnHistory[i] = 0;
  stopAll();
  autoLog("Autopilot ON (v2) | speed:" + String(AUTO_SPEED_MIN) + "-" + String(AUTO_SPEED_MAX));
  autoPublish("AUTO:started");
}

inline void stopAutopilot() {
  autoState = AUTO_OFF;
  stopAll();
  autoLog("Autopilot OFF | cycles:" + String(autoCycleCount));
  autoPublish("AUTO:stopped");
}

// Adaptive speed: proportional to nearest obstacle distance
inline int getAutoSpeed(int nearestMM) {
  if (nearestMM >= AUTO_CLEAR_MM) return AUTO_SPEED_MAX;
  if (nearestMM <= AUTO_OBSTACLE_MM) return AUTO_SPEED_MIN;
  // Linear interpolation between min and max
  float ratio = (float)(nearestMM - AUTO_OBSTACLE_MM) / (AUTO_CLEAR_MM - AUTO_OBSTACLE_MM);
  return AUTO_SPEED_MIN + (int)(ratio * (AUTO_SPEED_MAX - AUTO_SPEED_MIN));
}

// Progressive turn duration: longer when stuck
inline unsigned long getTurnDuration() {
  unsigned long dur = AUTO_TURN_MS + (autoStuckCounter * 80);
  if (uTrapDetected) dur = 800;  // longer turn to escape U-trap
  if (dur > 1000) dur = 1000;
  return dur;
}

// Record a turn direction in history (+1=left, -1=right)
inline void recordTurn(int dir) {
  dirHistory[dirHistoryIdx] = dir;
  dirHistoryIdx = (dirHistoryIdx + 1) % DIR_HISTORY_SIZE;
  turnHistory[turnHistoryIdx] = 1;  // mark as turn cycle
  turnHistoryIdx = (turnHistoryIdx + 1) % UTRAP_HISTORY;
}

// Record a forward drive (not a turn)
inline void recordForward() {
  turnHistory[turnHistoryIdx] = 0;
  turnHistoryIdx = (turnHistoryIdx + 1) % UTRAP_HISTORY;
}

// Count how many recent turns went in a given direction
inline int countRecentDir(int dir) {
  int count = 0;
  for (int i = 0; i < DIR_HISTORY_SIZE; i++) {
    if (dirHistory[i] == dir) count++;
  }
  return count;
}

// Check if trapped in U-shape (too many turns, no forward progress)
inline bool checkUTrap() {
  int turns = 0;
  for (int i = 0; i < UTRAP_HISTORY; i++) turns += turnHistory[i];
  return turns >= UTRAP_THRESHOLD;
}

// Choose best turn direction with memory bias
inline bool chooseTurnLeft(int fl, int fr, int sl, int sr) {
  // Base score from sensor readings
  int leftScore = fl + sl;
  int rightScore = fr + sr;

  // Bias AGAINST recently used direction (explore new paths)
  int leftTurns = countRecentDir(1);
  int rightTurns = countRecentDir(-1);
  leftScore += rightTurns * DIR_BIAS_WEIGHT;   // bonus if we've been turning right
  rightScore += leftTurns * DIR_BIAS_WEIGHT;    // bonus if we've been turning left

  // Wall-following hysteresis (weak — overridden by exploration)
  if (abs(leftScore - rightScore) < 20 && autoLastTurnDir.length() > 0) {
    return (autoLastTurnDir == "left");
  }

  return leftScore > rightScore;
}

// PID wall-following: returns speed differential for inner/outer wheels
// Positive = curve right (away from left wall), negative = curve left
inline int wallFollowPID(int wallDist, bool wallOnLeft) {
  int error = wallDist - WALL_TARGET_MM;  // positive = too far from wall
  if (!wallOnLeft) error = -error;        // flip for right wall

  int derivative = error - wallLastError;
  wallLastError = error;

  float correction = WALL_KP * error + WALL_KD * derivative;

  // Clamp correction to ±60 PWM
  if (correction > 60) correction = 60;
  if (correction < -60) correction = -60;

  return (int)correction;
}

// Call from loop() — non-blocking, ~50ms cycle time
inline void updateAutopilot() {
  if (autoState == AUTO_OFF) return;

  unsigned long now = millis();

  switch (autoState) {

    case AUTO_READING: {
      // Read all ToF sensors (non-blocking timing)
      if (now - autoStepTime < AUTO_READ_INTERVAL) return;
      autoStepTime = now;
      readSensors();
      autoCycleCount++;
      autoState = AUTO_DECIDE;
      break;
    }

    case AUTO_DECIDE: {
      int fc = frontCenter();
      int fl = frontLeft();
      int fr = frontRight();
      int sl = sideLeft();
      int sr = sideRight();
      int bc = backCenter();
      int fmin = frontMin();
      bool cliffF = isCliffFront();
      bool cliffB = isCliffBack();

      // U-trap detection
      uTrapDetected = checkUTrap();

      // Adaptive speed based on nearest obstacle
      int autoSpeed = getAutoSpeed(fmin);

      // Log every 10th cycle
      if (autoCycleCount % 10 == 0) {
        autoLog("C" + String(autoCycleCount) +
                " FC:" + String(fc) + " FL:" + String(fl) + " FR:" + String(fr) +
                " L:" + String(sl) + " R:" + String(sr) +
                " BC:" + String(bc) + " spd:" + String(autoSpeed) +
                (cliffF ? " CLIFF_F!" : "") + (cliffB ? " CLIFF_B!" : "") +
                (uTrapDetected ? " UTRAP!" : "") +
                " stk:" + String(autoStuckCounter));
      }

      // === PRIORITY 1: CLIFF DETECTION ===
      if (cliffF) {
        stopAll();
        autoLog("!! CLIFF FRONT");
        autoPublish("AUTO:cliff-front");
        recordTurn(0);
        if (!cliffB && bc > AUTO_BACKUP_MM) {
          moveBackward(AUTO_TURN_SPEED);
          autoStepTime = now;
          autoState = AUTO_BACKUP;
        } else {
          autoStuckCounter++;
          bool goLeft = chooseTurnLeft(fl, fr, sl, sr);
          if (goLeft) { turnLeft(AUTO_TURN_SPEED); autoLastTurnDir = "left"; recordTurn(1); }
          else { turnRight(AUTO_TURN_SPEED); autoLastTurnDir = "right"; recordTurn(-1); }
          autoStepTime = now;
          autoState = AUTO_TURNING;
        }
        break;
      }

      // === PRIORITY 2: EMERGENCY OBSTACLE ===
      if (fmin < AUTO_EMERGENCY_MM) {
        stopAll();
        autoLog("!! EMERGENCY " + String(fmin) + "mm");
        autoPublish("AUTO:emergency");
        if (!cliffB && bc > AUTO_BACKUP_MM) {
          moveBackward(AUTO_TURN_SPEED);
          autoStepTime = now;
          autoState = AUTO_BACKUP;
        } else {
          autoStuckCounter++;
          bool goLeft = chooseTurnLeft(fl, fr, sl, sr);
          if (goLeft) { turnLeft(AUTO_TURN_SPEED); autoLastTurnDir = "left"; recordTurn(1); }
          else { turnRight(AUTO_TURN_SPEED); autoLastTurnDir = "right"; recordTurn(-1); }
          autoStepTime = now;
          autoState = AUTO_TURNING;
        }
        break;
      }

      // === PRIORITY 3: U-TRAP ESCAPE ===
      if (uTrapDetected && fmin < AUTO_CLEAR_MM) {
        stopAll();
        autoLog("!! U-TRAP — 180° turn");
        autoPublish("AUTO:utrap");
        // Force opposite of recent dominant direction
        int leftCount = countRecentDir(1);
        int rightCount = countRecentDir(-1);
        if (leftCount >= rightCount) {
          turnRight(AUTO_TURN_SPEED);
          autoLastTurnDir = "right";
          recordTurn(-1);
        } else {
          turnLeft(AUTO_TURN_SPEED);
          autoLastTurnDir = "left";
          recordTurn(1);
        }
        autoStuckCounter = 0;  // reset — we're trying something new
        // Clear turn history to give the new direction a fair chance
        for (int i = 0; i < UTRAP_HISTORY; i++) turnHistory[i] = 0;
        autoStepTime = now;
        autoState = AUTO_TURNING;
        break;
      }

      // === PRIORITY 4: CORNER (front + both sides blocked) ===
      if (fmin < AUTO_OBSTACLE_MM && sl < AUTO_OBSTACLE_MM && sr < AUTO_OBSTACLE_MM) {
        stopAll();
        autoStuckCounter++;
        autoLog(">> CORNER (stk:" + String(autoStuckCounter) + ")");
        autoPublish("AUTO:backup");
        if (!cliffB && bc > AUTO_BACKUP_MM) {
          moveBackward(AUTO_TURN_SPEED);
          autoStepTime = now;
          autoState = AUTO_BACKUP;
        } else {
          bool goLeft = chooseTurnLeft(fl, fr, sl, sr);
          if (goLeft) { turnLeft(AUTO_TURN_SPEED); autoLastTurnDir = "left"; recordTurn(1); }
          else { turnRight(AUTO_TURN_SPEED); autoLastTurnDir = "right"; recordTurn(-1); }
          autoStepTime = now;
          autoState = AUTO_TURNING;
        }
        break;
      }

      // === PRIORITY 5: FRONT BLOCKED — smart turn ===
      if (fmin < AUTO_OBSTACLE_MM) {
        stopAll();
        bool goLeft = chooseTurnLeft(fl, fr, sl, sr);
        if (goLeft) {
          turnLeft(AUTO_TURN_SPEED);
          autoLastTurnDir = "left";
          autoLastAction = "left";
          recordTurn(1);
        } else {
          turnRight(AUTO_TURN_SPEED);
          autoLastTurnDir = "right";
          autoLastAction = "right";
          recordTurn(-1);
        }
        autoStuckCounter++;
        autoLog(">> TURN " + autoLastAction + " (FL:" + String(fl) + " FR:" + String(fr) + ")");
        autoPublish("AUTO:" + autoLastAction);
        autoStepTime = now;
        autoState = AUTO_TURNING;
        break;
      }

      // === PRIORITY 6: DRIVE with PID wall-following + adaptive speed ===
      bool wallLeft = (sl < AUTO_SIDE_MIN_MM * 2);   // detect wall within 24cm
      bool wallRight = (sr < AUTO_SIDE_MIN_MM * 2);

      if (wallLeft || wallRight) {
        // PID wall-following: smooth curve to maintain distance
        int pid;
        if (wallLeft && !wallRight) {
          pid = wallFollowPID(sl, true);   // positive = curve right
        } else if (wallRight && !wallLeft) {
          pid = wallFollowPID(sr, false);  // negative = curve left
        } else {
          // Both walls — center between them
          pid = wallFollowPID(sl, true);
        }

        int leftSpeed = autoSpeed - pid;   // pid>0 → left faster → curve right
        int rightSpeed = autoSpeed + pid;

        // Clamp speeds
        if (leftSpeed < AUTO_SPEED_MIN) leftSpeed = AUTO_SPEED_MIN;
        if (rightSpeed < AUTO_SPEED_MIN) rightSpeed = AUTO_SPEED_MIN;
        if (leftSpeed > AUTO_SPEED_MAX) leftSpeed = AUTO_SPEED_MAX;
        if (rightSpeed > AUTO_SPEED_MAX) rightSpeed = AUTO_SPEED_MAX;

        targetFL = leftSpeed;  targetFR = rightSpeed;
        targetRL = leftSpeed;  targetRR = rightSpeed;
        autoLastAction = "wall-follow";
        autoDriveStart = now;
        autoState = AUTO_CURVING;
        recordForward();
      }
      else {
        // Open space — straight ahead at adaptive speed
        moveForward(autoSpeed);
        autoLastAction = "forward";
        autoStuckCounter = 0;
        autoPublish("AUTO:forward");
        autoDriveStart = now;
        autoState = AUTO_DRIVING;
        recordForward();
      }
      break;
    }

    case AUTO_DRIVING:
    case AUTO_CURVING: {
      if (now - autoStepTime >= AUTO_READ_INTERVAL) {
        autoStepTime = now;
        readSensors();

        // CLIFF CHECK while driving
        if (isCliffFront()) {
          stopAll();
          autoLog("!! CLIFF while driving!");
          autoPublish("AUTO:cliff-front");
          autoState = AUTO_READING;
          break;
        }

        int fmin = frontMin();
        int autoSpeed = getAutoSpeed(fmin);

        // Emergency stop
        if (fmin < AUTO_EMERGENCY_MM) {
          stopAll();
          autoLog("!! OBSTACLE while driving " + String(fmin) + "mm");
          autoState = AUTO_READING;
          break;
        }

        // Front getting close — re-evaluate
        if (fmin < AUTO_OBSTACLE_MM) {
          stopAll();
          autoState = AUTO_READING;
          break;
        }

        // Adapt speed continuously while driving
        if (autoState == AUTO_DRIVING) {
          moveForward(autoSpeed);
        }

        // PID wall-following while curving — continuously adjust
        if (autoState == AUTO_CURVING) {
          int sl = sideLeft(), sr = sideRight();
          bool wallL = (sl < AUTO_SIDE_MIN_MM * 2);
          bool wallR = (sr < AUTO_SIDE_MIN_MM * 2);

          if (!wallL && !wallR) {
            // Wall gone — go straight
            moveForward(autoSpeed);
            autoState = AUTO_DRIVING;
          } else {
            // Continue PID wall-following
            int pid;
            if (wallL && !wallR) pid = wallFollowPID(sl, true);
            else if (wallR && !wallL) pid = wallFollowPID(sr, false);
            else pid = wallFollowPID(sl, true);

            int ls = autoSpeed - pid;
            int rs = autoSpeed + pid;
            if (ls < AUTO_SPEED_MIN) ls = AUTO_SPEED_MIN;
            if (rs < AUTO_SPEED_MIN) rs = AUTO_SPEED_MIN;
            if (ls > AUTO_SPEED_MAX) ls = AUTO_SPEED_MAX;
            if (rs > AUTO_SPEED_MAX) rs = AUTO_SPEED_MAX;
            targetFL = ls; targetFR = rs;
            targetRL = ls; targetRR = rs;
          }
        }
      }

      // Re-evaluate after 500ms
      if (now - autoDriveStart >= 500) {
        autoState = AUTO_READING;
      }
      break;
    }

    case AUTO_TURNING: {
      if (now - autoStepTime >= getTurnDuration()) {
        stopAll();
        autoState = AUTO_READING;
      }
      break;
    }

    case AUTO_BACKUP: {
      // Check rear cliff while backing up
      if (now - autoStepTime >= AUTO_READ_INTERVAL) {
        readSensors();
        if (isCliffBack()) {
          stopAll();
          autoLog("!! CLIFF REAR while backing!");
          autoPublish("AUTO:cliff-back");
          // Turn instead of continuing backward
          turnLeft(AUTO_TURN_SPEED);
          autoLastTurnDir = "left";
          autoStepTime = now;
          autoState = AUTO_TURNING;
          break;
        }
      }

      if (now - autoStepTime >= AUTO_BACKUP_MS) {
        stopAll();
        // Turn away from obstacle after backing up
        int sl = sideLeft(), sr = sideRight();
        bool goLeft = (sl > sr);
        if (abs(sl - sr) < 50 && autoLastTurnDir.length() > 0) {
          goLeft = (autoLastTurnDir == "left");
        }
        if (goLeft) {
          turnLeft(AUTO_TURN_SPEED);
          autoLastTurnDir = "left";
        } else {
          turnRight(AUTO_TURN_SPEED);
          autoLastTurnDir = "right";
        }
        autoLog("   post-backup turn " + String(goLeft ? "L" : "R"));
        autoStepTime = now;
        autoState = AUTO_TURNING;
      }
      break;
    }

    default:
      break;
  }
}

#endif
