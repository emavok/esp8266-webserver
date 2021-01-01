// ------------------------------------------------------------------------------------------------
/*
  ESP8266 WebServer example with

  * connect to existing wlan
  * mDNS responder setup for http://esp.local
  * serving files from SPIFFS file system

*/
// ------------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>

// WLAN settings it should try to connect to
const char *ssid = "MyNetworkSSID";
const char *password = "myNetworkPassword";

// Hostname for mDNS - note that .local will be added automatically
const char *hostname = "myeps";

// pins for controlling a red and a green LED
#define RED_PIN D6
#define GREEN_PIN D4

// web server port - 80 is http
const int port = 80;

// Create a async http server
AsyncWebServer server(port);

// ------------------------------------------------------------------------------------------------
/**
 * A simple function that returns template parameter values
 * @param var Reference to the template parameter name
 * @return String that should replace the template parameter
 * @note The AsyncWebServer supports template parameters using %PARAMETER% syntax
 */
// ------------------------------------------------------------------------------------------------
String processor(const String &var)
{
    Serial.print("Getting template parameter ");
    Serial.print(var);
    String value;

    if (var == "IP")
    {
        IPAddress ip = WiFi.localIP();
        value = ip.toString();
    }
    else if (var == "HOSTNAME")
    {
        value = WiFi.hostname();
    }
    else if (var == "GREEN_LED_STATE")
    {
        value = digitalRead(GREEN_PIN) ? "ON" : "OFF";
    }
    else if (var == "RED_LED_STATE")
    {
        value = digitalRead(RED_PIN) ? "ON" : "OFF";
    }

    Serial.print(" = ");
    Serial.println(value);
    return value;
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

    // Connect to WiFi network
    Serial.print("Connecting to wlan with ssid ");
    Serial.print(ssid);
    Serial.print("...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("ok.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    Serial.print("Starting up HTTP server...");

    // Setup HTTP server callback
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", String(), false, processor);
    });
    server.on("/api/led/red", HTTP_POST, [](AsyncWebServerRequest *request) {
        const bool isOn = digitalRead(RED_PIN);
        digitalWrite(RED_PIN, isOn ? LOW : HIGH );
        request->send(SPIFFS, "/index.html", String(), false, processor);
    });
    server.on("/api/led/green", HTTP_POST, [](AsyncWebServerRequest *request) {
        const bool isOn = digitalRead(GREEN_PIN);
        digitalWrite(GREEN_PIN, isOn ? LOW : HIGH );
        request->send(SPIFFS, "/index.html", String(), false, processor);
    });

    // Start HTTP server
    server.begin();
    Serial.println("ok.");

    // Set up mDNS responder:
    // - first argument is the domain name, in this example
    //   the fully-qualified domain name is "esp8266.local"
    // - second argument is the IP address to advertise
    //   we send our IP address on the WiFi network
    Serial.print("Setting up mDNS with hostename ");
    Serial.print(hostname);
    Serial.print("...");
    if (!MDNS.begin(hostname))
    {
        Serial.println("Error setting up MDNS responder!");
        while (1)
        {
            delay(1000);
        }
    }
    Serial.println("ok. mDNS responder started");

    // Add service to MDNS-SD
    MDNS.addService("http", "tcp", 80);

    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, HIGH);
}

// ------------------------------------------------------------------------------------------------
// Main loop function
// ------------------------------------------------------------------------------------------------
void loop(void)
{
    MDNS.update();
}
