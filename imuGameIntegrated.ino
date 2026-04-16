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

// -------------------- Angle tracking --------------------
float currentAngle = 0.0;
float smoothedGz = 0.0;
unsigned long lastTime = 0;
unsigned long packetCounter = 0;

// -------------------- Tuning --------------------
#define VELOCITY_DEADZONE       0.8
#define TURN_THRESHOLD         12.0
#define REARM_CENTER_BAND       5.0
#define RETURN_TO_CENTER_RATE   6.0
#define MAX_ANGLE              45.0
#define SMOOTHING_ALPHA         0.35

// true = ready to send next L/R event
bool readyForNextTurn = true;
char lastSentCommand = 'C';

// -------------------- Packet --------------------
struct ControlPacket {
  char command;              // 'L', 'C', or 'R'
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

void calibrateGyroscope() {
  long sumZ = 0;

  Serial.println("Calibrating... keep wheel perfectly still.");
  for (int i = 0; i < 300; i++) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(GYRO_XOUT_H + 4);   // Z axis
    Wire.endTransmission(false);
    Wire.requestFrom(MPU6050_ADDR, 2, true);

    int16_t rawGyroZ = (Wire.read() << 8) | Wire.read();
    sumZ += rawGyroZ;
    delay(5);
  }

  gyroZ_offset = sumZ / 300;

  Serial.print("gyroZ offset = ");
  Serial.println(gyroZ_offset);
  Serial.println("Calibration done.");
}

bool readGyroZ(float &gz) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(GYRO_XOUT_H + 4);
  if (Wire.endTransmission(false) != 0) return false;

  Wire.requestFrom(MPU6050_ADDR, 2, true);
  if (Wire.available() < 2) return false;

  int16_t rawGyroZ = (Wire.read() << 8) | Wire.read();
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
  Serial.print(" angle=");
  Serial.print(currentAngle, 1);
  Serial.print(" ok=");
  Serial.println(ok ? "1" : "0");

  lastSentCommand = cmd;
}

// -------------------- Setup --------------------
void setup() {
  Serial.begin(9600);
  Wire.begin();

  wakeMPU();
  delay(1000);
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
  Serial.println("L/R moves one lane, C means stay in current lane.");
  lastTime = millis();
}

// -------------------- Loop --------------------
void loop() {
  float gz = 0.0;

  if (!readGyroZ(gz)) {
    Serial.println("MPU read failed");
    delay(20);
    return;
  }

  smoothedGz = (1.0 - SMOOTHING_ALPHA) * smoothedGz + SMOOTHING_ALPHA * gz;

  unsigned long currentTime = millis();
  float dt = (currentTime - lastTime) / 1000.0;
  lastTime = currentTime;

  if (abs(smoothedGz) > VELOCITY_DEADZONE) {
    currentAngle -= smoothedGz * dt;
    // If left/right is backwards, change to:
    // currentAngle += smoothedGz * dt;
  } else {
    if (currentAngle > 0.0) {
      currentAngle -= RETURN_TO_CENTER_RATE * dt;
      if (currentAngle < 0.0) currentAngle = 0.0;
    } else if (currentAngle < 0.0) {
      currentAngle += RETURN_TO_CENTER_RATE * dt;
      if (currentAngle > 0.0) currentAngle = 0.0;
    }
  }

  if (currentAngle > MAX_ANGLE) currentAngle = MAX_ANGLE;
  if (currentAngle < -MAX_ANGLE) currentAngle = -MAX_ANGLE;

  // If wheel is back near center, re-arm and send C once
  if (!readyForNextTurn && abs(currentAngle) < REARM_CENTER_BAND) {
    readyForNextTurn = true;
    if (lastSentCommand != 'C') {
      sendCommand('C');
    }
  }

  // Send one L or R event when crossing threshold
  if (readyForNextTurn) {
    if (currentAngle <= -TURN_THRESHOLD) {
      sendCommand('L');
      readyForNextTurn = false;
    } else if (currentAngle >= TURN_THRESHOLD) {
      sendCommand('R');
      readyForNextTurn = false;
    }
  }

  Serial.print("gz=");
  Serial.print(gz, 2);
  Serial.print(" smoothed=");
  Serial.print(smoothedGz, 2);
  Serial.print(" angle=");
  Serial.print(currentAngle, 1);
  Serial.print(" ready=");
  Serial.print(readyForNextTurn ? "1" : "0");
  Serial.print(" last=");
  Serial.println(lastSentCommand);

  delay(20);
}