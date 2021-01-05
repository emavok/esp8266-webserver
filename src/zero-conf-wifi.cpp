// ------------------------------------------------------------------------------------------------
// Wrapper to simplify wifi access
// ------------------------------------------------------------------------------------------------

#include "zero-conf-wifi.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>

#include <Hash.h>
#include <functional>

// Installing this see https://platformio.org/lib/show/64/ArduinoJson/installation
#include <ArduinoJson.h>

#define JSON_KEY_SSID "ssid"
#define JSON_KEY_PASSWORD "password"
#define JSON_KEY_HOSTNAME "hostname"
#define JSON_KEY_AP_NETWORK "ap-network"
#define JSON_KEY_AP_IP "ap-ip"
#define JSON_KEY_AP_GW "ap-gw"
#define JSON_KEY_AP_NETMASK "ap-netmask"

// ------------------------------------------------------------------------------------------------
/** Constructor */
// ------------------------------------------------------------------------------------------------
ZeroConfWifi::ZeroConfWifi()
: m_aWebServer( DEFAULT_HTTP_PORT )
{
    // initialize ip addresses
    m_ipAP.fromString(DEFAULT_AP_IP);
    m_ipGateway.fromString(DEFAULT_GW_IP);
    m_ipSubnet.fromString(DEFAULT_SUBNET);
}

// ------------------------------------------------------------------------------------------------
/** Destructor */
// ------------------------------------------------------------------------------------------------
ZeroConfWifi::~ZeroConfWifi() {
}

// ------------------------------------------------------------------------------------------------
/** Saves current configuration on SPIFFS
 * @return True on success, false otherwise
 */
// ------------------------------------------------------------------------------------------------
bool ZeroConfWifi::saveConfig() {
    Serial.print("Saving wlan config to ");
    Serial.print(m_sConfigFileName);
    Serial.print("...");

    File file = SPIFFS.open(m_sConfigFileName, "w");
    if (!file) {
        Serial.println("error. Could not open file for writing.");
        return false;
    }

    DynamicJsonDocument doc(1024);
    doc[JSON_KEY_SSID] = m_sSSID;
    doc[JSON_KEY_PASSWORD] = m_sPassword;
    doc[JSON_KEY_HOSTNAME] = m_sHostname;
    doc[JSON_KEY_AP_IP] = m_ipAP.toString();
    doc[JSON_KEY_AP_GW] = m_ipGateway.toString();
    doc[JSON_KEY_AP_NETMASK] = m_ipSubnet.toString();
    doc[JSON_KEY_AP_NETWORK] = m_sNetName;

    String jsonStr;
    serializeJson(doc, jsonStr);

    const size_t bytesWritten = file.write(jsonStr.c_str());
    file.close();
    Serial.print("ok. ");
    Serial.print(bytesWritten);
    Serial.println(" Bytes written.");
    return true;
}

// ------------------------------------------------------------------------------------------------
/** Loads configuration on SPIFFS
 * @return True on success, false otherwise
 */
// ------------------------------------------------------------------------------------------------
bool ZeroConfWifi::loadConfig() {
    Serial.print("Reading wlan config from ");
    Serial.print(m_sConfigFileName);
    Serial.print("...");

    // try to open file for reading
    File file = SPIFFS.open(m_sConfigFileName, "r");
    if (!file) {
        Serial.println("error. Could not open file for reading.");
        return 1;
    }

    // read content as string
    String jsonStr = file.readString();

    // close file handle
    file.close();

    // try parsing
    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, jsonStr);
    if (err != DeserializationError::Ok) {
        Serial.println("failed. A deserialization error occured.");
        Serial.println(err.f_str());
        return false;
    }

    //  copy inputs with fallback to defaults
    m_sSSID = doc[JSON_KEY_SSID] | DEFAULT_SSID;
    m_sPassword = doc[JSON_KEY_PASSWORD] | DEFAULT_PASSWORD;
    m_sHostname = doc[JSON_KEY_HOSTNAME] | DEFAULT_HOSTNAME;
    m_ipAP.fromString(doc[JSON_KEY_AP_IP] | DEFAULT_AP_IP);
    m_ipGateway.fromString(doc[JSON_KEY_AP_GW] | DEFAULT_GW_IP);
    m_ipSubnet.fromString(doc[JSON_KEY_AP_NETMASK] | DEFAULT_SUBNET);
    m_sNetName = doc[JSON_KEY_AP_NETWORK] | DEFAULT_NETNAME;

    return true;
}

// ------------------------------------------------------------------------------------------------
/** Resets configuration on SPIFFS to default values
 * @return True on success, false otherwise
 */
// ------------------------------------------------------------------------------------------------
bool ZeroConfWifi::resetConfig() {
    Serial.print("Resetting wlan config...");

    //  set inputs to default values
    m_sSSID = DEFAULT_SSID;
    m_sPassword = DEFAULT_PASSWORD;
    m_sHostname = DEFAULT_HOSTNAME;
    m_ipAP.fromString(DEFAULT_AP_IP);
    m_ipGateway.fromString(DEFAULT_GW_IP);
    m_ipSubnet.fromString(DEFAULT_SUBNET);
    m_sNetName = DEFAULT_NETNAME;

    // remove config file
    if (SPIFFS.remove(m_sConfigFileName)) {
        Serial.println("ok.");
        return true;
    }
    Serial.println("error. Failed to remove config file.");
    return false;
}

// ------------------------------------------------------------------------------------------------
/** Starts the wifi connection - either connects to existing network or spawns an own network
 * @return True on success, false otherwise
 */
// ------------------------------------------------------------------------------------------------
bool ZeroConfWifi::start() {
    // try loading wlan config
    if (loadConfig()) {
        // if successful try connecting to existing wlan
        if (startSTA()) {
            // if successful start mDNS
            return startMDNS();
        }
    }

    // fallback: start own network
    if (startAP()) {
        // if successful start mDNS service
        return startMDNS();
    }

    // all things failed
    return false;
}

// ------------------------------------------------------------------------------------------------
/** Connects to an existing network
 * @return True on success, false otherwise
 */
// ------------------------------------------------------------------------------------------------
bool ZeroConfWifi::startSTA() {
    Serial.println("Starting wifi in STA mode...");
    Serial.print("Attempting to connect to wlan with ssid ");
    Serial.print(m_sSSID);
    Serial.print("...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(m_sSSID, m_sPassword);

    // Wait for connection
    uint8 connectionRetries = DEFAULT_TIMEOUT;
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
        m_eActiveMode = E_MODE_STA;
        return true;
    }
    // failed
    Serial.println("failed.");
    return false;
}

// ------------------------------------------------------------------------------------------------
/** Spawns its own network as access point
 * @return True on success, false otherwise
 */
// ------------------------------------------------------------------------------------------------
bool ZeroConfWifi::startAP() {
    Serial.println("Starting wifi in AP mode...");
    Serial.print("Configuring access point...");
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAPConfig(m_ipAP, m_ipGateway, m_ipSubnet)) {
        Serial.println("failed.");
        return false;
    }
    Serial.println("ok.");
    Serial.print("Starting access point...");
    if (!WiFi.softAP(m_sNetName)) {
        Serial.println("failed.");
        return false;
    }
    Serial.println("ok.");
    Serial.print("AP IP is: ");
    Serial.println(WiFi.softAPIP());
    m_eActiveMode = E_MODE_AP;
    return true;
}

// ------------------------------------------------------------------------------------------------
/** Starts the mDNS responder
 * @return True on success, false otherwise
 */
// ------------------------------------------------------------------------------------------------
bool ZeroConfWifi::startMDNS() {
    Serial.print("Starting mDNS responder...");
    // if (m_eActiveMode != E_MODE_STA) {
    //     Serial.println("error. Wifi not in STA mode.");
    //     return false;
    // }

    // start mDNS service
    if (!MDNS.begin(m_sHostname, m_ipAP))
    {
        Serial.println("failed.");
        return false;
    }
    Serial.println("ok.");

    // add HTTP service to mDNS
    Serial.print("Adding HTTP service to mDNS...");
    if (!MDNS.addService("http", "tcp", 80)) {
        Serial.println("failed.");
        return false;
    }
    Serial.println("ok.");

    // debug info
    Serial.print("mDNS started with hostname: ");
    Serial.println(m_sHostname);
    return true;
}

// ------------------------------------------------------------------------------------------------
/** Performs update tasks - call in loop function
 * @return True on success, false otherwise
 */
// ------------------------------------------------------------------------------------------------
void ZeroConfWifi::update() {
    MDNS.update();
}

// ------------------------------------------------------------------------------------------------
/**
 * A simple function that returns template parameter values
 * @param var Reference to the template parameter name
 * @return String that should replace the template parameter
 * @note The AsyncWebServer supports template parameters using %PARAMETER% syntax
 */
// ------------------------------------------------------------------------------------------------
String ZeroConfWifi::processor(const String &var)
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
        value = m_sSSID;
    }
    else if (var == "PASSWORD")
    {
        value = m_sPassword;
    }
    else if (var == "HOSTNAME")
    {
        value = m_sHostname;
    }
    Serial.print(" = ");
    Serial.println(value);
    return value;
}

void ZeroConfWifi::handleAPRequestGetRoot(AsyncWebServerRequest *request) {
    Serial.println("Received GET / - returning /ap_index.html");
    request->send(SPIFFS, "/wifi/ap_index.html", String(), false,
        std::bind(&ZeroConfWifi::processor, this, std::placeholders::_1)
    );
}

// ------------------------------------------------------------------------------------------------
/** Starts the web server
 * @return True on success, false otherwise
 */
// ------------------------------------------------------------------------------------------------
bool ZeroConfWifi::startWebServer() {
    Serial.print("Starting up HTTP server...");

    // Setup HTTP server callback for AP mode
    m_aWebServer.on("/", HTTP_GET,
        std::bind(&ZeroConfWifi::handleAPRequestGetRoot, this, std::placeholders::_1)
    ).setFilter(ON_AP_FILTER);

    m_aWebServer.on("/api/ap", HTTP_POST,
        [](AsyncWebServerRequest * request){
            Serial.println("Received POST /api/ap");
            Serial.print("Args: ");
            Serial.println(request->args());
            size_t i;
            for (i=0; i<request->args(); ++i) {
                if (request->argName(i) == "ssid") {
                    ssid = request->arg(i);
                    Serial.println("ssid ok");
                } else if (request->argName(i) == "password") {
                    password = request->arg(i);
                    Serial.println("password ok");
                } else if (request->argName(i) == "hostname") {
                    hostname = request->arg(i);
                    Serial.println("hostname ok");
                } else {
                    Serial.print("Unknown parameter ");
                    Serial.println(request->argName(i));
                }
            }
            saveWlanConfig();
            request->redirect("/");
            delay(1);
            ESP.restart();
        });

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
        delay(1);
        ESP.restart();
    }).setFilter(ON_STA_FILTER);

    // Start HTTP server
    server.begin();
    Serial.println("ok.");
}

