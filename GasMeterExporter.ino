/*
  This sketch monitors a digital pin's state, and increments
  a counter for each falling edge.
  The counter is exposed via a web server as a prometheus metrics
  page at `/metrics`.

  I use this to monitor gas consumption, with a reed contact attached to my
  gas meter that sends a pulse every 0.01 cubic meters.
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266TimerInterrupt.h>
#include "wifisettings.h"

#define HTTP_PORT (80)
#define SERIAL_SPEED (115200)
#define PULSE_PIN (D1) /* D1 == (5) */

#define PROM_PREFIX "iot"
#define PROM_COUNTER "gas_consumption_cubic_meters_total"

#define SAMPLE_MS (50) /* sample reed contact every ,,, ms */
#define COUNTER_INCREMENT (0.010)

const char* response_template =
  "# HELP " PROM_PREFIX "_" PROM_COUNTER " Gas m^3 consumed.\n"
  "# TYPE " PROM_PREFIX "_" PROM_COUNTER " counter\n"
  PROM_PREFIX "_" PROM_COUNTER " %.2lf\n"
  "# HELP " PROM_PREFIX "_wifi_signal_rssi Wifi Signal RSSI.\n"
  "# TYPE " PROM_PREFIX "_wifi_signal_rssi gauge\n"
  PROM_PREFIX "_wifi_signal_rssi %d\n";

volatile double counter = 0;
ESP8266WebServer server(HTTP_PORT);
ESP8266Timer ITimer;

const char* ssid = WIFI_SSID;
const char* password = WIFI_PSK;

void setupCounter();
void setupWifi();
void setupHttpServer();
void checkCounterPinStateChange();
void checkPinStateChange(int newPinState);
void indexHtml();
void metrics();

void setup() {
  Serial.begin(SERIAL_SPEED);

  setupCounter();
  setupWifi();
  setupHttpServer();
}

void loop() {
  server.handleClient();
}

void setupCounter() {
  pinMode(PULSE_PIN, INPUT_PULLUP);
  if (!ITimer.attachInterruptInterval(1000 * SAMPLE_MS, checkCounterPinStateChange)) {
    Serial.println(F("ERR: Can't set ITimer correctly. Select another freq. or interval"));
  }
}

void setupWifi() {
  Serial.println();
  Serial.print(F("INF: Connecting to WiFi: "));
  Serial.println(ssid);

  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  if (!WiFi.mode(WIFI_STA)) {
    Serial.println(F("ERR: Could not set STA mode."));
  }
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }

  Serial.println();
  Serial.println(F("INF: WiFi connected"));
  Serial.println(F("INF: IP address: "));
  Serial.println(WiFi.localIP());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}

void handleRisingEdge() {
  Serial.println(F("DEB: Rising edge detected."));
}

void handleFallingEdge() {
  Serial.println(F("DEB: Falling edge detected."));
  counter += COUNTER_INCREMENT;
}

void checkCounterPinStateChange() {
  checkPinStateChange(digitalRead(PULSE_PIN));
}

void checkPinStateChange(int newPinState) {
  static volatile int pinState = HIGH;

  if (newPinState == pinState) {
    return;
  }

  pinState = newPinState;
  switch (pinState) {
    case HIGH:
      handleRisingEdge();
      break;
    case LOW:
      handleFallingEdge();
      break;
    default:
      /* do I smell fish? */
      break;
  }
}

void setupHttpServer() {
  server.on("/", indexHtml);
  server.on("/metrics", metrics);
  server.begin();

  Serial.print(F("INF: HTTP Server listening at http://"));
  Serial.print(WiFi.localIP());
  Serial.print(F(":"));
  Serial.print(HTTP_PORT);
  Serial.println(F("/"));
}

void indexHtml() {
  server.send(200, F("text/html"), F("<html><body><a href=\"/metrics\">/metrics</a></body></html>"));
}

void metrics() {
  const size_t BUFSIZE = 1024;

  int rssi = WiFi.RSSI();

  char response[BUFSIZE];
  snprintf(response, BUFSIZE, response_template, counter, rssi);
  server.send(200, F("text/plain;version=0.0.4;charset=utf-8"), response);
}
