#include <Wire.h>

// MPU6050 Registers
#define MPU6050_ADDR   0x68
#define ACCEL_XOUT_H   0x3B
#define GYRO_XOUT_H    0x43

// Calibration Offsets
long gyroX_offset = 0, gyroY_offset = 0, gyroZ_offset = 0;

// Scales
float gyroScale = 131.0;    // For ±250 °/s

// Deadzone threshold in degrees/sec
#define DEADZONE_THRESHOLD 10.0

void setup() {
  Serial.begin(9600);
  Wire.begin();

  // Wake up MPU6050 - write 0 to power management register
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission();

  delay(1000);

  calibrateGyroscope();

  Serial.println("\nStarting Final Reading...\n");
}

void loop() {
  int16_t rawGyroX, rawGyroY, rawGyroZ;

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
  float gz = rawGyroZ / gyroScale;

  // -------- Apply Deadzone & Map to Discrete Command --------
  char command = getDirectionCommand(gz);

  Serial.println(command);

  delay(200);
}

// Returns 'L', 'R', or 'C' based on gz with deadzone applied
char getDirectionCommand(float gz) {
  if (gz > DEADZONE_THRESHOLD) {
    return 'R';
  } else if (gz < -DEADZONE_THRESHOLD) {
    return 'L';
  } else {
    return 'C';
  }
}

// ---------------- Calibration Function ---------------- //

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