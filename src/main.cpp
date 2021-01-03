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

#define DEFAULT_SSID "MyNetworkSSID"
#define DEFAULT_PASSWORD "myNetworkPassword"
#define DEFAULT_HOSTNAME "myesp"

// WLAN settings it should try to connect to
String ssid = DEFAULT_SSID;
String password = DEFAULT_PASSWORD;
// Hostname for mDNS - note that .local will be added automatically
String hostname = DEFAULT_HOSTNAME;

// AP settings
const char *ap_ssid = "esp8266-network";
IPAddress ap_ip(192,168,111,1);
IPAddress ap_gateway(192,168,111,0);
IPAddress ap_subnet(255,255,255,0);

// wlan config file name in SPIFFS
const char *wlanConfigFileName = "wlan.cfg";

// pins for controlling a red and a green LED
#define RED_PIN D6
#define GREEN_PIN D7

// web server port - 80 is http
const int port = 80;

#define MODE_UNSET 0
#define MODE_AP 1
#define MODE_STA 2
uint8 wifiMode = MODE_UNSET;

// Create a async http server
AsyncWebServer server(port);

// ------------------------------------------------------------------------------------------------
// Save ssid, password and hostname to spiffs file
// ------------------------------------------------------------------------------------------------
int saveWlanConfig() {
    Serial.print("Saving wlan config to ");
    Serial.print(wlanConfigFileName);
    Serial.print("...");

    File file = SPIFFS.open(wlanConfigFileName, "w");
    if (!file) {
        Serial.println("error. Could not open file for writing.");
        return 1;
    }

    file.print("ssid=");
    file.println(ssid);
    file.print("password=");
    file.println(password);
    file.print("hostname=");
    file.println(hostname);

    file.close();
    Serial.print("ok. ");
    return 0;
}

// ------------------------------------------------------------------------------------------------
// Resets the wlan config by deleting the config spiffs file
// ------------------------------------------------------------------------------------------------
int resetWlanConfig() {
    Serial.println("Resetting wlan config...");
    Serial.print("* Resetting values...");
    ssid = DEFAULT_SSID;
    password = DEFAULT_PASSWORD;
    hostname = DEFAULT_HOSTNAME;
    Serial.println("ok.");

    Serial.print("* Deleting wlan config file ");
    Serial.print(wlanConfigFileName);
    Serial.print("...");
    if (SPIFFS.remove(wlanConfigFileName)) {
        Serial.println("ok.");
        return 0;
    }
    Serial.println("error.");
    return 1;
}

// ------------------------------------------------------------------------------------------------
// Reads ssid, password and hostname from spiffs file
// ------------------------------------------------------------------------------------------------
int readWlanConfig() {
    Serial.print("Reading wlan config from ");
    Serial.print(wlanConfigFileName);
    Serial.print("...");

    ssid="MA48";
    password="sooK2102";
    hostname="myesp";
    return 0;

    bool ssidOk = false;
    bool pwOk = false;
    bool hnOk = false;

    File file = SPIFFS.open(wlanConfigFileName, "r");
    if (!file) {
        Serial.println("error. Could not open file for reading.");
        return 1;
    }

    // read file line-wise
    while(file.available()) {
        String line = file.readStringUntil('\n');
        Serial.println(line);

        // parse string
        Serial.print("-> parsing...");
        char str[line.length()+1];
        strcpy(str, line.c_str());
        char *token = strtok(str, "=");
        String tok = token;
        if (tok == "ssid") {
            ssid = strtok(NULL, "=");
            ssidOk = true;
            Serial.println("ok");
        }
        else if (tok == "password") {
            password = strtok(NULL, "=");
            pwOk = true;
            Serial.println("ok");
        } else if (tok == "hostname") {
            hostname = strtok(NULL, "=");
            hnOk = true;
            Serial.println("ok");
        } else {
            Serial.println("error. Unknown config parameter. Skipped.");
        }
    }
    // close file handle
    file.close();

    // check result
    if (ssidOk && pwOk && hnOk) {
        Serial.println("Reading config successful.");
        Serial.print("  ssid: ");
        Serial.println(ssid);
        Serial.print("  password: ");
        Serial.println(password);
        Serial.print("  hostname: ");
        Serial.println(hostname);
        return 0;
    }
    Serial.println("Error reading config:");
    if (!ssidOk) {
        Serial.println("Missing parameter ssid");
    }
    if (!pwOk) {
        Serial.println("Missing parameter password");
    }
    if (!hnOk) {
        Serial.println("Missing parameter hostname");
    }
    return 2;
}

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
    else if (var == "APIP")
    {
        IPAddress ip = WiFi.softAPIP();
        value = ip.toString();
    }
    else if (var == "SSID")
    {
        value = ssid;
    }
    else if (var == "PASSWORD")
    {
        value = password;
    }
    else if (var == "HOSTNAME")
    {
        // value = WiFi.hostname();
        value = hostname;
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
// mDNS setup
// ------------------------------------------------------------------------------------------------
void setupMDNS() {
    // Set up mDNS responder:
    // - first argument is the domain name, in this example
    //   the fully-qualified domain name is "esp8266.local"
    // - second argument is the IP address to advertise
    //   we send our IP address on the WiFi network
    Serial.println("Setting up mDNS...");
    if (wifiMode != MODE_STA) {
        Serial.println("* wifi not in STA mode.");
        Serial.println("* mDNS skipped.");
        return;
    }

    Serial.print("* hostename: ");
    Serial.println(hostname);
    Serial.print("* Starting up...");
    if (!MDNS.begin(hostname, ap_ip ))
    {
        Serial.println("error.");
        return;
    }
    Serial.println("ok.");
    Serial.print("* Adding HTTP service to mDNS...");

    // Add service to MDNS-SD
    MDNS.addService("http", "tcp", 80);
    Serial.println("ok.");
}

// ------------------------------------------------------------------------------------------------
// Setup WIFI
// ------------------------------------------------------------------------------------------------
void setupWifi() {
    // if (wifiMode != MODE_UNSET) {
    //     Serial.print("Shutting down wifi...");
    //     if (wifiMode == MODE_STA) {
    //         MDNS.close();
    //         WiFi.disconnect();
    //     }
    //     if (wifiMode == MODE_AP) {
    //         WiFi.softAPdisconnect();
    //     }
    //     wifiMode = MODE_UNSET;
    //     Serial.println("ok.");
    // }

    Serial.println("Setup wifi...");
    uint connectionRetries = 50;
    // try reading wlan config
    if (readWlanConfig() == 0) {
        // Connect to WiFi network
        Serial.println();
        Serial.println();
        Serial.print("* Connecting to wlan with ssid ");
        Serial.print(ssid);
        Serial.print("...");
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);

        // Wait for connection
        while (WiFi.status() != WL_CONNECTED && connectionRetries > 0)
        {
            delay(1000);
            Serial.print(".");
            --connectionRetries;
        }

        // successfully connected to wlan?
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("success.");
            Serial.print("* IP address: ");
            Serial.println(WiFi.localIP());
            wifiMode = MODE_STA;
            return;
        }
        // failed
        Serial.println("failed. Falling back to access point.");
    } else {
        Serial.println("* No wlan config found. Skipping connection.");
    }

    Serial.print("* Configuring access point...");
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAPConfig(ap_ip, ap_gateway, ap_subnet)) {
        Serial.println("failed.");
        return;
    }
    Serial.println("ok.");
    Serial.print("* Starting up access point...");
    if (!WiFi.softAP(ap_ssid)) {
        Serial.println("failed.");
        return;
    }
    Serial.println("ok.");
    Serial.print("* AP IP is: ");
    Serial.println(WiFi.softAPIP());
    wifiMode = MODE_AP;
}

// ------------------------------------------------------------------------------------------------
// Web server initialization
// ------------------------------------------------------------------------------------------------
void setupWebServer() {
    Serial.print("Starting up HTTP server...");

    // Setup HTTP for all modes
    server.on("/reboot.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Received GET /reboot - returning /reboot.html");
        request->send(SPIFFS, "/reboot.html", String(), false, processor);
    });

    // Setup HTTP server callback for AP mode
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Received GET / - returning /ap_index.html");
        request->send(SPIFFS, "/ap_index.html", String(), false, processor);
    }).setFilter(ON_AP_FILTER);
    server.on("/api/ap", HTTP_POST, [](AsyncWebServerRequest *request) {
        Serial.println("Received POST / - returning /ap_reboot.html");
        ssid = "MA48";
        password = "sooK2102";
        hostname = "myesp";
        saveWlanConfig();
        request->redirect("/reboot.html");
        // WiFi.softAPdisconnect();
        // setupWifi();
    }).setFilter(ON_AP_FILTER);

    // Setup HTTP server callback for STA mode
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Received GET / - returning /index.html");
        request->send(SPIFFS, "/index.html", String(), false, processor);
    }).setFilter(ON_STA_FILTER);
    server.on("/api/led/red", HTTP_POST, [](AsyncWebServerRequest *request) {
        Serial.println("Received POST /api/led/red - returning /index.html");
        const bool isOn = digitalRead(RED_PIN);
        digitalWrite(RED_PIN, isOn ? LOW : HIGH );
        request->send(SPIFFS, "/index.html", String(), false, processor);
    }).setFilter(ON_STA_FILTER);
    server.on("/api/led/green", HTTP_POST, [](AsyncWebServerRequest *request) {
        Serial.println("Received POST /api/led/green - returning /index.html");
        const bool isOn = digitalRead(GREEN_PIN);
        digitalWrite(GREEN_PIN, isOn ? LOW : HIGH );
        request->send(SPIFFS, "/index.html", String(), false, processor);
    }).setFilter(ON_STA_FILTER);
    server.on("/api/wlan-reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        Serial.println("Received POST /api/wlan-reset - returning /ap_reboot.html");
        resetWlanConfig();
        request->redirect("/reboot.html");
        // WiFi.disconnect();
        // setupWifi();
    }).setFilter(ON_STA_FILTER);

    // Start HTTP server
    server.begin();
    Serial.println("ok.");
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

    // setup wifi
    setupWifi();

    // setup web server
    setupWebServer();

    // setup mDNS
    setupMDNS();

    // set LED to green to indicate success
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
