#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include "fauxmoESP.h"
#include "credentials.h"

#define SERIAL_BAUDRATE     115200
#define LED                 LED_BUILTIN
#define TOP_GROWLIGHT       D5
#define BOTTOM_GROWLIGHT    D6

fauxmoESP fauxmo;
ESP8266WebServer server(80);

// -----------------------------------------------------------------------------
// Wifi
// -----------------------------------------------------------------------------

void wifiSetup() {

  // Set WIFI module to STA mode
  WiFi.mode(WIFI_STA);

  // Connect
  Serial.printf("[WIFI] Connecting to %s ", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Wait
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // Connected!
  Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());

}

void otaSetup() {
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("espGrowLights");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("[OTA] Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("[OTA] Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("[OTA] Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("[OTA] Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("[OTA] Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("[OTA] End Failed");
    ESP.restart();
  });
  ArduinoOTA.begin();
}

void setup() {

  // Init serial port and clean garbage
  Serial.begin(SERIAL_BAUDRATE);
  Serial.println();
  Serial.println();

  // Wifi
  wifiSetup();

  // ArduinoOTA
  otaSetup();

  // LED
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  // Grow lights
  pinMode(TOP_GROWLIGHT, OUTPUT);
  digitalWrite(TOP_GROWLIGHT, HIGH);
  pinMode(BOTTOM_GROWLIGHT, OUTPUT);
  digitalWrite(BOTTOM_GROWLIGHT, HIGH);


  // You have to call enable(true) once you have a WiFi connection
  // You can enable or disable the library at any moment
  // Disabling it will prevent the devices from being discovered and switched
  fauxmo.enable(true);

  // Add virtual devices
  fauxmo.addDevice("ESP LED");
  fauxmo.addDevice("Grow Lights");

  // fauxmoESP 2.0.0 has changed the callback signature to add the device_id,
  // this way it's easier to match devices to action without having to compare strings.
  fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state) {
    Serial.printf("[MAIN] Device #%d (%s) state: %s\n", device_id, device_name, state ? "ON" : "OFF");
    switch (device_id) {
      case 0:
        digitalWrite(LED, !state);
        break;
      case 1:
        setGrowLightState(state);
        break;
      default:
        Serial.printf("[MAIN] Device #%d (%s) not found in switch case statement\n", device_id, device_name);
        break;
    }
  });

  // Callback to retrieve current state (for GetBinaryState queries)
  fauxmo.onGetState([](unsigned char device_id, const char * device_name) {
    switch (device_id) {
      case 0:
        return !digitalRead(LED);
        break;
      case 1:
        return getGrowLightState();
        break;
      default:
        Serial.printf("[MAIN] Device #%d (%s) not found in switch case statement\n", device_id, device_name);
        break;
    }
  });

  if (MDNS.begin("espGrowLights")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  server.on("/inline", [](){
    server.send(200, "text/plain", "this works as well");
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

}

void setGrowLightState(bool state) {
  //    Serial.printf("[GROWLIGHT] Setting state to : %s\n", state ? "ON" : "OFF");
  digitalWrite(TOP_GROWLIGHT, !state);
  digitalWrite(BOTTOM_GROWLIGHT, !state);
}

bool getGrowLightState() {
  bool top_growlight_state = !digitalRead(TOP_GROWLIGHT);
  bool bottom_growlight_state = !digitalRead(BOTTOM_GROWLIGHT);
  bool state;
  if (top_growlight_state && bottom_growlight_state) {
    state = true;
  } else {
    state = false;
  }
  //    Serial.printf("[GROWLIGHT] Both grow lights are %s\n", state ? "on" : "off");
  return state;
}

void handleRoot() {
  server.send(200, "text/html", "<!DOCTYPE html><html><body><h1>Hello from espGrowLights!</h1></body></html>");
}

void handleNotFound(){
  String message = "404... couldn't find\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void loop() {
  ArduinoOTA.handle();

  // Since fauxmoESP 2.0 the library uses the "compatibility" mode by
  // default, this means that it uses WiFiUdp class instead of AsyncUDP.
  // The later requires the Arduino Core for ESP8266 staging version
  // whilst the former works fine with current stable 2.3.0 version.
  // But, since it's not "async" anymore we have to manually poll for UDP
  // packets
  fauxmo.handle();

  static unsigned long last = millis();
  if (millis() - last > 5000) {
    last = millis();
    //        Serial.printf("[MAIN] Free heap: %d bytes\n", ESP.getFreeHeap());
  }

  server.handleClient();

}
