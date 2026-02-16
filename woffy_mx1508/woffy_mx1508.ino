/*
 * Woffy 2.0 — L293D 4WD + Wi-Fi + Full Test Suite
 * Pins & speed match AKASH's working ESP-IDF code exactly
 */

#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "motors.h"
#include "html.h"

WebServer server(80);

void setupWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WOFFY_SSID, WOFFY_PASS);
  Serial.print("Woffy AP: ");
  Serial.println(WiFi.softAPIP());
}

void handleRoot() {
  server.send_P(200, "text/html", CONTROL_HTML);
}

void handleCommand() {
  if (!server.hasArg("cmd")) { server.send(400, "text/plain", "Missing cmd"); return; }
  String cmd = server.arg("cmd");
  int speed = server.hasArg("speed") ? server.arg("speed").toInt() : DEFAULT_SPEED;

  int slow = speed / 3;

  if (cmd == "fwd")        moveForward(speed);
  else if (cmd == "bwd")   moveBackward(speed);
  else if (cmd == "left")  turnLeft(speed);
  else if (cmd == "right") turnRight(speed);
  else if (cmd == "stop")  stopAll();
  // Curves: fwd + left/right
  else if (cmd == "cl") {
    motorFwd(FL1, FL2, slow); motorFwd(RL1, RL2, slow);
    motorFwd(FR1, FR2, speed); motorFwd(RR1, RR2, speed);
  }
  else if (cmd == "cr") {
    motorFwd(FL1, FL2, speed); motorFwd(RL1, RL2, speed);
    motorFwd(FR1, FR2, slow); motorFwd(RR1, RR2, slow);
  }
  // Reverse curves: bwd + left/right
  else if (cmd == "bkl") {
    motorRev(FL1, FL2, slow); motorRev(RL1, RL2, slow);
    motorRev(FR1, FR2, speed); motorRev(RR1, RR2, speed);
  }
  else if (cmd == "bkr") {
    motorRev(FL1, FL2, speed); motorRev(RL1, RL2, speed);
    motorRev(FR1, FR2, slow); motorRev(RR1, RR2, slow);
  }
  else if (cmd == "test") {
    server.send(200, "text/plain", "Running all tests...");
    test4_allMotors(DEFAULT_SPEED); delay(2000);
    test1_individual(DEFAULT_SPEED); delay(2000);
    test2_directions(DEFAULT_SPEED); delay(2000);
    test3_speedSweep(); delay(2000);
    test5_figure8(DEFAULT_SPEED);
    return;
  }
  else if (cmd == "test1") { server.send(200, "text/plain", "Running..."); test1_individual(DEFAULT_SPEED); return; }
  else if (cmd == "test2") { server.send(200, "text/plain", "Running..."); test2_directions(DEFAULT_SPEED); return; }
  else if (cmd == "test3") { server.send(200, "text/plain", "Running..."); test3_speedSweep(); return; }
  else if (cmd == "test4") { server.send(200, "text/plain", "Running..."); test4_allMotors(DEFAULT_SPEED); return; }
  else if (cmd == "test5") { server.send(200, "text/plain", "Running..."); test5_figure8(DEFAULT_SPEED); return; }
  else { server.send(400, "text/plain", "Unknown"); return; }

  server.send(200, "text/plain", "OK");
}

/* ====================================================
 *  TEST 1: Individual Motor Spin (FWD + REV each)
 *  Check: each motor spins, correct direction
 * ==================================================== */
void test1_individual(int speed) {
  Serial.println();
  Serial.println("========================================");
  Serial.println("  TEST 1: INDIVIDUAL MOTOR SPIN");
  Serial.printf("  Speed: PWM %d\n", speed);
  Serial.println("========================================");

  const char* names[] = {"FRONT-LEFT (25/26)", "FRONT-RIGHT (27/14)",
                         "REAR-LEFT (18/19)",  "REAR-RIGHT (21/22)"};
  int pins1[] = {FL1, FR1, RL1, RR1};
  int pins2[] = {FL2, FR2, RL2, RR2};

  for (int i = 0; i < 4; i++) {
    Serial.printf("\n--- %s: FORWARD ---\n", names[i]);
    motorFwd(pins1[i], pins2[i], speed);
    delay(1500);
    motorBrake(pins1[i], pins2[i]);
    delay(800);

    Serial.printf("--- %s: REVERSE ---\n", names[i]);
    motorRev(pins1[i], pins2[i], speed);
    delay(1500);
    motorBrake(pins1[i], pins2[i]);
    delay(800);
  }
  Serial.println("\n>> Test 1 COMPLETE");
}

/* ====================================================
 *  TEST 2: Direction Verification
 *  All motors: forward, backward, spin left, spin right
 * ==================================================== */
void test2_directions(int speed) {
  Serial.println();
  Serial.println("========================================");
  Serial.println("  TEST 2: DIRECTION VERIFICATION");
  Serial.printf("  Speed: PWM %d\n", speed);
  Serial.println("========================================");

  Serial.println("\n>> ALL FORWARD — robot should move forward");
  moveForward(speed);
  delay(2000);
  stopAll();
  delay(1000);

  Serial.println(">> ALL BACKWARD — robot should move backward");
  moveBackward(speed);
  delay(2000);
  stopAll();
  delay(1000);

  Serial.println(">> SPIN LEFT — robot should rotate left");
  turnLeft(speed);
  delay(1500);
  stopAll();
  delay(1000);

  Serial.println(">> SPIN RIGHT — robot should rotate right");
  turnRight(speed);
  delay(1500);
  stopAll();

  Serial.println("\n>> Test 2 COMPLETE");
}

/* ====================================================
 *  TEST 3: Speed Sweep (Slip Detection)
 *  PWM 60 → 200, watch where wheels start slipping
 * ==================================================== */
void test3_speedSweep() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("  TEST 3: SPEED SWEEP (SLIP DETECTION)");
  Serial.println("  WATCH wheels — note when they slip!");
  Serial.println("========================================");

  int speeds[] = {60, 80, 100, 120, 140, 160, 180, 200};

  for (int i = 0; i < 8; i++) {
    int rpm = speeds[i] * 300 / 255;
    Serial.printf("\n>> PWM %3d | %2d%% | ~%d RPM\n", speeds[i], speeds[i]*100/255, rpm);
    moveForward(speeds[i]);
    delay(2000);
    stopAll();
    delay(1500);
  }

  Serial.println("\n>> Test 3 COMPLETE");
  Serial.println(">> Note the highest PWM without slip = safe max");
}

/* ====================================================
 *  TEST 4: All Motors Together (from working code)
 *  Same sequence as AKASH's original test
 * ==================================================== */
void test4_allMotors(int speed) {
  Serial.println();
  Serial.println("========================================");
  Serial.println("  TEST 4: ALL MOTORS (AKASH SEQUENCE)");
  Serial.printf("  Speed: PWM %d\n", speed);
  Serial.println("========================================");

  Serial.println("\n1. Front Left...");
  motorFwd(FL1, FL2, speed);
  delay(1500);
  motorBrake(FL1, FL2);
  delay(800);

  Serial.println("2. Front Right...");
  motorFwd(FR1, FR2, speed);
  delay(1500);
  motorBrake(FR1, FR2);
  delay(800);

  Serial.println("3. Rear Left...");
  motorFwd(RL1, RL2, speed);
  delay(1500);
  motorBrake(RL1, RL2);
  delay(800);

  Serial.println("4. Rear Right...");
  motorFwd(RR1, RR2, speed);
  delay(1500);
  motorBrake(RR1, RR2);
  delay(800);

  Serial.println("5. ALL Forward...");
  moveForward(speed);
  delay(3000);

  Serial.println("6. STOP ALL");
  stopAll();

  Serial.println("\n>> Test 4 COMPLETE");
}

/* ====================================================
 *  TEST 5: Figure-8 Pattern
 *  Forward → right curve → forward → left curve
 * ==================================================== */
void test5_figure8(int speed) {
  Serial.println();
  Serial.println("========================================");
  Serial.println("  TEST 5: FIGURE-8 MOVEMENT");
  Serial.println("========================================");

  int slow = speed / 2;

  for (int lap = 1; lap <= 2; lap++) {
    Serial.printf("\n>> Lap %d/2\n", lap);

    Serial.println("  Straight...");
    moveForward(speed);
    delay(1500);

    Serial.println("  Curve right...");
    motorFwd(FL1, FL2, speed);
    motorFwd(RL1, RL2, speed);
    motorFwd(FR1, FR2, slow);
    motorFwd(RR1, RR2, slow);
    delay(2000);

    Serial.println("  Straight...");
    moveForward(speed);
    delay(1500);

    Serial.println("  Curve left...");
    motorFwd(FL1, FL2, slow);
    motorFwd(RL1, RL2, slow);
    motorFwd(FR1, FR2, speed);
    motorFwd(RR1, RR2, speed);
    delay(2000);
  }

  stopAll();
  Serial.println("\n>> Test 5 COMPLETE");
}

// ===== Serial Command Handler =====
void handleSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;

  Serial.print("> ");
  Serial.println(cmd);

  if      (cmd == "fwd")   { Serial.println("Forward");  moveForward(DEFAULT_SPEED); }
  else if (cmd == "bwd")   { Serial.println("Backward"); moveBackward(DEFAULT_SPEED); }
  else if (cmd == "left")  { Serial.println("Left");     turnLeft(DEFAULT_SPEED); }
  else if (cmd == "right") { Serial.println("Right");    turnRight(DEFAULT_SPEED); }
  else if (cmd == "stop")  { Serial.println("Stop");     stopAll(); }
  else if (cmd == "test") {
    Serial.println("=== RUNNING ALL TESTS ===");
    test4_allMotors(DEFAULT_SPEED);
    delay(2000);
    test1_individual(DEFAULT_SPEED);
    delay(2000);
    test2_directions(DEFAULT_SPEED);
    delay(2000);
    test3_speedSweep();
    delay(2000);
    test5_figure8(DEFAULT_SPEED);
    Serial.println();
    Serial.println("========================================");
    Serial.println("  ALL 5 TESTS COMPLETE!");
    Serial.println("========================================");
  }
  else if (cmd == "test1") { test1_individual(DEFAULT_SPEED); }
  else if (cmd == "test2") { test2_directions(DEFAULT_SPEED); }
  else if (cmd == "test3") { test3_speedSweep(); }
  else if (cmd == "test4") { test4_allMotors(DEFAULT_SPEED); }
  else if (cmd == "test5") { test5_figure8(DEFAULT_SPEED); }
  else {
    Serial.println("Commands:");
    Serial.println("  fwd bwd left right stop");
    Serial.println("  test  — run all 5 tests");
    Serial.println("  test1 — individual motor spin (fwd+rev)");
    Serial.println("  test2 — direction verification");
    Serial.println("  test3 — speed sweep (slip detection)");
    Serial.println("  test4 — AKASH original sequence");
    Serial.println("  test5 — figure-8 pattern");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("############################################");
  Serial.println("#  WOFFY 2.0 — L293D Full Test Suite       #");
  Serial.printf( "#  Default speed: PWM %d (~210 RPM)       #\n", DEFAULT_SPEED);
  Serial.println("############################################");

  setupMotors();
  setupWiFi();

  server.on("/", handleRoot);
  server.on("/cmd", HTTP_GET, handleCommand);
  server.begin();

  Serial.println("WiFi: " + String(WOFFY_SSID) + " -> http://" + WiFi.softAPIP().toString());
  Serial.println("Serial: fwd bwd left right stop test test1-5");
}

void loop() {
  server.handleClient();
  handleSerial();
}
