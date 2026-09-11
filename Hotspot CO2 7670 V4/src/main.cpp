#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME680.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ---------------------------------------------------------------------------
// NOTE: This sketch only uses the ESP32-S3's built-in WiFi radio to upload
// data (it does NOT use the SIM7670G cellular/GPS modem at all). That means
// the only things that actually needed to change when porting to
// T-SIM7670G-S3 are the board-specific GPIO pins listed below, taken from
// LilyGO's official pin map for this board.
// ---------------------------------------------------------------------------

const char* ssid = "Roger's Phone";
const char* password = "qwertyuiop";

const char* scriptURL =
"https://script.google.com/macros/s/AKfycbyCrH6vVaznfHf4bLDbJxBdZjPP7FsokRXKmH5J4fwS1Ypp-JgbEafOhqvAiNBhBeNE/exec";

// ---------------------------------------------------------------------------
// Board-specific pins for LilyGO T-SIM7670G-S3-Standard (ESP32-S3-WROOM-1)
// ---------------------------------------------------------------------------
const int BAT_ADC_PIN = 8;     // official "Battery ADC Pin" on this board

// This board's dedicated I2C pins (its ESP32-S3 core default Wire pins can
// differ, so set these explicitly rather than relying on Wire.begin()'s
// defaults).
const int I2C_SDA_PIN = 3;
const int I2C_SCL_PIN = 2;

// K30 Pins
const int K30_TX_PIN = 47;  // Connect to the top-most pin
const int K30_RX_PIN = 16;  // Connect to the top-2nd pin
// const int K30_5V_PIN =;  // Connect to the top-3rd pin
// const int K30_GD_PIN =;  // Connect to the bot-most pin


// This board has no spare, user-controllable onboard LED - the visible LEDs
// are hardwired to the modem/charging chips and always show their own
// status. If you want a status LED, wire one (with a resistor) to a free
// GPIO such as this one, or comment out the LED lines below entirely.
const int LED_PIN = 15;        // Camera Y3 pin, free if no camera is attached

HardwareSerial K30Serial(2);

Adafruit_BME680 bme;

// K30 packet
byte requestCO2[] =
{
    0x68,
    0x04,
    0x00,
    0x03,
    0x00,
    0x01,
    0xC8,
    0xF3
};

byte response[7];

int readCO2()
{
    while(K30Serial.available())
        K30Serial.read();

    K30Serial.write(requestCO2, 8);

    unsigned long start = millis();

    while (K30Serial.available() < 7)
    {
        if (millis() - start > 200)
            return -1;
    }

    for(int i=0;i<7;i++)
        response[i]=K30Serial.read();

    int ppm = response[3]*256 + response[4];

    return ppm;
}


// Function to read and calculate the current battery voltage
float readBatteryVoltage()
{
    // Read raw ADC value (0 to 4095)
    int adcValue = analogRead(BAT_ADC_PIN);
    
    // Convert ADC to the voltage present at the ESP32 pin (assuming 3.3V reference)
    float pinVoltage = (adcValue / 4095.0) * 3.3;
    
    // Multiply by 2 to reverse the board's internal voltage divider.
    // NOTE: this assumes the same 1:2 divider ratio used on LilyGO's other
    // SIM7000G/7600 boards. This has not been independently confirmed for
    // the T-SIM7670G's schematic - check a raw multimeter reading of your
    // battery against this calculation once, and adjust the multiplier if
    // it's off.
    float batteryVoltage = pinVoltage * 2.0; 
    
    return batteryVoltage;
}

void setup()
{

    Serial.begin(115200);

    // Connect wifi
    Serial.print("Connecting");

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("Connected!");

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // LED check code is running
    // (Remove these two lines if you don't wire up an external status LED)

    pinMode(LED_PIN, OUTPUT); 
    digitalWrite(LED_PIN, HIGH);

    //BME688

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    Serial.println();
    Serial.println("Starting...");

    if(!bme.begin())
    {
        Serial.println("BME688 NOT FOUND");
        while(1);
    }

    Serial.println("BME688 OK");

    // UART2 CO2
    K30Serial.begin(
        9600,
        SERIAL_8N1,
        K30_TX_PIN,    // RX. Note that the code logic is swapped — the K30 TX pin is actually used for receiving,
        K30_RX_PIN     // TX. and the K30 RX pin is actually used for transmitting.
    );

    Serial.println("K30 UART Started");
}

void loop()
{

    //---------------------------------------
    // Read BME688
    //---------------------------------------

    if(bme.performReading())
    {
        Serial.println("========== BME688 ==========");

        Serial.print("Temperature : ");
        Serial.print(bme.temperature);
        Serial.println(" C");

        Serial.print("Humidity    : ");
        Serial.print(bme.humidity);
        Serial.println(" %");

        Serial.print("Pressure    : ");
        Serial.print(bme.pressure/100.0);
        Serial.println(" hPa");

        Serial.print("Gas         : ");
        Serial.print(bme.gas_resistance);
        Serial.println(" Ohms");
    }
    else
    {
        Serial.println("BME688 Read Failed");
    }

    //---------------------------------------
    // Read K30
    //---------------------------------------

    int co2 = readCO2();

    Serial.println("========== K30 ==========");

    if(co2 > 0)
    {
        Serial.print("CO2 : ");
        Serial.print(co2);
        Serial.println(" ppm");
    }
    else
    {
        Serial.println("No response from K30");
    }

    Serial.println();
    Serial.println("------------------------------");
    Serial.println();

    //---------------------------------------
    // Read Battery Voltage
    //---------------------------------------
    float batteryVoltage = readBatteryVoltage();
    
    Serial.println("========== Battery ==========");
    Serial.print("Voltage     : ");
    Serial.print(batteryVoltage, 2);
    Serial.println(" V");

    Serial.println();
    Serial.println("------------------------------");
    Serial.println();

    // delay(2000);
    delay(200);
    
    //Wifi + Upload data
    if (WiFi.status() == WL_CONNECTED)
    {
        HTTPClient http;

        String url = String(scriptURL)
            + "?temp=" + String(bme.temperature)
            + "&humidity=" + String(bme.humidity)
            + "&pressure=" + String(bme.pressure / 100.0)
            + "&gas=" + String(bme.gas_resistance)
            + "&co2=" + String(co2)
            + "&vbat=" + String(batteryVoltage, 2);

        http.begin(url);

        int httpCode = http.GET();

        Serial.print("HTTP Code: ");
        Serial.println(httpCode);

        http.end();
    }
}
