// ------------------------------------------------------------------------------------------------
/*
  ESP8266 WebServer example with

  * connect to existing wlan
  * mDNS responder setup for http://esp.local
  * serving files from SPIFFS file system

*/
// ------------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <WiFiClient.h>

#include <FS.h>

#include "zero-conf-wifi.h"

// pins for controlling a red and a green LED
#define RED_PIN D6
#define GREEN_PIN D7

ZeroConfWifi zcWifi;

// ------------------------------------------------------------------------------------------------
// Setup function
// ------------------------------------------------------------------------------------------------
void setup(void)
{
    Serial.begin(115200);

    // enable GPIO pins
    Serial.println("Initializing GPIO pins...");
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    digitalWrite(RED_PIN, HIGH);
    digitalWrite(GREEN_PIN, LOW);

    // get access to SPIFFS file system
    Serial.println("Initializing SPIFFS file system...");
    if (!SPIFFS.begin())
    {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    zcWifi.start();

    // setup web server
    setupWebServer();

    // set LED to green to indicate success
    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, HIGH);
}

// ------------------------------------------------------------------------------------------------
// Main loop function
// ------------------------------------------------------------------------------------------------
void loop(void)
{
    zcWifi.update();
}
