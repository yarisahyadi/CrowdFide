#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, D2, D1, 0, D4, D5, D6, D7, D3, POSITIVE);
#define sensorPin1 D7
#define sensorPin2 D8

int sensorState1 = 0;
int sensorState2 = 0;
int count = 0;
String sequence = "";

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// NTP settings
const char* ntpServer = "pool.ntp.org";
const long utcOffsetInSeconds = 25200; // UTC offset in seconds (GMT+7 for example)

// Twilio credentials
const char* accountSID = "YOUR_TWILIO_ACCOUNT_SID";
const char* authToken = "YOUR_TWILIO_AUTH_TOKEN";
const char* fromNumber = "YOUR_TWILIO_PHONE_NUMBER";
const char* toNumber = "RECIPIENT_PHONE_NUMBER";

// Endpoint details
const char* endpoint = "YOUR_API_Endpoint";

// WiFiUDP instance for NTP
WiFiUDP ntpUDP;

// NTP client instance
NTPClient timeClient(ntpUDP, ntpServer, utcOffsetInSeconds);

float readCounter() {
  return count;
}

void setup() {
  Serial.begin(9600);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");

  // Initialize and synchronize time
  timeClient.begin();
  timeClient.update();

  pinMode(sensorPin1, INPUT_PULLUP);
  pinMode(sensorPin2, INPUT_PULLUP);

  lcd.begin(16, 2);
  lcd.backlight();
  lcd.setCursor(4, 0);
  lcd.print("COUNTER");
  lcd.setCursor(0, 1);
  lcd.print("No Visitors");
  delay(200);
}

void sendTwilioMessage(const String& message) {
  // Build the URL for the Twilio API request
  String url = "https://api.twilio.com/2010-04-01/Accounts/" + String(accountSID) + "/Messages.json";

  // Create the HTTP client
  WiFiClientSecure client;
  if (!client.connect("api.twilio.com", 443)) {
    Serial.println("Connection failed");
    return;
  }

  // Prepare the POST request
  String postData = "From=" + String(fromNumber) + "&Body=" + message + "&To=" + String(toNumber);
  String headers = "Content-Type: application/x-www-form-urlencoded\r\nAuthorization: Basic " + String(base64Encode(accountSID + String(":") + authToken)) + "\r\nContent-Length: " + String(postData.length());

  // Send the POST request
  client.print("POST " + url + " HTTP/1.1\r\n");
  client.print("Host: api.twilio.com\r\n");
  client.print(headers + "\r\n");
  client.print(postData);

  // Read the response
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  String response = client.readString();
  client.stop();

  Serial.println("Message sent!");
  Serial.println("Response:");
  Serial.println(response);
}

String base64Encode(String value) {
  static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
  String encoded = "";
  int inLen = value.length();
  int outLen = 4 * ((inLen + 2) / 3);
  for (int i = 0; i < inLen; i += 3) {
    int byte1 = value.charAt(i);
    int byte2 = i + 1 < inLen ? value.charAt(i + 1) : 0;
    int byte3 = i + 2 < inLen ? value.charAt(i + 2) : 0;
    int enc1 = byte1 >> 2;
    int enc2 = ((byte1 & 3) << 4) | (byte2 >> 4);
    int enc3 = ((byte2 & 15) << 2) | (byte3 >> 6);
    int enc4 = byte3 & 63;
    encoded += alphabet[enc1];
    encoded += alphabet[enc2];
    encoded += alphabet[enc3];
    encoded += alphabet[enc4];
  }
  return encoded;
}

void loop() {
  // Visitor count logic
  sensorState1 = digitalRead(sensorPin1);
  sensorState2 = digitalRead(sensorPin2);

  if (sensorState1 == LOW) {
    sequence += "1";
    delay(500);
  }

  if (sensorState2 == LOW) {
    sequence += "2";
    delay(500);
  }

  if (sequence.equals("12")) {
    count++;
    sequence = "";
    delay(550);
  } else if (sequence.equals("21") && count > 0) {
    count--;
    sequence = "";
    delay(550);
  }

  if (sequence.length() > 2 || sequence.equals("11") || sequence.equals("22")) {
    sequence = "";
  }

  // Display visitor count on LCD
  if (count <= 0) {
    lcd.setCursor(0, 1);
    lcd.print("No visitors    ");
    Serial.print("Visitors: ");
    Serial.print(count);
  } else if (count > 0 && count < 10) {
    lcd.setCursor(0, 1);
    lcd.print("Visitors:   ");
    lcd.setCursor(12, 1);
    lcd.print(count);
    lcd.setCursor(13, 1);
    lcd.print("  ");
    Serial.print("Visitors: ");
    Serial.print(count);
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Visitors:   ");
    lcd.setCursor(12, 1);
    lcd.print(count);
    Serial.print("Visitors: ");
    Serial.print(count);
  }

  // Check if visitor count exceeds whUsed
  float whUsed = 11.00f; // Set the threshold for whUsed
  if (count > whUsed) {
    sendTwilioMessage("Ruangan ini penuh, jumlah: " + String(count)+"/" + String(whUsed));
  }

  // Check if time synchronization is needed
  if (timeClient.update()) {
    // Create JSON object
    StaticJsonDocument<256> jsonDoc;
    float whGenerated = readCounter() + 0.001;

    // Populate JSON data
    jsonDoc["siteId"] = 1;
    jsonDoc["whUsed"] = whUsed; // Float value for whUsed
    jsonDoc["whGenerated"] = whGenerated; // Use the visitor count as whGenerated value
    jsonDoc["tempC"] = 27.3f; // Float value for tempC

    // Get the current Unix timestamp
    time_t currentTime = timeClient.getEpochTime();
    jsonDoc["dateTime"] = (unsigned long)currentTime;

    // Serialize JSON to string
    String jsonString;
    serializeJson(jsonDoc, jsonString);

    // Print the JSON payload
    Serial.println("JSON Payload:");
    Serial.println(jsonString);

    // Send HTTP POST request
    WiFiClient client;
    HTTPClient http;
    http.begin(client, endpoint);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(jsonString);

    // Check for successful request
    if (httpResponseCode == HTTP_CODE_OK) {
      Serial.println("Data sent successfully");
      sendTwilioMessage("Visitor count updated: " + String(count));
    } else {
      Serial.print("HTTP POST request failed with error code: ");
      Serial.println(httpResponseCode);
    }

    // Disconnect from the server
    http.end();
  }

  // Wait for some time before checking for time update and sending the next request
  delay(5000);
}
