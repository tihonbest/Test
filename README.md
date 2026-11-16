# Mobility Lab — ESP32 + MPU6050

## Wiring (common ESP32 DevKit)

| MPU6050 | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

Use 3.3 V logic. Do not connect the sensor's I2C pins to 5 V logic.

## Upload firmware

1. Install Arduino IDE.
2. In Arduino IDE, install **esp32 by Espressif Systems** using Boards Manager.
3. Open `esp32_mpu6050_ble.ino`.
4. Select your ESP32 board and its serial port.
5. Upload. Open Serial Monitor at **115200 baud** to see startup messages.
6. The board advertises over Bluetooth LE as **MobilityLab**.

The firmware uses the BLE library included with the Espressif ESP32 Arduino board package; no separate BLE library is required.

## Connect to the website

The firmware implements the Nordic UART Service (NUS) used by `index.html`:

- Service: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- TX notify: `6e400003-b5a3-f393-e0a9-e50e24dcca9e`
- RX write: `6e400002-b5a3-f393-e0a9-e50e24dcca9e`

It sends newline-terminated JSON at approximately 10 samples/second:

`{"ax":123,"ay":-456,"az":16384,"temp":24.7}`

Acceleration values are raw MPU6050 counts at the default ±2g range. Temperature is the temperature of the sensor chip, not body or room temperature.

## Important compatibility note

Web Bluetooth is not supported by Safari on iPhone/iPad. The website's BLE connection therefore needs a compatible browser, such as Chrome or Edge on a computer. The firmware can still be uploaded from a computer using Arduino IDE.

## Troubleshooting

- If Serial Monitor says the sensor was not detected, check wiring and that the module uses I2C address `0x68`. Some boards use `0x69`; this sketch is configured for `0x68`.
- If the board does not appear, reset it and scan again from a Web Bluetooth-compatible browser.
- Keep the sensor and ESP32 on a stable 3.3 V supply.
