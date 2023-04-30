// ------------------------------------------------------------------------------------------------
/*
  ESP8266 WebServer example with

  * connect to existing wlan
  * mDNS responder setup for http://esp.local
  * serving files from LittleFS file system

*/
// ------------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <WiFiClient.h>

#include <LittleFS.h>

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

bool filterIsHtmlAndModeSTA(AsyncWebServerRequest *request) {
    const bool canHandle = ON_STA_FILTER(request)
        // && request->url().length() > 5
        // && request->url().substring(request->url().length() - 5) == ".html"
    ;
    Serial.print("filterIsHtmlAndModeSTA: ");
    Serial.print(request->url());
    Serial.print(" -> ");
    Serial.println(canHandle ? "OK" : "pass");
    return canHandle;
}

// ------------------------------------------------------------------------------------------------
// Additional web server setup
// ------------------------------------------------------------------------------------------------
void setupWebServer() {
    // zcWifi.m_aWebServer
        // .rewrite("/", "/index.html");

    zcWifi.m_aWebServer
        .serveStatic("/", LittleFS, "/www/")
        .setDefaultFile("index.html")
        .setTemplateProcessor(templateProcessor)
        .setFilter(filterIsHtmlAndModeSTA);

    // zcWifi.m_aWebServer
    //     .serveStatic("/", LittleFS, "/www/")
    //     .setFilter(ON_STA_FILTER);

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
        request->redirect("/www-ap/reset.html");
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

    // get access to LittleFS file system
    Serial.println("Initializing LittleFS file system...");
    if (!LittleFS.begin())
    {
        Serial.println("An Error has occurred while mounting LittleFS");
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
