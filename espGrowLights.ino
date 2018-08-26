#include <Arduino.h>
#include <ArduinoOTA.h>
#include <CircularBuffer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <fauxmoESP.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include "config.h"

fauxmoESP fauxmo;
ESP8266WebServer server(80);

time_t lastButtonToggle = 0;

// Buffer to hold log entires for web page
CircularBuffer<String, 50> buffer;

// -----------------------------------------------------------------------------
// NTP
// -----------------------------------------------------------------------------
unsigned int localPort = 8888;      // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48;     // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets
WiFiUDP Udp;
time_t prevDisplay = 0;             // when the digital clock was displayed

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
void setup() {
  // LED
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  // Grow lights
  pinMode(TOP_GROWLIGHT, OUTPUT);
  digitalWrite(TOP_GROWLIGHT, HIGH);
  pinMode(BOTTOM_GROWLIGHT, OUTPUT);
  digitalWrite(BOTTOM_GROWLIGHT, HIGH);

  // Button
  pinMode(BUTTON, INPUT_PULLUP);

  setupSerial();
  setupWifi();
  setupOTA();
  setupFauxmo();
  setupWebserver();
  setupNTP();
}

// -----------------------------------------------------------------------------
// Loop
// -----------------------------------------------------------------------------
void loop() {
  ArduinoOTA.handle();
  fauxmo.handle();

  static unsigned long last = millis();
  if (millis() - last > 5000) {
    last = millis();
    // Serial.printf("[MAIN] Free heap: %d bytes\n", ESP.getFreeHeap());
  }

  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
//      Serial.println(timeString());
    }
  }

  handleButtonPush();
  growLightAutoOnOff();
  server.handleClient();
}
// -----------------------------------------------------------------------------
// espGrowLights functions
// -----------------------------------------------------------------------------
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

void growLightAutoOnOff() {
  // Turn the grow lights on and off at specified times
  if (hour() == lightOnHour && minute() == 0 && second() == 0) {
    Serial.print("It's ");
    Serial.print(timeString());
    Serial.println(" turning on the grow lights.");
    buffer.unshift("It's " + timeString() + " turning on the grow lights.");
    setGrowLightState(true);
  } else if (hour() == lightOffHour && minute() == 0 && second() == 0) {
    Serial.print("It's ");
    Serial.print(timeString());
    Serial.println(" turning off the grow lights.");
    buffer.unshift("It's " + timeString() + " turning off the grow lights.");
    setGrowLightState(false);
  }
}

void handleButtonPush() {
  int buttonState = digitalRead(BUTTON);

  // check if the pushbutton is pressed. If it is, the buttonState is LOW.
  // toggle ligths if time has changed since last press
  if (buttonState == LOW) {
    if (now() != lastButtonToggle && (now()-1) != lastButtonToggle && (now()-2) != lastButtonToggle) {
      lastButtonToggle = now();
      bool currentState = getGrowLightState();
      setGrowLightState(!currentState);
      Serial.printf("[GROWLIGHT] Toggling grow lights %s at ", !currentState ? "on" : "off");
      Serial.println(timeString());
      String onOff = !currentState ? "on" : "off";
      buffer.unshift("[GROWLIGHT] Toggling grow lights " + onOff + " at " + timeString());
    }
  }
}

// -----------------------------------------------------------------------------
// Setup functions called in setup()
// -----------------------------------------------------------------------------
void setupFauxmo() {
  // Disabling fauxmo will prevent the devices from being discovered and switched
  fauxmo.enable(true);

  // Virtual Wemo devices
  fauxmo.addDevice(LED_DEVICE_NAME);
  fauxmo.addDevice(DEVICE_NAME);

  fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state) {
    Serial.printf("[FAUXMO] Device #%d (%s) state: %s\n", device_id, device_name, state ? "ON" : "OFF");
    String onOff = state ? "ON" : "OFF";
    buffer.unshift("[FAUXMO] Device #" + String(device_id) + " (" + device_name + ") state: "+ onOff + String(" at ") + timeString());
    switch (device_id) {
      case 0:
        digitalWrite(LED, !state);
        break;
      case 1:
        setGrowLightState(state);
        break;
      default:
        Serial.printf("[FAUXMO] Device #%d (%s) not found in switch case statement\n", device_id, device_name);
        buffer.unshift("[FAUXMO] Device #" + String(device_id) + " (" + device_name + ") not found in switch case statement");
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
        Serial.printf("[FAUXMO] Device #%d (%s) not found in switch case statement\n", device_id, device_name);
        buffer.unshift("[FAUXMO] Device #" + String(device_id) + " (" + device_name + ") not found in switch case statement");
        break;
    }
  });
}

void setupNTP() {
  Udp.begin(localPort);
  Serial.print("[NTP] Local port: ");
  Serial.println(Udp.localPort());
  buffer.unshift("[NTP] Local port: " + String(Udp.localPort()));
  Serial.println("[NTP] waiting for sync");
  buffer.unshift("[NTP] waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
  Serial.print("[NTP] It's currently ");
  Serial.println(timeString());
  buffer.unshift("[NTP] It's currently " + timeString());
}

void setupOTA() {
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(HOST_NAME);

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

void setupSerial() {
  // Init serial port and clean garbage
  Serial.begin(SERIAL_BAUDRATE);
  Serial.println();
  Serial.println();
}

void setupWebserver() {
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("[SETUP] HTTP server started");
  buffer.unshift("[SETUP] HTTP server started");

  if (MDNS.begin(HOST_NAME)) {
    Serial.println("[SETUP] MDNS responder started");
    buffer.unshift("[SETUP] MDNS responder started");
  }
}

void setupWifi() {

  // Set WIFI module to STA mode
  WiFi.mode(WIFI_STA);

  // Connect
  Serial.printf("[WIFI] Connecting to %s ", WIFI_SSID);
  buffer.unshift("[WIFI] Connecting to " + String(WIFI_SSID));
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Wait
  String dots = "[WIFI] ";
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    dots += ".";
    delay(100);
  }
  Serial.println();
  buffer.unshift(dots);

  // Connected!
  Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  buffer.unshift("[WIFI] STATION Mode, SSID: " + WiFi.SSID() + ", IP address: " + WiFi.localIP().toString().c_str());
}

// -----------------------------------------------------------------------------
// Webserver functions
// -----------------------------------------------------------------------------
void handleRoot() {
  String html = "<!DOCTYPE html>";
  html += "<html>";
  html += "<meta http-equiv='refresh' content='30'/>";
  html += "<title>" + String(HOST_NAME) + "</title>";
  html += "<style>";
  html += "  body {";
  html += "    background-color: #002b36;";
  html += "    color: white;";
  html += "  }";
  html += "  pre {";
  html += "    font-size: large;";
  html += "    width: 100%;";
  html += "    display: inline-block;";
  html += "  }";
  html += "</style>";
  html += "<body>";
  html += "  <h1>" + String(HOST_NAME) + "</h1>";
  html += "  <pre>";
  for (unsigned int i = 0; i < buffer.size(); i++) {
    html += "    " + buffer[i] + "<br />";
  }
  html += "  </pre>";
  html += "</body>";
  html += "</html>";
  server.send(200, "text/html", html);
}

void handleNotFound() {
  String message = "404... couldn't find\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

// -----------------------------------------------------------------------------
// NTP functions
// -----------------------------------------------------------------------------
String timeString() {
  const String timeSep = ":";
  const String space = " ";
  String theTime = monthShortStr(month()) + space + day() + space + year() + space + hourFormat12() + timeSep;
  if (minute() < 10) {
    theTime += "0";
  }
  theTime += minute() + timeSep;
  if (second() < 10) {
  theTime += "0";
  }
  theTime += second();
  if (isAM()) {
    theTime += " a.m";
  } else {
    theTime += " p.m";
  }
  return theTime;
}

time_t getNtpTime() {
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("[NTP] Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print("[NTP] ");
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("[NTP] Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("[NTP] No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;            // Stratum, or type of clock
  packetBuffer[2] = 6;            // Polling Interval
  packetBuffer[3] = 0xEC;         // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
