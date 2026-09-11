When creating the project in PlaformIO, use:
espressif ESP32-S3-DevKitC-1-N8 (8MB QD, No PSRAM)

Here are the pin connections.
BME688:
VIN > 3v3
GND > GND
SCK > SCL
SDI > SDA

K30 (orientate s.t. jumper wires coming out to the left):
const int K30_TX_PIN = 47;  // Connect to the top-most pin
const int K30_RX_PIN = 16;  // Connect to the top-2nd  pin
// const int K30_5V_PIN =;  // Connect to the top-3rd  pin
// const int K30_GD_PIN =;  // Connect to the bot-most pin