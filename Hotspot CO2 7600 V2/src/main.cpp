#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME680.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ---------------------------------------------------------------------------
// NOTE: This sketch only uses the ESP32's built-in WiFi radio to upload data
// (it does NOT use the SIM7600 cellular modem at all). That means the only
// things that actually needed to change when porting from T-SIM7000G to
// T-SIM7600 are the board-specific GPIO pins listed below.
// ---------------------------------------------------------------------------

const char* ssid = "Roger's Phone";
const char* password = "qwertyuiop";

const char* scriptURL =
"https://script.google.com/macros/s/AKfycbyCrH6vVaznfHf4bLDbJxBdZjPP7FsokRXKmH5J4fwS1Ypp-JgbEafOhqvAiNBhBeNE/exec";

// ---------------------------------------------------------------------------
// Board-specific pins for LilyGO T-SIM7600
// (per LilyGO's official LilyGo-Modem-Series pin definitions for SIM7600X)
// ---------------------------------------------------------------------------
const int LED_PIN = 12;        // was 25 on T-SIM7000G; GPIO25 = MODEM_FLIGHT on T-SIM7600
const int BAT_ADC_PIN = 35;    // same on both boards (battery ADC via 1:2 divider)

// K30 UART pins moved off GPIO32/33, which are MODEM_DTR / MODEM_RING on T-SIM7600.
// GPIO14/13 are the (unused, since no SD card here) SD_CLK/SD_CS pins - safe to reuse.
const int K30_RX_PIN = 14;
const int K30_TX_PIN = 13;

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
    
    // Multiply by 2 to reverse the internal 1:2 voltage divider on the battery ADC pin.
    // Note: ESP32 ADCs are slightly non-linear. If your reading is slightly off,
    // you can adjust this 2.0 multiplier (e.g., to 2.05 or 1.95) to calibrate it.
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

    //LED check code is running

    pinMode(LED_PIN, OUTPUT); 
    digitalWrite(LED_PIN, HIGH);

    //BME688

    Wire.begin();

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
        K30_RX_PIN,     // RX
        K30_TX_PIN      // TX
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
