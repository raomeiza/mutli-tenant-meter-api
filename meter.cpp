#include "EmonLib.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <time.h>

EnergyMonitor emon;
EnergyMonitor emon2;
Preferences preferences;

// Define the width and height of the OLED display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// I2C address for the OLED display (commonly 0x3C)
#define OLED_ADDR 0x3C

#define TENANT1_PIN 2 // Define GPIO pin for Relay 1
#define TENANT2_PIN 4 // Define GPIO pin for Relay 2
#define SENSOR_1_IN                                                            \
  34 // pin where the OUT pin from sensor is connected on Arduino
#define SENSOR_2_IN                                                            \
  35 // pin where the OUT pin from sensor is connected on Arduino

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

int mVperAmp = 66; // this the 5A version of the ACS712 -use 100 for 20A Module
                   // and 66 for 30A Module
int Watt = 0;
double AmpsRMS = 0;
double AmpsRMS2 = 0;
double consumedkWh = 0;
double consumedkWh2 = 0;
double kWh = 0;
double kWh2 = 0;
bool tenant1_on = false;
bool tenant2_on = false;

// ofsset for the currents
double offset1 = 0.00;
double offset2 = 0.00;

const char *ssid = "omeiza";
const char *pass = "omeiza112";
unsigned long lastMillis = millis();

// define the url of the server
char *serverName = "http://192.168.137.1:5000";

// create a variable that we will use to store timestamp
char timestamp[30];

// create a long that will store the time we initiate the system. we can only
// get the timestamp from server at startup and subsequently, using the
// timestamp and the time that has elapsed since we boot, we can calculate the
// current time
long start_time = 0;

// define a function that when called, it will calculate the current time using
// the timestamp we got from the server and the time that has elapsed since we
// boot we are not using any library. all we will depend on is the timestamp
// which will be gotten from the server at startup and the time that has elapsed
// since we boot
void calculate_time() {
  // Get the time that has elapsed since we boot
  long elapsed_time = millis() - start_time;
  // Get the current time
  long current_time = atol(timestamp) + elapsed_time;
  // Convert current_time to time_t
  time_t current_time_t = (time_t)current_time;
  // Get the current time in a struct tm format
  struct tm *timeinfo = localtime(&current_time_t);
  // Get the current time in a string format
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
}
// create a method that will be called at setup to ask the server for the
// current time it will send a http get request to servername/timestamp and the
// server is an express server with the route to responde to this as
// app.get('/timestamp', (req, res) => {
//   res.send({ timestamp: Date.now() });
// });
void get_time() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(serverName) + "/timestamp";
    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      String response = http.getString();
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, response);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
      if (doc.containsKey("timestamp")) {
        // Convert milliseconds to seconds
        long serverTimestamp = doc["timestamp"].as<long>() / 1000;
        snprintf(timestamp, sizeof(timestamp), "%ld", serverTimestamp);
        start_time = millis();
      }
    } else {
      Serial.print("Error on sending GET: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

double vCalibration = 88.60;
const char *filename = "/data.json"; // File to store data

// Create a variable to store the MAC address of this ESP32
uint8_t mac[6];
char macStr[18]; // String to store the MAC address

void setup() {
  Serial.begin(115200);
  Serial.println("ACS712 current sensor");
  // get the values of consumedkWH and consumedkWH2 from the preferences
  preferences.begin("energy", false);
  consumedkWh = preferences.getDouble("consumedkWh", 0.0000);
  consumedkWh2 = preferences.getDouble("consumedkWh2", 0.0000);
  kWh = preferences.getDouble("kWh", 0.0000);
  kWh2 = preferences.getDouble("kWh2", 0.0000);
  if (preferences.isKey("tenant1_on")) {
    tenant1_on = preferences.getBool("tenant1_on");
  } else {
    preferences.putBool("tenant1_on", kWh > consumedkWh);
    tenant1_on = kWh > consumedkWh;
  }
  if (preferences.isKey("tenant2_on")) {
    tenant2_on = preferences.getBool("tenant2_on");
  } else {
    preferences.putBool("tenant2_on", kWh2 > consumedkWh2);
    tenant2_on = kWh2 > consumedkWh2;
  }

  // Get the MAC address
  WiFi.macAddress(mac);
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0],
           mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print("MAC Address: ");
  Serial.println(macStr);
  // store the mac address in the preferences
  preferences.putString("mac", macStr);

  emon.voltage(32, vCalibration,
               1.7); // Voltage: input pin, calibration, phase_shift

  calibrateCurrOffsets();
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }

  // Clear the display
  display.clearDisplay();

  // Set text size and color
  display.setTextSize(1);              // Can adjust size (1-8)
  display.setTextColor(SSD1306_WHITE); // White text

  // Display a welcome message
  display.setCursor(0, 0);
  display.println("Hello, ESP32!");
  display.display(); // Output to the screen

  pinMode(TENANT1_PIN, OUTPUT); // Set the relay pins as outputs
  pinMode(TENANT2_PIN, OUTPUT);
  // Initially turn off both relays
  digitalWrite(TENANT1_PIN, HIGH);
  digitalWrite(TENANT2_PIN, HIGH);

  if (tenant1_on) {
    digitalWrite(TENANT1_PIN, LOW);
  }
  if (tenant2_on) {
    digitalWrite(TENANT2_PIN, LOW);
  };
  // Connect to Wi-Fi
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");


  // Get the current time from the server
  get_time();
  // set the time of the system
  start_time = millis();
}

void loop() {
  sendComplexData();
  if (tenant1_on) {
    digitalWrite(TENANT1_PIN, LOW);
  } else {
    digitalWrite(TENANT1_PIN, HIGH);
  }
  if (tenant2_on) {
    digitalWrite(TENANT2_PIN, LOW);
  } else {
    digitalWrite(TENANT2_PIN, HIGH);
  }
}

float getAmpsRMS(int pin, double offset) {
  float result;
  int readValue;       // value read from the sensor
  long sumValue = 0;   // accumulate readings
  int sampleCount = 0; // number of readings taken
  double voltage = 0;  // voltage value
  double vrms = 0;     // rms voltage value
  double ampsRms = 0;  // rms voltage value
  double VRMS = 0;     // rms voltage value

  uint32_t start_time = millis();

  while ((millis() - start_time) < 1000) // sample for 1 Sec
  {
    readValue = analogRead(pin); // read the sensor
    sumValue += readValue;       // accumulate the sensor readings
    sampleCount++;               // increment sample count
  }

  // Calculate the average sensor value
  float averageValue = sumValue / (float)sampleCount;

  // Convert the average sensor value to voltage (3.3V scale)
  voltage = (averageValue * 3.3) / 4096.0; // ESP32 ADC resolution 4096
  VRMS = (voltage / 2.0) * 0.707;          // root 2 is 0.707
  ampsRms = ((VRMS * 1000) / mVperAmp) -
            offset; // 0.3 is the error I got for my sensor
  return ampsRms;
}

void calibrateCurrOffsets() {
  Serial.print("before. offset 1:   ");
  Serial.print(offset1);
  Serial.print("\t");
  Serial.print("offset 2:   ");
  Serial.println(offset2);

  float initialAmpsRMS1 = getAmpsRMS(SENSOR_1_IN, offset1);
  float initialAmpsRMS2 = getAmpsRMS(SENSOR_2_IN, offset2);
  // Determine offsets to ensure RMS current readings are zero
  offset1 += initialAmpsRMS1 +0.300;
  offset2 += initialAmpsRMS2 + 0.300;
  Serial.print("after. offset 1:   ");
  Serial.print(offset1);
  Serial.print("\t");
  Serial.print("offset 2:   ");
  Serial.println(offset2);
}

void sendComplexData() {
  emon.calcVI(20, 2000);

  // Calculate the RMS current and take the absolute value to avoid negative
  // values
  if (tenant1_on && emon.Vrms > 120) {
    AmpsRMS = getAmpsRMS(SENSOR_1_IN, offset1);
    // if ampsrms is negetive, return 0
    if (AmpsRMS < 0) {
      AmpsRMS = 0;
    }
    consumedkWh = consumedkWh + (emon.Vrms * AmpsRMS) *
                                    (millis() - lastMillis) / 3600000000.0;
    if (consumedkWh >= kWh) {
      tenant1_on = false;
      preferences.putBool("tenant1_on", false);
    }
    preferences.putDouble("consumedkWh", consumedkWh);
  } else {
    AmpsRMS = 0;
  }
  if (tenant2_on && emon.Vrms > 120) {
    AmpsRMS2 = abs(getAmpsRMS(SENSOR_2_IN, offset2));

    consumedkWh2 = consumedkWh2 + (emon.Vrms * AmpsRMS2) *
                                      (millis() - lastMillis) / 3600000000.0;
    if (consumedkWh2 >= kWh) {
      tenant2_on = false;
      preferences.putBool("tenant2_on", false);
    }
    preferences.putDouble("consumedkWh2", consumedkWh2);
  } else {
    AmpsRMS2 = 0;
  }
  // Save the updated consumedkWh values to preferences

  HTTPClient http;
  yield();
  // log tenant 1 data to console
  Serial.print("tenant 1:  ");
  Serial.print("Vrms: ");
  Serial.print(emon.Vrms, 2);
  Serial.print("V");

  Serial.print("\tIrms: ");
  Serial.print(AmpsRMS, 4);
  Serial.print("A");

  Serial.print("\tPower: ");
  Serial.print((emon.Vrms * AmpsRMS), 4);
  Serial.print("W");

  Serial.print("\tconsumedkWh: ");
  Serial.print(consumedkWh, 5);
  Serial.println("consumedkWh");

  // log tenant 2 to console
  Serial.print("tenant 2;  ");
  Serial.print("Vrms: ");
  Serial.print(emon.Vrms, 2);
  Serial.print("V");

  Serial.print("\tIrms: ");
  Serial.print(AmpsRMS2, 4);
  Serial.print("A");

  Serial.print("\tPower: ");
  Serial.print((emon.Vrms * AmpsRMS2), 4);
  Serial.print("W");

  Serial.print("\tconsumedkWh: ");
  Serial.print(consumedkWh2, 5);
  Serial.println("consumedkWh");
  Serial.println("");

  File file = SPIFFS.open(filename, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for reading");
    // return false;
  }

  String data = "";
  while (file.available()) {
    data += file.readString(); // Read all stored data
  }
  file.close();

  Serial.println("Data to be sent to API: " + data);
  // Create JSON object with nested data
  StaticJsonDocument<512> jsonDoc;

  // Main object fields
  jsonDoc["device_id"] = "meter_001";
  // get the current time and store it in the json object
  // struct tm timeinfo;
  // if (!getLocalTime(&timeinfo)) {
  //   Serial.println("Failed to obtain time");
  //   return;
  // }
  calculate_time();
  jsonDoc["timestamp"] = timestamp;
  // if the emon.Vrms is greater than 120, then there is phcn
  jsonDoc["phcn"] = emon.Vrms > 120;
  if (jsonDoc["phcn"] == false) {
    jsonDoc["energy_consumed"] = 0;
  } else {
    jsonDoc["energy_consumed"] = consumedkWh + consumedkWh2; // consumedkWh
  }

   // Nested object for tenant information
  JsonArray tenants = jsonDoc.createNestedArray("tenants");


  // Tenant 1
  JsonObject tenant1 = tenants.createNestedObject();
  tenant1["tenant_id"] = "tenant_01";
  tenant1["status"] = tenant1_on;
  tenant1["subscription_end"] = "2024-09-01T00:00:00Z";
  tenant1["phcn"] = emon.Vrms > 120;
  tenant1["voltage"] = emon.Vrms;
  tenant1["tenant_on"] = tenant1_on;
  tenant1["kWh"] = kWh;

  // Tenant 2
  JsonObject tenant2 = tenants.createNestedObject();
  tenant2["tenant_id"] = "tenant_02";
  tenant2["status"] = tenant2_on;
  tenant2["subscription_end"] = "2024-07-15T00:00:00Z";
  tenant2["phcn"] = emon.Vrms > 120;
  tenant2["voltage"] = emon.Vrms;
  tenant2["tenant_on"] = tenant2_on;
  tenant2["kWh"] = kWh2;

  // Nested object for power data
  JsonObject powerData = jsonDoc.createNestedObject("power_data");
  powerData["current"] = AmpsRMS;
  powerData["power"] = AmpsRMS * emon.VRMS; // Calculated power consumption
  powerData["energy_consumed"] = consumedkWh; // consumedkWh
  tenant1["data"] = powerData;

  // Nested object for power data
  JsonObject powerData2 = jsonDoc.createNestedObject("power_data");
  powerData2["current"] = AmpsRMS2;
  powerData2["power"] = AmpsRMS2 * emon.Vrms; // Calculated power consumption
  powerData2["energy_consumed"] = consumedkWh2; // consumedkWh
  tenant2["data"] = powerData2;


  // Convert JSON object to string
  String jsonString;
  serializeJson(jsonDoc, jsonString);
  data += jsonString;
  addDataToFile(jsonString);
  Serial.println(data);
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Begin the HTTP request
    http.begin(serverName);

    // Add headers
    http.addHeader("Content-Type", "application/json");
    // http.addHeader("Authorization", authToken);

    // Send POST request with JSON payload
    int httpResponseCode = http.POST(data);

    if (httpResponseCode > 0) {
      clearData();
      // Successfully sent, read the response
      String response = http.getString();
      // Allocate a temporary JsonDocument
      StaticJsonDocument<256> doc;

      // Parse the JSON response
      DeserializationError error = deserializeJson(doc, response);
      Serial.println("response: " + response);
      // Check for errors in parsing
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }

      // Check and extract each possible key-value pair
      if (doc.containsKey("tenant_1_on")) {
        bool tenant_1_on = doc["tenant_1_on"];
        Serial.print("tenant_1_on: ");
        if (tenant_1_on && consumedkWh < kWh) {
          tenant1_on = true;
          preferences.putBool("tenant1_on", true);
          digitalWrite(TENANT1_PIN, LOW);
        } else {
          tenant1_on = false;
          preferences.putBool("tenant1_on", false);
          digitalWrite(TENANT1_PIN, HIGH);
        }
        Serial.println(tenant_1_on);
      }

      if (doc.containsKey("tenant_2_on")) {
        bool tenant_2_on = doc["tenant_2_on"];
        if (tenant_2_on && consumedkWh2 < kWh2) {
          tenant2_on = true;
          preferences.putBool("tenant2_on", true);
          digitalWrite(TENANT2_PIN, LOW);
        } else {
          tenant2_on = false;
          preferences.putBool("tenant2_on", false);
          digitalWrite(TENANT2_PIN, HIGH);
        }
        Serial.print("tenant_2_on: ");
        Serial.println(tenant_2_on);
      }

      if (doc.containsKey("tenant_1_clear")) {
        bool tenant_1_clear = doc["tenant_1_clear"];
        if (tenant_1_clear) {
          consumedkWh = 0;
          kWh = 0;
          preferences.putDouble("consumedkWh", 0);
          preferences.putDouble("kWh", 0);
        }
        Serial.print("tenant_1_clear: ");
        Serial.println(tenant_1_clear);
      }

      if (doc.containsKey("tenant_2_clear")) {
        bool tenant_2_clear = doc["tenant_2_clear"];
        if (tenant_2_clear) {
          consumedkWh2 = 0;
          kWh2 = 0;
          preferences.putDouble("consumedkWh2", 0);
          preferences.putDouble("kWh2", 0);
        }
        Serial.print("tenant_2_clear: ");
        Serial.println(tenant_2_clear);
      }

      if (doc.containsKey("tenant_1_add_kwh")) {
        float tenant_1_add_kwh = doc["tenant_1_add_kwh"];
        if (tenant_1_add_kwh > 0) {
          kWh += tenant_1_add_kwh;
          preferences.putDouble("kWh", kWh);
        }

        Serial.print("tenant_1_add_kwh: ");
        Serial.println(tenant_1_add_kwh);
      }

      if (doc.containsKey("tenant_2_add_kwh")) {
        float tenant_2_add_kwh = doc["tenant_2_add_kwh"];
        if (tenant_2_add_kwh > 0) {
          kWh2 += tenant_2_add_kwh;
          preferences.putDouble("kWh2", kWh2);
        }
        Serial.print("tenant_2_add_kwh: ");
        Serial.println(tenant_2_add_kwh);
      }

    } else {
      // Error occurred
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }

    // End the HTTP request
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}
// Function to add data to the file
void addDataToFile(String newData) {
  File file = SPIFFS.open(filename, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }

  // file.println(newData); // Append new data
  file.close();

  Serial.println("Data stored: " + newData);
}

// Function to retrieve stored data (if any) and attempt to send it to the API

// Function to clear the stored data after successful API submission
void clearData() {
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for clearing");
    return;
  }

  file.print(""); // Overwrite file with empty content
  file.close();

  Serial.println("Data cleared");
}
