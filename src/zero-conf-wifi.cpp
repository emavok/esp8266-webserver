// ------------------------------------------------------------------------------------------------
// Wrapper to simplify wifi access
// ------------------------------------------------------------------------------------------------

#include "zero-conf-wifi.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

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
ZeroConfWifi::ZeroConfWifi() : m_aWebServer( DEFAULT_HTTP_PORT ) {
    // initialize ip addresses
    m_ipAP.fromString(DEFAULT_AP_IP);
    m_ipGateway.fromString(DEFAULT_GW_IP);
    m_ipSubnet.fromString(DEFAULT_SUBNET);
    m_pWebSocket = new AsyncWebSocket("/ws");
}

// ------------------------------------------------------------------------------------------------
/** Destructor */
// ------------------------------------------------------------------------------------------------
ZeroConfWifi::~ZeroConfWifi() {
    delete m_pWebSocket;
}

// ------------------------------------------------------------------------------------------------
/** Saves current configuration on LittleFS
 * @return True on success, false otherwise
 */
// ------------------------------------------------------------------------------------------------
bool ZeroConfWifi::saveConfig() {
    Serial.print("Saving wlan config to ");
    Serial.print(m_sConfigFileName);
    Serial.print("...");

    File file = LittleFS.open(m_sConfigFileName, "w");
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
/** Loads configuration on LittleFS
 * @return True on success, false otherwise
 */
// ------------------------------------------------------------------------------------------------
bool ZeroConfWifi::loadConfig() {
    Serial.print("Reading wlan config from ");
    Serial.print(m_sConfigFileName);
    Serial.print("...");

    // try to open file for reading
    File file = LittleFS.open(m_sConfigFileName, "r");
    if (!file) {
        Serial.println("error. Could not open file for reading.");
        return 1;
    }

    // read content as string
    String jsonStr = file.readString();

    // close file handle
    file.close();

    Serial.println("ok.");
    Serial.print("Parsing content...");

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

    Serial.println("ok.");
    return true;
}

// ------------------------------------------------------------------------------------------------
/** Resets configuration on LittleFS to default values
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
    if (LittleFS.remove(m_sConfigFileName)) {
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
        // if successful start DNS service
        return startDNS();
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

    // start mDNS service
    if (!MDNS.begin(m_sHostname))
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
/** Starts the DNS server
 * @return True on success, false otherwise
 */
// ------------------------------------------------------------------------------------------------
bool ZeroConfWifi::startDNS() {
    Serial.print("Starting DNS server...");

    // start mDNS service
    if (!m_aDnsServer.start( DEFAULT_DNS_PORT, "*", m_ipAP ))
    {
        Serial.println("failed.");
        return false;
    }
    Serial.println("ok.");
    return true;
}

// ------------------------------------------------------------------------------------------------
/** Performs update tasks - call in loop function
 * @return True on success, false otherwise
 */
// ------------------------------------------------------------------------------------------------
void ZeroConfWifi::update() {
    if (m_eActiveMode == E_MODE_AP) {
        m_aDnsServer.processNextRequest();
    } else if (m_eActiveMode == E_MODE_STA) {
        MDNS.update();
    }
    if (m_tRebootAt != 0 && (sys_now() > m_tRebootAt)) {
        Serial.println("Stopping wifi...");
        if (m_eActiveMode == E_MODE_AP) {
            WiFi.softAPdisconnect(true);
        } else if (m_eActiveMode == E_MODE_STA) {
            WiFi.disconnect(true);
        }
        delay(500);
        Serial.println("Restarting ESP8266...");
        ESP.restart();
    }
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

// ------------------------------------------------------------------------------------------------
/** Handles a POST request to save updated config
 * @return True on success, false otherwise
 */
// ------------------------------------------------------------------------------------------------
void ZeroConfWifi::handleUpdateConfigRequest(AsyncWebServerRequest *request) {
    Serial.println("Received POST /www-ap/update");
    Serial.println("Parsing parameters...");
    size_t i;
    for (i=0; i<request->args(); ++i) {
        if (request->argName(i) == "ssid") {
            m_sSSID = request->arg(i);
            Serial.println("ssid ok");
        } else if (request->argName(i) == "password") {
            m_sPassword = request->arg(i);
            Serial.println("password ok");
        } else if (request->argName(i) == "hostname") {
            m_sHostname = request->arg(i);
            Serial.println("hostname ok");
        } else {
            Serial.print("Unknown parameter ");
            Serial.println(request->argName(i));
        }
    }

    // store updated config
    saveConfig();

    Serial.println("Redirecting to /www-ap/reboot.html...");
    request->redirect("/www-ap/reboot.html");

    Serial.println("Schedule ESP restart in 5 sec...");
    // schedule a reboot in 5 sec
    scheduleReboot(5000);
}

// ------------------------------------------------------------------------------------------------
/** Handles a not found request in AP mode
 * @return True on success, false otherwise
 */
// ------------------------------------------------------------------------------------------------
void ZeroConfWifi::handleAPNotFoundRequest(AsyncWebServerRequest *request) {
    const String hostnameWithDotLocal = m_sHostname + ".local";
    // if in AP mode and request does not correct target host
    if (WiFi.localIP() != request->client()->localIP()
        &&
        request->host() != m_sHostname
    ) {
        // redirect to correct host
        String sURL = "http://" + hostnameWithDotLocal;
        // sURL += '/';
        Serial.print("Unsupported host name. Redirecting from '");
        Serial.print(request->host());
        Serial.print("' to '");
        Serial.print(hostnameWithDotLocal);
        Serial.print("' (-> ");
        Serial.print(sURL);
        Serial.println(")");
        request->redirect(sURL);
    } else {
        // just print an info
        Serial.print("404 ressource not found: ");
        Serial.println(request->url());
        request->send(404);
    }
}

bool filterIsHtmlAndModeAP(AsyncWebServerRequest *request) {
    const bool canHandle = ON_AP_FILTER(request)
        && request->url().length() > 5
        && request->url().substring(request->url().length() - 5) == ".html"
    ;
    Serial.print("filterIsHtmlAndModeAP: ");
    Serial.print(request->url());
    Serial.print(" -> ");
    Serial.println(canHandle ? "OK" : "pass");
    return canHandle;
}

bool filterIsModeAP(AsyncWebServerRequest *request) {
    const bool canHandle = ON_AP_FILTER(request);
    Serial.print("filterIsModeAP: ");
    Serial.print(request->url());
    Serial.print(" -> ");
    Serial.println(canHandle ? "OK" : "pass");
    return canHandle;
}

// ------------------------------------------------------------------------------------------------
/** Starts the web server
 * @return True on success, false otherwise
 */
// ------------------------------------------------------------------------------------------------
bool ZeroConfWifi::startWebServer() {
    Serial.print("Starting up HTTP server...");

    m_aWebServer
        .on("/", HTTP_GET,  [](AsyncWebServerRequest *request) {
            Serial.println("GET / --> redirecting to GET /www-ap/");
            request->redirect("/www-ap/");
        })
        .setFilter(ON_AP_FILTER);

    m_aWebServer
        .on("/www-ap/", HTTP_GET,  [](AsyncWebServerRequest *request) {
            Serial.println("GET /www-ap/ --> redirecting to GET /www-ap/index.html");
            request->redirect("/www-ap/index.html");
        })
        .setFilter(ON_AP_FILTER);

    m_aWebServer.onNotFound(
        std::bind(&ZeroConfWifi::handleAPNotFoundRequest, this, std::placeholders::_1)
    );

    m_aWebServer
        .serveStatic("/www-ap/", LittleFS, "/www-ap/")
        .setTemplateProcessor(
            std::bind(&ZeroConfWifi::processor, this, std::placeholders::_1)
        )
        .setFilter(filterIsHtmlAndModeAP);

    m_aWebServer
        .serveStatic("/www-ap/", LittleFS, "/www-ap/")
        .setFilter(filterIsModeAP);

    m_aWebServer
        .on("/www-ap/save-config", HTTP_POST,
            std::bind(&ZeroConfWifi::handleUpdateConfigRequest, this, std::placeholders::_1)
        );

    // attach web wocket
    m_aWebServer.addHandler(m_pWebSocket);

    // Start HTTP server
    m_aWebServer.begin();
    Serial.println("ok.");

    return true;
}

// ------------------------------------------------------------------------------------------------
/** Schedules a reboot in x milliseconds
 * @param timeout Delay in milliseconds before reboot
 */
// ------------------------------------------------------------------------------------------------
void ZeroConfWifi::scheduleReboot( uint64_t timeout ) {
    m_tRebootAt = sys_now() + timeout;
}

// eof
