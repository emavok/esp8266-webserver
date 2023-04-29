// ------------------------------------------------------------------------------------------------
// Wrapper to simplify wifi access
// ------------------------------------------------------------------------------------------------

#include "wifi-wrapper.h"

#include <LittleFS.h>

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
Wifi::Wifi() {
    // initialize ip addresses
    m_ipAP.fromString(DEFAULT_AP_IP);
    m_ipGateway.fromString(DEFAULT_GW_IP);
    m_ipSubnet.fromString(DEFAULT_SUBNET);
}

// ------------------------------------------------------------------------------------------------
/** Destructor */
// ------------------------------------------------------------------------------------------------
Wifi::~Wifi () {
}

// ------------------------------------------------------------------------------------------------
/** Saves current configuration on LittleFS
 * @return True on success, false otherwise
 */
// ------------------------------------------------------------------------------------------------
bool Wifi::saveConfig() {
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
bool Wifi::loadConfig() {
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
/** Resets configuration on LittleFS to default values
 * @return True on success, false otherwise
 */
// ------------------------------------------------------------------------------------------------
bool Wifi::resetConfig() {
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

