/*
  Mobility Lab — ESP32 + MPU6050 BLE firmware
  Arduino IDE, ESP32 board package.
  BLE protocol: Nordic UART Service (NUS), newline-terminated JSON:
  {"ax":123,"ay":-456,"az":16384,"temp":24.7}

  Wiring (common ESP32 DevKit):
    MPU6050 VCC  -> 3V3
    MPU6050 GND  -> GND
    MPU6050 SDA  -> GPIO 21
    MPU6050 SCL  -> GPIO 22
*/

#include <Arduino.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#ifndef I2C_SDA
#define I2C_SDA 21
#endif
#ifndef I2C_SCL
#define I2C_SCL 22
#endif

static constexpr uint8_t MPU_ADDR = 0x68;
static constexpr uint8_t REG_PWR_MGMT_1 = 0x6B;
static constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
static constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
static constexpr uint8_t REG_WHO_AM_I = 0x75;

static constexpr char DEVICE_NAME[] = "MobilityLab";
static constexpr char SERVICE_UUID[] = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static constexpr char RX_UUID[]      = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
static constexpr char TX_UUID[]      = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

BLECharacteristic* txCharacteristic = nullptr;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool sensorReady = false;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    deviceConnected = true;
  }
  void onDisconnect(BLEServer* server) override {
    deviceConnected = false;
  }
};

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readRegisters(uint8_t startReg, uint8_t* buffer, size_t length) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(startReg);
  if (Wire.endTransmission(false) != 0) return false;

  size_t received = Wire.requestFrom((int)MPU_ADDR, (int)length, (int)true);
  if (received != length) return false;

  for (size_t i = 0; i < length; i++) buffer[i] = Wire.read();
  return true;
}

bool initMPU6050() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_WHO_AM_I);
  if (Wire.endTransmission(false) != 0) return false;

  if (Wire.requestFrom((int)MPU_ADDR, 1, (int)true) != 1) return false;
  uint8_t identity = Wire.read();
  if (identity != 0x68) {
    Serial.printf("Unexpected MPU6050 WHO_AM_I: 0x%02X\n", identity);
    return false;
  }

  // Wake sensor and select the default +/-2g accelerometer range.
  if (!writeRegister(REG_PWR_MGMT_1, 0x00)) return false;
  delay(100);
  if (!writeRegister(REG_ACCEL_CONFIG, 0x00)) return false;
  return true;
}

void setupBLE() {
  BLEDevice::init(DEVICE_NAME);
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService* service = server->createService(SERVICE_UUID);

  txCharacteristic = service->createCharacteristic(
    TX_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  txCharacteristic->addDescriptor(new BLE2902());

  // RX is included for NUS compatibility; this firmware does not need commands.
  BLECharacteristic* rx = service->createCharacteristic(
    RX_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  (void)rx;

  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->start();

  Serial.println("BLE advertising as MobilityLab");
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  sensorReady = initMPU6050();
  if (sensorReady) {
    Serial.println("MPU6050 detected.");
  } else {
    Serial.println("ERROR: MPU6050 not detected. Check 3V3/GND/SDA/SCL and address 0x68.");
  }

  setupBLE();
}

void loop() {
  // Restart advertising after a client disconnects.
  if (!deviceConnected && oldDeviceConnected) {
    delay(300);
    BLEDevice::startAdvertising();
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  static uint32_t lastSample = 0;
  if (millis() - lastSample < 100) return; // 10 Hz
  lastSample = millis();

  if (!sensorReady || !deviceConnected || txCharacteristic == nullptr) return;

  uint8_t data[14];
  if (!readRegisters(REG_ACCEL_XOUT_H, data, sizeof(data))) {
    Serial.println("MPU6050 read failed.");
    return;
  }

  int16_t ax = (int16_t)((data[0] << 8) | data[1]);
  int16_t ay = (int16_t)((data[2] << 8) | data[3]);
  int16_t az = (int16_t)((data[4] << 8) | data[5]);
  int16_t rawTemp = (int16_t)((data[6] << 8) | data[7]);

  // MPU6050 datasheet conversion: temperature in degrees Celsius.
  float temperatureC = (rawTemp / 340.0f) + 36.53f;

  char message[100];
  snprintf(message, sizeof(message),
           "{\"ax\":%d,\"ay\":%d,\"az\":%d,\"temp\":%.2f}\n",
           ax, ay, az, temperatureC);

  txCharacteristic->setValue((uint8_t*)message, strlen(message));
  txCharacteristic->notify();

  Serial.print(message);
}
