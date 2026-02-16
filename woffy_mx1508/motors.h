/*
 * Woffy 2.0 — L293D Motor Control
 * Matches AKASH's working config: same pins, same speed
 * Arduino Core 3.x API (ledcAttach + ledcWrite by pin)
 */

#ifndef MOTORS_H
#define MOTORS_H

// ===== EXACT pins from working code =====
#define FL1  25   // Front-Left  A1
#define FL2  26   // Front-Left  A2
#define FR1  27   // Front-Right B1
#define FR2  14   // Front-Right B2
#define RL1  18   // Rear-Left   A1
#define RL2  19   // Rear-Left   A2
#define RR1  21   // Rear-Right  B1
#define RR2  22   // Rear-Right  B2

#define DEFAULT_SPEED  180   // SAME as working code

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
  Serial.println("Motors: 8 pins initialized");
}

// ===== Core: forward on a motor pair =====
inline void motorFwd(int pin1, int pin2, int speed) {
  ledcWrite(pin1, speed);
  ledcWrite(pin2, 0);
}

inline void motorRev(int pin1, int pin2, int speed) {
  ledcWrite(pin1, 0);
  ledcWrite(pin2, speed);
}

inline void motorBrake(int pin1, int pin2) {
  ledcWrite(pin1, 0);
  ledcWrite(pin2, 0);
}

// ===== Per-motor (positive=fwd, negative=rev, 0=stop) =====
inline void motorFL(int s) { if(s>0) motorFwd(FL1,FL2,s); else if(s<0) motorRev(FL1,FL2,-s); else motorBrake(FL1,FL2); }
inline void motorFR(int s) { if(s>0) motorFwd(FR1,FR2,s); else if(s<0) motorRev(FR1,FR2,-s); else motorBrake(FR1,FR2); }
inline void motorRL(int s) { if(s>0) motorFwd(RL1,RL2,s); else if(s<0) motorRev(RL1,RL2,-s); else motorBrake(RL1,RL2); }
inline void motorRR(int s) { if(s>0) motorFwd(RR1,RR2,s); else if(s<0) motorRev(RR1,RR2,-s); else motorBrake(RR1,RR2); }

// ===== Stop =====
inline void stopAll() {
  motorBrake(FL1, FL2);
  motorBrake(FR1, FR2);
  motorBrake(RL1, RL2);
  motorBrake(RR1, RR2);
}

// ===== Movement =====
inline void moveForward(int speed) {
  motorFwd(FL1, FL2, speed);
  motorFwd(FR1, FR2, speed);
  motorFwd(RL1, RL2, speed);
  motorFwd(RR1, RR2, speed);
}

inline void moveBackward(int speed) {
  motorRev(FL1, FL2, speed);
  motorRev(FR1, FR2, speed);
  motorRev(RL1, RL2, speed);
  motorRev(RR1, RR2, speed);
}

inline void turnLeft(int speed) {
  motorRev(FL1, FL2, speed);
  motorFwd(FR1, FR2, speed);
  motorRev(RL1, RL2, speed);
  motorFwd(RR1, RR2, speed);
}

inline void turnRight(int speed) {
  motorFwd(FL1, FL2, speed);
  motorRev(FR1, FR2, speed);
  motorFwd(RL1, RL2, speed);
  motorRev(RR1, RR2, speed);
}

#endif
