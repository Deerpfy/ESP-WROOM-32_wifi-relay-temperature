//////////////////////////////////////////////////////////////
//                                                          //
//  Project: Temperature-controlled Relay System            //
//  Author: Deerpfy                                         //
//  Date: 15th September 2024                               //
//                                                          //
//  Description:                                            //
//  This project uses an ESP32 to monitor the temperature   //
//  from a DHT11 sensor and control a relay (simulated      //
//  using an LED). The ESP32 hosts a web server for manual  //
//  control and to display current temperature data.        //
//                                                          //
//  Pin Configuration:                                      //
//  - DHT11 Sensor:                                         //
//    - Data Pin -> GPIO 4 (DHTPIN)                         //
//                                                          //
//  - Relay (simulated using an LED):                       //
//    - Control Pin -> GPIO 2 (RELAY_PIN)                   //
//                                                          //
//  - WiFi credentials:                                     //
//    - Loaded from an external file ("wifi_credentials.h").//
//                                                          //
//////////////////////////////////////////////////////////////

//----------------------------------------------------------//

#include "WiFi.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>

// Include external file for WiFi credentials from the correct subfolder
#include "ESP-WROOM-32_wifi-settings/wifi_credentials.h"

// Define pin for DHT11 and relay (simulated using LED)
#define DHTPIN 4
#define DHTTYPE DHT11
#define RELAY_PIN 2  // Pin for relay (simulated using LED)

DHT dht(DHTPIN, DHTTYPE);

// WiFi connection setup
// The SSID and password are loaded from an external file "wifi_credentials.h"
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Create an instance of the web server on port 80
AsyncWebServer server(80);

// Global variables
float temperature = 0.0;
bool relayState = false;  // Relay status (ON/OFF)
bool manualMode = false;  // Mode switch: false = automatic, true = manual
float criticalTemp = 23.80;
String serialOutput = "";  // To store the serial monitor output
int refreshTime = 10;  // Interval for automatic refresh (in seconds)

void setup() {
  Serial.begin(9600);
  
  // Initialize the DHT sensor
  dht.begin();
  
  // Initialize the relay (simulated using LED)
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // The relay is initially off

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Still connecting...");
  }

  Serial.println("Connected to WiFi");
  Serial.println("IP Address: ");
  Serial.println(WiFi.localIP());

  // Set up the web server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<html><head>";
    html += "<meta charset='UTF-8'>";  // Ensure UTF-8 encoding
    html += "<meta http-equiv='refresh' content='" + String(refreshTime) + "'>";  // Auto-refresh every refreshTime seconds
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; background-color: #f0f0f0; text-align: center; padding-top: 50px; }";
    html += "h1 { font-size: 62px; color: #333; }";  // Enlarge heading by 30%
    html += ".temperature { font-size: 94px; color: #FF5733; }";  // Enlarge temperature by 30%
    html += ".relay-status { font-size: 31px; color: " + String(relayState ? "#28a745" : "#dc3545") + "; }";  // Green for ON, red for OFF
    html += ".serial-output { font-size: 18px; color: #000; padding-top: 20px; }";  // Enlarge output text
    html += "button { padding: 15px 40px; font-size: 20px; margin: 10px; }";  // Enlarge buttons
    html += ".on-button { background-color: #28a745; color: white; }";  // Green button for ON
    html += ".off-button { background-color: #dc3545; color: white; }";  // Red button for OFF
    html += "input[type='number'] { font-size: 30px; padding: 10px; width: 120px; }";  // Enlarge input field for critical temperature
    html += "</style>";
    
    // JavaScript to display countdown
    html += "<script>";
    html += "var countdown=" + String(refreshTime) + ";";
    html += "setInterval(function() {";
    html += "if(countdown > 0) { countdown--; document.getElementById('countdown').innerHTML = countdown; }";
    html += "}, 1000);";
    html += "</script>";
    
    html += "</head><body>";
    html += "<h1>Current Temperature</h1>";
    html += "<div class='temperature'>" + String(temperature) + " °C</div>";
    html += "<div class='relay-status'>Relay is " + String(relayState ? "ON" : "OFF") + "</div>";  // Display relay status

    // Display countdown
    html += "<div style='padding-top:20px;'><strong>Automatic refresh in: <span id='countdown'>" + String(refreshTime) + "</span> seconds</strong></div>";

    // Mode switch
    html += "<div style='padding-top:20px;'><strong>Mode:</strong> ";
    html += "<form action='/toggleMode' method='GET'>";
    html += "<button type='submit'>" + String(manualMode ? "Switch to Automatic" : "Switch to Manual") + "</button>";
    html += "</form></div>";

    // Manual relay control (only displayed in manual mode)
    if (manualMode) {
      html += "<div style='padding-top:20px;'><strong>Manual Relay Control:</strong> ";
      html += "<form action='/relayOn' method='GET'><button class='on-button' type='submit'>ON</button></form>";
      html += "<form action='/relayOff' method='GET'><button class='off-button' type='submit'>OFF</button></form></div>";
    }

    // Display critical temperature and option to adjust it
    html += "<div style='padding-top:20px;'><strong>Critical Temperature: </strong>" + String(criticalTemp) + " °C</div>";
    html += "<form action='/setCritical' method='GET'>";
    html += "<input type='number' step='0.1' name='temp' value='" + String(criticalTemp) + "'>";
    html += "<button type='submit'>Set Critical Temperature</button>";
    html += "</form>";

    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  // Mode switch (automatic/manual)
  server.on("/toggleMode", HTTP_GET, [](AsyncWebServerRequest *request){
    manualMode = !manualMode;
    Serial.println("Switched to " + String(manualMode ? "Manual" : "Automatic") + " Mode");
    request->redirect("/");
  });

  // Relay control - turn ON (manual mode)
  server.on("/relayOn", HTTP_GET, [](AsyncWebServerRequest *request){
    if (manualMode) {
      relayState = true;
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("Relay turned ON (Manual)");
      serialOutput = "Manual mode: Relay is ON";
    }
    request->redirect("/");
  });

  // Relay control - turn OFF (manual mode)
  server.on("/relayOff", HTTP_GET, [](AsyncWebServerRequest *request){
    if (manualMode) {
      relayState = false;
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("Relay turned OFF (Manual)");
      serialOutput = "Manual mode: Relay is OFF";
    }
    request->redirect("/");
  });

  // Set critical temperature
  server.on("/setCritical", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("temp")) {
      criticalTemp = request->getParam("temp")->value().toFloat();
      Serial.println("Critical Temperature set to: " + String(criticalTemp) + " °C");
      serialOutput = "Critical Temperature set to: " + String(criticalTemp) + " °C";
    }
    request->redirect("/");
  });

  // Start the server
  server.begin();
}

void loop() {
  // Read temperature every 2 seconds
  temperature = dht.readTemperature();

  if (!manualMode) {  // Automatic mode
    if (isnan(temperature)) {
      Serial.println("Error reading temperature");
      serialOutput = "Error reading temperature";  // Write to serial monitor
    } else {
      String tempString = "Temperature: " + String(temperature) + " °C";
      Serial.println(tempString);
      
      // If the temperature drops below the critical threshold, the relay turns on
      if (temperature < criticalTemp) {
        relayState = true;
        Serial.println("Relay is ON");
        digitalWrite(RELAY_PIN, HIGH);  // Turn on the relay (LED)
      } else {
        relayState = false;
        Serial.println("Relay is OFF");
        digitalWrite(RELAY_PIN, LOW);   // Turn off the relay (LED)
      }

      // Write the temperature and relay status to the serial monitor
      serialOutput = tempString + "<br>Relay is " + (relayState ? "ON" : "OFF");
    }
  }

  delay(2000);
}
