/*
 * Woffy 2.0 — ToF Sensor Array (8x VL53L0X via MCP23017)
 * Instant 360° obstacle detection, no moving parts
 */

#ifndef SENSORS_H
#define SENSORS_H

#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_VL53L0X.h>

// ===== I2C =====
#define I2C_SDA 21
#define I2C_SCL 22
#define MCP23017_ADDR 0x20
#define VL53L0X_BASE_ADDR 0x30

// ===== Sensor indices =====
#define S_CLIFF_F    0   // GPA0 — front cliff (downward 45°)
#define S_CLIFF_B    1   // GPA1 — back cliff (downward 45°)
#define S_FRONT_R    2   // GPA2 — front right (horizontal)
#define S_FRONT_C    3   // GPA3 — front center (horizontal)
#define S_FRONT_L    4   // GPA4 — front left (horizontal)
#define S_BACK_C     5   // GPA5 — back center (horizontal)
#define S_LEFT       6   // GPA6 — left (horizontal)
#define S_RIGHT      7   // GPA7 — right (horizontal)
#define NUM_SENSORS  8

const char* sensorNames[] = {
  "CF", "CB", "FR", "FC", "FL", "BC", "L", "R"
};

// ===== Sensor data =====
Adafruit_MCP23X17 mcp;
Adafruit_VL53L0X tof[NUM_SENSORS];
bool sensorOK[NUM_SENSORS] = {false};
int sensorRaw[NUM_SENSORS] = {0};    // raw readings in mm
float sensorEMA[NUM_SENSORS] = {0};  // EMA-filtered readings
int sensorMM[NUM_SENSORS] = {0};     // final values (int cast of EMA)
bool mcpReady = false;
int sensorsActive = 0;

// Max reliable range — beyond this we treat as "clear"
#define TOF_MAX_MM 1200

// EMA filter: alpha=0.3 → 70% old + 30% new. Smooths noise without lag.
#define EMA_ALPHA 0.3f

// ===== Init =====
inline bool setupSensors() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  Serial.println("I2C: SDA=21, SCL=22, 400kHz");

  // Init MCP23017
  if (!mcp.begin_I2C(MCP23017_ADDR)) {
    Serial.println("[FAIL] MCP23017 not found!");
    return false;
  }
  mcpReady = true;
  Serial.println("[OK] MCP23017 at 0x20");

  // All XSHUT LOW (sensors off)
  for (int i = 0; i < NUM_SENSORS; i++) {
    mcp.pinMode(i, OUTPUT);
    mcp.digitalWrite(i, LOW);
  }
  delay(10);

  // Wake sensors one by one, assign unique addresses
  sensorsActive = 0;
  for (int i = 0; i < NUM_SENSORS; i++) {
    mcp.digitalWrite(i, HIGH);
    delay(10);

    uint8_t addr = VL53L0X_BASE_ADDR + i;
    if (tof[i].begin(addr)) {
      sensorOK[i] = true;
      sensorsActive++;
      // Shorter range, faster reads (~20ms per sensor → ~33ms timing budget)
      tof[i].configSensor(Adafruit_VL53L0X::VL53L0X_SENSE_HIGH_SPEED);
      Serial.printf("[OK] %s (GPA%d) → 0x%02X\n", sensorNames[i], i, addr);
    } else {
      sensorOK[i] = false;
      Serial.printf("[FAIL] %s (GPA%d)\n", sensorNames[i], i);
    }
  }

  Serial.printf("Sensors: %d/%d active\n", sensorsActive, NUM_SENSORS);
  return sensorsActive > 0;
}

// ===== Read all sensors with EMA filtering =====
// ~40ms for 8 sensors in high-speed mode
inline void readSensors() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (!sensorOK[i]) {
      sensorMM[i] = 0;  // 0 = offline
      continue;
    }
    VL53L0X_RangingMeasurementData_t measure;
    tof[i].rangingTest(&measure, false);

    int raw;
    if (measure.RangeStatus != 4 && measure.RangeMilliMeter > 0 && measure.RangeMilliMeter < TOF_MAX_MM) {
      raw = measure.RangeMilliMeter;
    } else {
      raw = TOF_MAX_MM;
    }
    sensorRaw[i] = raw;

    // EMA filter: smooths noise, fast response to real changes
    if (sensorEMA[i] == 0) {
      sensorEMA[i] = raw;  // first reading — no history
    } else {
      sensorEMA[i] = EMA_ALPHA * raw + (1.0f - EMA_ALPHA) * sensorEMA[i];
    }
    sensorMM[i] = (int)sensorEMA[i];
  }
}

// ===== Convenience getters (mm) =====
// Obstacle sensors (horizontal) — default to TOF_MAX_MM if offline (assume clear)
inline int frontCenter() { return sensorOK[S_FRONT_C] ? sensorMM[S_FRONT_C] : TOF_MAX_MM; }
inline int frontLeft()   { return sensorOK[S_FRONT_L] ? sensorMM[S_FRONT_L] : TOF_MAX_MM; }
inline int frontRight()  { return sensorOK[S_FRONT_R] ? sensorMM[S_FRONT_R] : TOF_MAX_MM; }
inline int sideLeft()    { return sensorOK[S_LEFT] ? sensorMM[S_LEFT] : TOF_MAX_MM; }
inline int sideRight()   { return sensorOK[S_RIGHT] ? sensorMM[S_RIGHT] : TOF_MAX_MM; }
inline int backCenter()  { return sensorOK[S_BACK_C] ? sensorMM[S_BACK_C] : TOF_MAX_MM; }

// Cliff sensors (downward 45°) — default to 0 if offline (assume floor present = safe)
inline int cliffFront()  { return sensorOK[S_CLIFF_F] ? sensorMM[S_CLIFF_F] : 0; }
inline int cliffBack()   { return sensorOK[S_CLIFF_B] ? sensorMM[S_CLIFF_B] : 0; }

// Minimum of front 3 horizontal sensors
inline int frontMin() {
  int fc = frontCenter(), fl = frontLeft(), fr = frontRight();
  int m = fc;
  if (fl < m) m = fl;
  if (fr < m) m = fr;
  return m;
}

// ===== Cliff detection =====
// Floor distance depends on sensor mount height. Calibrate on startup.
// If cliff sensor reads > threshold → no floor → DANGER
int cliffFloorMM = 0;           // calibrated floor distance (set in setup)
#define CLIFF_MARGIN_MM  80     // floor + this margin = cliff threshold

inline void calibrateCliff() {
  // Read floor distance at startup (robot must be on flat ground)
  readSensors();
  int cf = sensorMM[S_CLIFF_F];
  int cb = sensorMM[S_CLIFF_B];
  if (cf > 0 && cf < 200) {
    cliffFloorMM = cf;
  } else if (cb > 0 && cb < 200) {
    cliffFloorMM = cb;
  } else {
    cliffFloorMM = 60;  // default ~6cm if sensors offline
  }
  Serial.printf("Cliff calibrated: floor=%dmm, threshold=%dmm\n",
                cliffFloorMM, cliffFloorMM + CLIFF_MARGIN_MM);
}

inline bool isCliffFront() {
  int cf = cliffFront();
  return (cf > 0 && cf > cliffFloorMM + CLIFF_MARGIN_MM);
}

inline bool isCliffBack() {
  int cb = cliffBack();
  return (cb > 0 && cb > cliffFloorMM + CLIFF_MARGIN_MM);
}

// ===== Debug print =====
inline void printSensors() {
  Serial.print("ToF:");
  for (int i = 0; i < NUM_SENSORS; i++) {
    Serial.printf(" %s=%d", sensorNames[i], sensorMM[i]);
  }
  Serial.println();
}

// ===== MQTT-friendly compact string =====
inline String sensorString() {
  String s = "TOF:";
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (i > 0) s += ",";
    s += String(sensorMM[i]);
  }
  return s;
}

#endif
