/*
 * Woffy 2.0 — Full Hardware Test
 * Tests: 4 Motors + MCP23017 GPIO Expander + 8x VL53L0X ToF Sensors
 *
 * Wiring:
 *   I2C: SDA=21, SCL=22 (ESP32 default)
 *   MCP23017: addr 0x20 (A0=A1=A2=GND)
 *   VL53L0X XSHUT pins via MCP23017 GPA0-GPA7
 *
 * Motor Pins (2x L293D):
 *   FL: 14/27  FR: 26/25  RL: 18/19  RR: 32/33
 *
 * Serial: 115200 baud — interactive menu
 */

#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_VL53L0X.h>

// ==================== MOTOR PINS ====================
// Front Driver
#define FL1 14   // Front-Left A1
#define FL2 27   // Front-Left A2
#define FR1 26   // Front-Right B1
#define FR2 25   // Front-Right B2
// Rear Driver
#define RL1 18   // Rear-Left A1
#define RL2 19   // Rear-Left A2
#define RR1 32   // Rear-Right B1
#define RR2 33   // Rear-Right B2

#define PWM_FREQ      20000
#define PWM_RESOLUTION 8
#define TEST_SPEED    150  // safe test speed

// ==================== I2C ====================
#define I2C_SDA 21
#define I2C_SCL 22
#define MCP23017_ADDR 0x20

// ==================== SENSOR MAP ====================
// MCP23017 GPA pins → VL53L0X XSHUT
// Each sensor gets a unique I2C address starting from 0x30
const char* sensorNames[] = {
  "Front-45°",     // GPA0
  "Back-45°",      // GPA1
  "Front-Right",   // GPA2
  "Front-Center",  // GPA3
  "Front-Left",    // GPA4
  "Back-Center",   // GPA5
  "Left",          // GPA6
  "Right"          // GPA7
};
#define NUM_SENSORS 8
#define VL53L0X_BASE_ADDR 0x30  // sensors get 0x30-0x37

Adafruit_MCP23X17 mcp;
Adafruit_VL53L0X tof[NUM_SENSORS];
bool sensorOK[NUM_SENSORS] = {false};
bool mcpOK = false;

// ==================== MOTOR HELPERS ====================
struct MotorPins {
  int pin1;
  int pin2;
  const char* name;
};

MotorPins motors[] = {
  {FL1, FL2, "Front-Left"},
  {FR1, FR2, "Front-Right"},
  {RL1, RL2, "Rear-Left"},
  {RR1, RR2, "Rear-Right"}
};
#define NUM_MOTORS 4

void setupMotorPins() {
  for (int i = 0; i < NUM_MOTORS; i++) {
    ledcAttach(motors[i].pin1, PWM_FREQ, PWM_RESOLUTION);
    ledcAttach(motors[i].pin2, PWM_FREQ, PWM_RESOLUTION);
  }
  Serial.println("[OK] Motor PWM pins configured (20kHz, 8-bit)");
}

void driveMotor(int pin1, int pin2, int speed) {
  if (speed > 0) {
    ledcWrite(pin1, speed);
    ledcWrite(pin2, 0);
  } else if (speed < 0) {
    ledcWrite(pin1, 0);
    ledcWrite(pin2, -speed);
  } else {
    ledcWrite(pin1, 0);
    ledcWrite(pin2, 0);
  }
}

void stopAll() {
  for (int i = 0; i < NUM_MOTORS; i++) {
    driveMotor(motors[i].pin1, motors[i].pin2, 0);
  }
}

// ==================== I2C SCANNER ====================
void i2cScan() {
  Serial.println("\n--- I2C Bus Scan ---");
  int found = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Found device at 0x%02X", addr);
      if (addr == MCP23017_ADDR) Serial.print(" (MCP23017)");
      else if (addr == 0x29) Serial.print(" (VL53L0X default)");
      else if (addr >= VL53L0X_BASE_ADDR && addr < VL53L0X_BASE_ADDR + NUM_SENSORS)
        Serial.printf(" (VL53L0X #%d: %s)", addr - VL53L0X_BASE_ADDR, sensorNames[addr - VL53L0X_BASE_ADDR]);
      Serial.println();
      found++;
    }
  }
  Serial.printf("Total: %d device(s)\n", found);
}

// ==================== MCP23017 SETUP ====================
bool setupMCP() {
  Serial.println("\n--- MCP23017 GPIO Expander ---");
  if (!mcp.begin_I2C(MCP23017_ADDR)) {
    Serial.println("[FAIL] MCP23017 not found at 0x20!");
    Serial.println("       Check: SDA→21, SCL→22, A0/A1/A2→GND, VCC→3.3V");
    return false;
  }
  Serial.println("[OK] MCP23017 detected at 0x20");

  // Configure GPA0-GPA7 as outputs for XSHUT
  for (int i = 0; i < NUM_SENSORS; i++) {
    mcp.pinMode(i, OUTPUT);
    mcp.digitalWrite(i, LOW);  // all sensors OFF initially
  }
  Serial.println("[OK] GPA0-GPA7 set as OUTPUT (all LOW = sensors XSHUT off)");
  return true;
}

// ==================== MCP23017 PIN TEST ====================
void testMCPpins() {
  Serial.println("\n--- MCP23017 Pin Toggle Test ---");
  if (!mcpOK) {
    Serial.println("[SKIP] MCP23017 not initialized");
    return;
  }

  for (int i = 0; i < 8; i++) {
    mcp.digitalWrite(i, HIGH);
    Serial.printf("  GPA%d HIGH (%s XSHUT) — ", i, sensorNames[i]);

    // Quick I2C scan to see if a VL53L0X appears at default 0x29
    delay(10);  // VL53L0X needs ~2ms after XSHUT HIGH to boot
    Wire.beginTransmission(0x29);
    bool found = (Wire.endTransmission() == 0);
    Serial.println(found ? "VL53L0X responded at 0x29" : "no VL53L0X at 0x29");

    mcp.digitalWrite(i, LOW);  // turn off again
    delay(5);
  }
  Serial.println("[OK] MCP23017 pin toggle complete");
}

// ==================== VL53L0X INIT ====================
void setupSensors() {
  Serial.println("\n--- VL53L0X Sensor Initialization ---");
  if (!mcpOK) {
    Serial.println("[SKIP] MCP23017 required for XSHUT control");
    return;
  }

  // All XSHUT LOW — all sensors off
  for (int i = 0; i < NUM_SENSORS; i++) {
    mcp.digitalWrite(i, LOW);
  }
  delay(10);

  // Bring up one at a time, assign unique address
  for (int i = 0; i < NUM_SENSORS; i++) {
    mcp.digitalWrite(i, HIGH);  // wake this sensor
    delay(10);  // boot time

    uint8_t newAddr = VL53L0X_BASE_ADDR + i;

    if (tof[i].begin(newAddr)) {
      sensorOK[i] = true;
      Serial.printf("  [OK] %s (GPA%d) → addr 0x%02X\n", sensorNames[i], i, newAddr);
    } else {
      sensorOK[i] = false;
      Serial.printf("  [FAIL] %s (GPA%d) — not responding\n", sensorNames[i], i);
    }
  }

  // Summary
  int okCount = 0;
  for (int i = 0; i < NUM_SENSORS; i++) if (sensorOK[i]) okCount++;
  Serial.printf("[RESULT] %d/%d sensors initialized\n", okCount, NUM_SENSORS);
}

// ==================== SENSOR READING ====================
void readAllSensors() {
  Serial.println("\n--- ToF Sensor Readings ---");
  bool anyOK = false;
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (!sensorOK[i]) {
      Serial.printf("  %-14s: OFFLINE\n", sensorNames[i]);
      continue;
    }
    anyOK = true;
    VL53L0X_RangingMeasurementData_t measure;
    tof[i].rangingTest(&measure, false);

    if (measure.RangeStatus != 4) {
      Serial.printf("  %-14s: %4d mm\n", sensorNames[i], measure.RangeMilliMeter);
    } else {
      Serial.printf("  %-14s: OUT OF RANGE\n", sensorNames[i]);
    }
  }
  if (!anyOK) Serial.println("  No sensors available. Run 'init' first.");
}

void continuousRead(int durationMs) {
  Serial.printf("\n--- Continuous Read (%d seconds) ---\n", durationMs / 1000);
  Serial.println("(Ctrl+C or wait to stop)\n");

  unsigned long start = millis();
  while (millis() - start < (unsigned long)durationMs) {
    // Print header
    Serial.print("  ");
    for (int i = 0; i < NUM_SENSORS; i++) {
      if (sensorOK[i]) Serial.printf("%-10s", sensorNames[i]);
    }
    Serial.println();
    Serial.print("  ");

    for (int i = 0; i < NUM_SENSORS; i++) {
      if (!sensorOK[i]) continue;
      VL53L0X_RangingMeasurementData_t measure;
      tof[i].rangingTest(&measure, false);
      if (measure.RangeStatus != 4) {
        Serial.printf("%-10d", measure.RangeMilliMeter);
      } else {
        Serial.printf("%-10s", "---");
      }
    }
    Serial.println(" mm");
    delay(200);
  }
  Serial.println("[DONE] Continuous read finished");
}

// ==================== MOTOR TESTS ====================
void testSingleMotor(int idx) {
  if (idx < 0 || idx >= NUM_MOTORS) return;

  Serial.printf("\n--- Testing %s (GPIO %d/%d) ---\n",
    motors[idx].name, motors[idx].pin1, motors[idx].pin2);

  Serial.printf("  FORWARD (pin1=%d HIGH, pin2=%d LOW) for 1.5s...\n",
    motors[idx].pin1, motors[idx].pin2);
  driveMotor(motors[idx].pin1, motors[idx].pin2, TEST_SPEED);
  delay(1500);
  stopAll();
  delay(500);

  Serial.printf("  BACKWARD (pin1=%d LOW, pin2=%d HIGH) for 1.5s...\n",
    motors[idx].pin1, motors[idx].pin2);
  driveMotor(motors[idx].pin1, motors[idx].pin2, -TEST_SPEED);
  delay(1500);
  stopAll();

  Serial.printf("[OK] %s test done — verify direction visually!\n", motors[idx].name);
}

void testAllMotors() {
  Serial.println("\n========== MOTOR TEST SEQUENCE ==========");
  Serial.println("Watch each wheel and verify direction.");
  Serial.println("Each motor runs 1.5s forward, 1.5s backward.\n");

  for (int i = 0; i < NUM_MOTORS; i++) {
    testSingleMotor(i);
    Serial.println();
    delay(500);
  }

  Serial.println("========== DIRECTION VERIFICATION ==========\n");

  Serial.println("--- FORWARD: all wheels should spin FORWARD ---");
  for (int i = 0; i < NUM_MOTORS; i++)
    driveMotor(motors[i].pin1, motors[i].pin2, TEST_SPEED);
  delay(2000);
  stopAll();
  delay(500);

  Serial.println("--- BACKWARD: all wheels should spin BACKWARD ---");
  for (int i = 0; i < NUM_MOTORS; i++)
    driveMotor(motors[i].pin1, motors[i].pin2, -TEST_SPEED);
  delay(2000);
  stopAll();
  delay(500);

  Serial.println("--- LEFT SPIN: left wheels forward, right wheels backward ---");
  driveMotor(FL1, FL2, TEST_SPEED);
  driveMotor(FR1, FR2, -TEST_SPEED);
  driveMotor(RL1, RL2, TEST_SPEED);
  driveMotor(RR1, RR2, -TEST_SPEED);
  delay(2000);
  stopAll();
  delay(500);

  Serial.println("--- RIGHT SPIN: left wheels backward, right wheels forward ---");
  driveMotor(FL1, FL2, -TEST_SPEED);
  driveMotor(FR1, FR2, TEST_SPEED);
  driveMotor(RL1, RL2, -TEST_SPEED);
  driveMotor(RR1, RR2, TEST_SPEED);
  delay(2000);
  stopAll();

  Serial.println("\n[DONE] Motor test complete!");
}

// ==================== FULL SYSTEM TEST ====================
void runFullTest() {
  Serial.println("\n╔══════════════════════════════════════╗");
  Serial.println("║    WOFFY 2.0 — FULL HARDWARE TEST    ║");
  Serial.println("╚══════════════════════════════════════╝\n");

  // Phase 1: I2C
  Serial.println("══ PHASE 1: I2C Bus ══");
  i2cScan();

  // Phase 2: MCP23017
  Serial.println("\n══ PHASE 2: MCP23017 GPIO Expander ══");
  mcpOK = setupMCP();
  if (mcpOK) testMCPpins();

  // Phase 3: ToF Sensors
  Serial.println("\n══ PHASE 3: VL53L0X ToF Sensors ══");
  setupSensors();
  if (mcpOK) readAllSensors();

  // Phase 4: Motors
  Serial.println("\n══ PHASE 4: Motors ══");
  Serial.println("Starting motor tests in 3 seconds...");
  Serial.println(">> LIFT THE ROBOT OFF THE GROUND! <<");
  delay(3000);
  testAllMotors();

  // Summary
  Serial.println("\n╔══════════════════════════════════════╗");
  Serial.println("║           TEST SUMMARY               ║");
  Serial.println("╠══════════════════════════════════════╣");
  Serial.printf("║ MCP23017:  %s                    ║\n", mcpOK ? "OK  " : "FAIL");
  int sensorsUp = 0;
  for (int i = 0; i < NUM_SENSORS; i++) if (sensorOK[i]) sensorsUp++;
  Serial.printf("║ ToF Sensors: %d/8                     ║\n", sensorsUp);
  Serial.println("║ Motors: CHECK SERIAL OUTPUT ABOVE    ║");
  Serial.println("╚══════════════════════════════════════╝");
  Serial.println("\nType 'help' for interactive commands.");
}

// ==================== MENU ====================
void printHelp() {
  Serial.println("\n--- Woffy Hardware Test Commands ---");
  Serial.println("  full      — Run complete test sequence");
  Serial.println("  scan      — I2C bus scan");
  Serial.println("  mcp       — MCP23017 init + pin toggle test");
  Serial.println("  init      — Initialize all VL53L0X sensors");
  Serial.println("  read      — Read all ToF sensors once");
  Serial.println("  live      — Continuous sensor read (10s)");
  Serial.println("  motors    — Test all 4 motors");
  Serial.println("  m:0       — Test single motor (0=FL 1=FR 2=RL 3=RR)");
  Serial.println("  fwd       — All motors forward 2s");
  Serial.println("  bwd       — All motors backward 2s");
  Serial.println("  left      — Spin left 2s");
  Serial.println("  right     — Spin right 2s");
  Serial.println("  speed     — Speed ramp test (60→255) all motors");
  Serial.println("  stop      — Emergency stop");
  Serial.println("  help      — Show this menu");
}

void handleCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();

  if (cmd == "full") {
    runFullTest();
  }
  else if (cmd == "scan") {
    i2cScan();
  }
  else if (cmd == "mcp") {
    mcpOK = setupMCP();
    if (mcpOK) testMCPpins();
  }
  else if (cmd == "init") {
    if (!mcpOK) mcpOK = setupMCP();
    setupSensors();
  }
  else if (cmd == "read") {
    readAllSensors();
  }
  else if (cmd == "live") {
    continuousRead(10000);
  }
  else if (cmd == "motors") {
    testAllMotors();
  }
  else if (cmd.startsWith("m:")) {
    int idx = cmd.substring(2).toInt();
    testSingleMotor(idx);
  }
  else if (cmd == "fwd") {
    Serial.println("All motors FORWARD 2s...");
    for (int i = 0; i < NUM_MOTORS; i++)
      driveMotor(motors[i].pin1, motors[i].pin2, TEST_SPEED);
    delay(2000);
    stopAll();
    Serial.println("[DONE]");
  }
  else if (cmd == "bwd") {
    Serial.println("All motors BACKWARD 2s...");
    for (int i = 0; i < NUM_MOTORS; i++)
      driveMotor(motors[i].pin1, motors[i].pin2, -TEST_SPEED);
    delay(2000);
    stopAll();
    Serial.println("[DONE]");
  }
  else if (cmd == "left") {
    Serial.println("SPIN LEFT 2s...");
    driveMotor(FL1, FL2, TEST_SPEED);
    driveMotor(FR1, FR2, -TEST_SPEED);
    driveMotor(RL1, RL2, TEST_SPEED);
    driveMotor(RR1, RR2, -TEST_SPEED);
    delay(2000);
    stopAll();
    Serial.println("[DONE]");
  }
  else if (cmd == "right") {
    Serial.println("SPIN RIGHT 2s...");
    driveMotor(FL1, FL2, -TEST_SPEED);
    driveMotor(FR1, FR2, TEST_SPEED);
    driveMotor(RL1, RL2, -TEST_SPEED);
    driveMotor(RR1, RR2, TEST_SPEED);
    delay(2000);
    stopAll();
    Serial.println("[DONE]");
  }
  else if (cmd == "speed") {
    Serial.println("\n========== MOTOR SPEED TEST ==========");
    Serial.println("Ramping all motors: 60 → 255 (step 15)\n");

    // Forward ramp up
    for (int spd = 60; spd <= 255; spd += 15) {
      if (spd > 255) spd = 255;
      Serial.printf("  FWD  PWM %3d  (%2d%%)...\n", spd, spd * 100 / 255);
      for (int i = 0; i < NUM_MOTORS; i++)
        driveMotor(motors[i].pin1, motors[i].pin2, spd);
      delay(1500);
    }
    stopAll();
    delay(500);

    // Backward ramp up
    Serial.println();
    for (int spd = 60; spd <= 255; spd += 15) {
      if (spd > 255) spd = 255;
      Serial.printf("  BWD  PWM %3d  (%2d%%)...\n", spd, spd * 100 / 255);
      for (int i = 0; i < NUM_MOTORS; i++)
        driveMotor(motors[i].pin1, motors[i].pin2, -spd);
      delay(1500);
    }
    stopAll();

    Serial.println("\n[DONE] Speed test complete!");
    Serial.println("Check: all 4 wheels should have sped up evenly.");
    Serial.println("If any wheel is slower/stalls at low PWM, note which one.\n");
  }
  else if (cmd == "stop") {
    stopAll();
    Serial.println("[STOPPED]");
  }
  else if (cmd == "help") {
    printHelp();
  }
  else {
    Serial.printf("Unknown: '%s' — type 'help'\n", cmd.c_str());
  }
}

// ==================== SETUP & LOOP ====================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n\n");
  Serial.println("╔══════════════════════════════════════╗");
  Serial.println("║  WOFFY 2.0 — HARDWARE TEST BENCH    ║");
  Serial.println("║  Motors + MCP23017 + 8x VL53L0X     ║");
  Serial.println("╚══════════════════════════════════════╝\n");

  // Init I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);  // 400kHz fast mode
  Serial.println("[OK] I2C started (SDA=21, SCL=22, 400kHz)");

  // Init motor PWM
  setupMotorPins();
  stopAll();

  // Auto-run full test
  runFullTest();
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.replace("\r", "");  // handle screen/terminals that send \r
    if (cmd.length() > 0) handleCommand(cmd);
  }
}
