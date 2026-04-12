#include <Wire.h>

// MPU6050 Registers
#define MPU6050_ADDR   0x68
#define ACCEL_XOUT_H   0x3B
#define GYRO_XOUT_H    0x43

// Calibration Offsets
long accX_offset = 0, accY_offset = 0, accZ_offset = 0;
long gyroX_offset = 0, gyroY_offset = 0, gyroZ_offset = 0;

// Scales
float accScale = 16384.0;   // For ±2g
float gyroScale = 131.0;    // For ±250 °/s

void setup() {
  Serial.begin(9600);
  Wire.begin();

  // Wake up MPU6050 - write 0 to power management register
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x6B);
  Wire.write(0); 
  Wire.endTransmission();

  delay(1000);

  calibrateAccelerometer();
  calibrateGyroscope();

  Serial.println("\nStarting Final Reading...\n");
}

void loop() {
  int16_t rawAccX, rawAccY, rawAccZ;
  int16_t rawGyroX, rawGyroY, rawGyroZ;

  // ------ Read Accelerometer ------
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6050_ADDR, 6, true);

  rawAccX = (Wire.read() << 8) | Wire.read();
  rawAccY = (Wire.read() << 8) | Wire.read();
  rawAccZ = (Wire.read() << 8) | Wire.read();

  // Apply offsets
  rawAccX -= accX_offset;
  rawAccY -= accY_offset;
  rawAccZ -= accZ_offset;

  // Convert to g
  float ax = rawAccX / accScale;
  float ay = rawAccY / accScale;
  float az = rawAccZ / accScale;

  // ------ Read Gyroscope ------
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(GYRO_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6050_ADDR, 6, true);

  rawGyroX = (Wire.read() << 8) | Wire.read();
  rawGyroY = (Wire.read() << 8) | Wire.read();
  rawGyroZ = (Wire.read() << 8) | Wire.read();

  // Apply offsets
  rawGyroX -= gyroX_offset;
  rawGyroY -= gyroY_offset;
  rawGyroZ -= gyroZ_offset;

  // Convert to degrees/sec
  float gx = rawGyroX / gyroScale;
  float gy = rawGyroY / gyroScale;
  float gz = rawGyroZ / gyroScale;

  // -------- Print Results --------
  Serial.print("Acc(g): ");
  Serial.print(ax); Serial.print(", ");
  Serial.print(ay); Serial.print(", ");
  Serial.println(az);

  Serial.print("Gyro(dps): ");
  Serial.print(gx); Serial.print(", ");
  Serial.print(gy); Serial.print(", ");
  Serial.println(gz);

  Serial.println("---------------------");
  delay(200);
}

// ---------------- Calibration Functions ---------------- //

void calibrateAccelerometer() {
  long sumX = 0, sumY = 0, sumZ = 0;

  Serial.println("Calibrating Accelerometer... Do not move!");

  for (int i = 0; i < 200; i++) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(ACCEL_XOUT_H);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU6050_ADDR, 6, true);

    sumX += (Wire.read() << 8) | Wire.read();
    sumY += (Wire.read() << 8) | Wire.read();
    sumZ += (Wire.read() << 8) | Wire.read();

    delay(5);
  }

  accX_offset = sumX / 200;
  accY_offset = sumY / 200;
  accZ_offset = (sumZ / 200) - 16384; // 1g adjustment

  Serial.println("Accelerometer Calibration Done.");
}

void calibrateGyroscope() {
  long sumX = 0, sumY = 0, sumZ = 0;

  Serial.println("Calibrating Gyroscope... Do not move!");

  for (int i = 0; i < 200; i++) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(GYRO_XOUT_H);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU6050_ADDR, 6, true);

    sumX += (Wire.read() << 8) | Wire.read();
    sumY += (Wire.read() << 8) | Wire.read();
    sumZ += (Wire.read() << 8) | Wire.read();

    delay(5);
  }

  gyroX_offset = sumX / 200;
  gyroY_offset = sumY / 200;
  gyroZ_offset = sumZ / 200;

  Serial.println("Gyroscope Calibration Done.");
}