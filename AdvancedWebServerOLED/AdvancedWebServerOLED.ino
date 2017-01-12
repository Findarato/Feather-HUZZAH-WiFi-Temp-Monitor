#include <SPI.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <Adafruit_Sensor.h>
#include <ESP8266WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "DHT.h"

// Defines
#define DHTPIN 14
#define DHTTYPE DHT21   // DHT 21 (AM2301)
#define OLED_RESET 3
Adafruit_SSD1306 display(OLED_RESET);

#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16
static const unsigned char PROGMEM logo16_glcd_bmp[] =
{ B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000 };

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

DHT dht(DHTPIN, DHTTYPE,12);             // this constant won't change:
const int buttonPinIP   = 0;             // This will show your IP
const int buttonPinTemp = 16;            // This will show your Temp
const char* ssid        = "SSID";        // Enter your SSID
const char* password    = "password";    // Enter your Password
const char* DeviceName  = "Device Name"; // Enter your Device Name
const int   DeviceID    = 1;             // This is for My Specific use case

int buttonStateIP       = LOW;           // current state of the button
int lastButtonStateIP   = HIGH;          // previous state of the button
int buttonStateTemp     = LOW;           // current state of the button
int lastButtonStateTemp = HIGH;          // previous state of the button

WiFiServer server(80);


float pfDew,pfHum,pfTemp,pfVcc,pfTempF,battery;


void connectionInfo () {
  display.clearDisplay();
  display.display();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println(ssid);
  display.println(WiFi.localIP());
  display.println("Device Location");
  display.print(DeviceName);
  display.display();
}


void readTempValues() {
   pfTemp = dht.readTemperature();
   pfHum = dht.readHumidity();
   pfTempF = dht.readTemperature(true);
   float a = 17.67;
   float b = 243.5;
   float alpha = (a * pfTemp)/(b + pfTemp) + log(pfHum/100);
   pfDew = (b * alpha)/(a - alpha);


  Serial.println(pfTemp);
  Serial.println(pfTempF);
  Serial.println(pfHum);
  Serial.println(pfDew);


  display.clearDisplay();
  display.display();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.print("Temp C: ");
  display.println(pfTemp);
  display.print("Temp F: ");
  display.println(pfTempF);
  display.print("Humidity: ");
  display.println(pfHum);
  display.print("Duepoint: ");
  display.println(pfDew);
  display.display();

}

bool readRequest(WiFiClient& client) {
  bool currentLineIsBlank = true;
  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      if (c == '\n' && currentLineIsBlank) {
        return true;
      } else if (c == '\n') {
        currentLineIsBlank = true;
      } else if (c != '\r') {
        currentLineIsBlank = false;
      }
    }
  }
  return false;
}

JsonObject& prepareResponse(JsonBuffer& jsonBuffer) {
  JsonObject& root = jsonBuffer.createObject();
  JsonArray& JSONDeviceName = root.createNestedArray("DeviceName");
    JSONDeviceName.add(DeviceName);
  JsonArray& JSONDeviceID = root.createNestedArray("DeviceID");
    JSONDeviceID.add(DeviceID);
  JsonArray& JSONtempF = root.createNestedArray("tempF");
    JSONtempF.add(pfTempF);
  JsonArray& JSONtempC = root.createNestedArray("tempC");
    JSONtempC.add(pfTemp);
  JsonArray& JSONhumiValues = root.createNestedArray("humidity");
    JSONhumiValues.add(pfHum);
  JsonArray& JSONdewpValues = root.createNestedArray("dewpoint");
    JSONdewpValues.add(pfDew);
  return root;
}

void writeResponse(WiFiClient& client, JsonObject& json) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();

  json.prettyPrintTo(client);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Up!");
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  delay(100);

    // Connect to WiFi network
  WiFi.begin(ssid, password);

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  delay(200);

  // Clear the buffer.
  display.clearDisplay();
  display.println("Startup Temp IoT Thingy");
  display.display();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    Serial.println("");
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.println("Connecting to: ...");
    display.println(ssid);
    display.display();
  }

  dht.begin();

  connectionInfo();
  delay(2000);

  // Start the server
  server.begin();
  Serial.println("Server started");

  // initialize the button pin as a input:
  pinMode(buttonPinIP, INPUT_PULLUP);
  pinMode(buttonPinTemp, INPUT_PULLUP);

}

void loop() {

  WiFiClient client = server.available();
  if (client) {
      bool success = readRequest(client);
      if (success) {
          delay(1000);
          readTempValues();
          StaticJsonBuffer<500> jsonBuffer;
          JsonObject& json = prepareResponse(jsonBuffer);
          writeResponse(client, json);
      }
      delay(1);
      client.stop();
  }

  // read the pushbutton input pin:
  buttonStateIP = digitalRead(buttonPinIP);
   // compare the buttonState to its previous state
  if (buttonStateIP != lastButtonStateIP) {
    // if the state has changed, increment the counter
    connectionInfo();
  }

  buttonStateTemp = digitalRead(buttonPinTemp);
   // compare the buttonState to its previous state
  if (buttonStateTemp != lastButtonStateTemp) {
    // if the state has changed, increment the counter
    readTempValues();
  }


}
