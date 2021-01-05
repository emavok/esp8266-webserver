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
// Function to return template parameter values
// ------------------------------------------------------------------------------------------------
String templateProcessor(const String &var) {
    String value = "%" + var + "%";
    if (var == "IP")
    {
        IPAddress ip = WiFi.localIP();
        value = ip.toString();
    } else if (var == "HOSTNAME") {
        value = zcWifi.m_sHostname;
    } else if (var == "GREEN_LED_STATE") {
        int greenValue = digitalRead(GREEN_PIN);
        value = greenValue;
    } else if (var == "RED_LED_STATE") {
        int redValue = digitalRead(RED_PIN);
        value = redValue;
    }
    return value;
}

// ------------------------------------------------------------------------------------------------
// Additional web server setup
// ------------------------------------------------------------------------------------------------
void setupWebServer() {
    zcWifi.m_aWebServer
        .serveStatic("/", SPIFFS, "/www/")
        .setDefaultFile("index.html")
        .setTemplateProcessor(templateProcessor)
        .setFilter(ON_STA_FILTER);

    zcWifi.m_aWebServer.on("/toggle-green", HTTP_POST, [](AsyncWebServerRequest *request) {
        int greenValue = digitalRead(GREEN_PIN);
        digitalWrite(GREEN_PIN, greenValue == 0 ? HIGH : LOW);
        request->redirect("/");
    });

    zcWifi.m_aWebServer.on("/toggle-red", HTTP_POST, [](AsyncWebServerRequest *request) {
        int greenValue = digitalRead(RED_PIN);
        digitalWrite(RED_PIN, greenValue == 0 ? HIGH : LOW);
        request->redirect("/");
    });

    zcWifi.m_aWebServer.on("/reset-config", HTTP_POST, [](AsyncWebServerRequest *request) {
        zcWifi.scheduleReboot(5000);
        zcWifi.resetConfig();
        request->redirect("/wifi/reset.html");
    });
}


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

    // setup additional webserver stuff
    setupWebServer();

    // start up wifi
    zcWifi.start();

    // setup web server
    zcWifi.startWebServer();

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
