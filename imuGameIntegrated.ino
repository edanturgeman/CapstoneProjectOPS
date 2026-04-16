#include <Wire.h>
#include <SPI.h>
#include <RF24.h>
#include <math.h>

// -------------------- nRF24 --------------------
RF24 radio(9, 10);   // CE, CSN
const byte address[6] = "CTRL1";

// -------------------- MPU6050 --------------------
#define MPU6050_ADDR 0x68
#define GYRO_XOUT_H  0x43

long gyroZ_offset = 0;
const float gyroScale = 131.0;

// -------------------- Debug / display angle --------------------
float currentAngle = 0.0;
float smoothedGz = 0.0;

unsigned long lastTime = 0;
unsigned long packetCounter = 0;

// -------------------- Tuning --------------------
#define SMOOTHING_ALPHA           0.28f
#define TURN_RATE_THRESHOLD       32.0f

// FIX 1: Increased from 12.0f to 25.0f to overcome IMU drift/noise
#define REARM_RATE_THRESHOLD      25.0f   

// FIX 2: Decreased from 120 to 80 so the game lets you turn again faster
#define CENTER_HOLD_MS           80      
#define TX_LOOP_DELAY_MS          12
#define DISPLAY_DEADZONE           1.2f

#define DISPLAY_RETURN_RATE       75.0f
#define MAX_DEBUG_ANGLE          45.0f

bool readyForNextTurn = true;
char lastSentCommand = 'C';
unsigned long stableCenterStart = 0;

// -------------------- Packet --------------------
struct ControlPacket {
  char command; // 'L', 'C', or 'R'
  float angle;
  unsigned long counter;
};

ControlPacket packet;

// -------------------- Functions --------------------
void wakeMPU() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
}

bool readRawGyroZ(int16_t &rawGyroZ) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(GYRO_XOUT_H + 4); // Z axis high byte
  if (Wire.endTransmission(false) != 0) return false;

  Wire.requestFrom(MPU6050_ADDR, 2, true);
  if (Wire.available() < 2) return false;

  rawGyroZ = (Wire.read() << 8) | Wire.read();
  return true;
}

void calibrateGyroscope() {
  long sumZ = 0;
  const int samples = 500;

  Serial.println("Calibrating... keep wheel perfectly still.");
  for (int i = 0; i < samples; i++) {
    int16_t rawGyroZ = 0;
    if (readRawGyroZ(rawGyroZ)) {
      sumZ += rawGyroZ;
    }
    delay(4);
  }

  gyroZ_offset = sumZ / samples;

  Serial.print("gyroZ offset = ");
  Serial.println(gyroZ_offset);
  Serial.println("Calibration done.");
}

bool readGyroZ(float &gz) {
  int16_t rawGyroZ = 0;
  if (!readRawGyroZ(rawGyroZ)) return false;

  rawGyroZ -= gyroZ_offset;
  gz = rawGyroZ / gyroScale;
  return true;
}

void sendCommand(char cmd) {
  packet.command = cmd;
  packet.angle = currentAngle;
  packet.counter = packetCounter++;

  bool ok = radio.write(&packet, sizeof(packet));

  Serial.print("SEND ");
  Serial.print(cmd);
  Serial.print("  gz=");
  Serial.print(smoothedGz, 2);
  Serial.print("  angle=");
  Serial.print(currentAngle, 1);
  Serial.print("  ready=");
  Serial.print(readyForNextTurn ? "1" : "0");
  Serial.print("  ok=");
  Serial.println(ok ? "1" : "0");
  
  lastSentCommand = cmd;
}

void updateDebugAngle(float dt) {
  if (fabs(smoothedGz) > DISPLAY_DEADZONE) {
    currentAngle += smoothedGz * dt;
  } else {
    if (currentAngle > 0.0f) {
      currentAngle -= DISPLAY_RETURN_RATE * dt;
      if (currentAngle < 0.0f) currentAngle = 0.0f;
    } else if (currentAngle < 0.0f) {
      currentAngle += DISPLAY_RETURN_RATE * dt;
      if (currentAngle > 0.0f) currentAngle = 0.0f;
    }
  }

  if (currentAngle > MAX_DEBUG_ANGLE) currentAngle = MAX_DEBUG_ANGLE;
  if (currentAngle < -MAX_DEBUG_ANGLE) currentAngle = -MAX_DEBUG_ANGLE;
}

// -------------------- Setup --------------------
void setup() {
  Serial.begin(9600);
  Wire.begin();

  wakeMPU();
  delay(1200);
  calibrateGyroscope();
  
  if (!radio.begin()) {
    Serial.println("nRF24 not found on Nano");
    while (true) {}
  }

  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.setChannel(108);
  radio.openWritingPipe(address);
  radio.stopListening();

  Serial.println("Nano IMU transmitter ready");

  lastTime = millis();
  stableCenterStart = millis();
}

// -------------------- Loop --------------------
void loop() {
  float gz = 0.0f;

  if (!readGyroZ(gz)) {
    Serial.println("MPU read failed");
    delay(20);
    return;
  }

  unsigned long now = millis();
  float dt = (now - lastTime) / 1000.0f;
  if (dt <= 0.0f) dt = 0.001f;
  lastTime = now;
  
  smoothedGz = (1.0f - SMOOTHING_ALPHA) * smoothedGz + SMOOTHING_ALPHA * gz;

  updateDebugAngle(dt);
  
  if (fabs(smoothedGz) < REARM_RATE_THRESHOLD) {
    if (stableCenterStart == 0) {
      stableCenterStart = now;
    }
  } else {
    stableCenterStart = 0;
  }

  if (!readyForNextTurn && stableCenterStart != 0 && (now - stableCenterStart >= CENTER_HOLD_MS)) {
    readyForNextTurn = true;
    currentAngle = 0.0f;

    if (lastSentCommand != 'C') {
      sendCommand('C');
    }
  }

  // LEFT / RIGHT
  if (readyForNextTurn) {
    if (smoothedGz <= -TURN_RATE_THRESHOLD) {
      sendCommand('R');
      readyForNextTurn = false;
      stableCenterStart = 0;
    }
    else if (smoothedGz >= TURN_RATE_THRESHOLD) {
      sendCommand('L');
      readyForNextTurn = false;
      stableCenterStart = 0;
    }
  }

  Serial.print("rawGz=");
  Serial.print(gz, 2);
  Serial.print("  smoothed=");
  Serial.print(smoothedGz, 2);
  Serial.print("  dbgAngle=");
  Serial.print(currentAngle, 1);
  Serial.print("  ready=");
  Serial.print(readyForNextTurn ? "1" : "0");
  Serial.print("  last=");
  Serial.println(lastSentCommand);

  delay(TX_LOOP_DELAY_MS);
}