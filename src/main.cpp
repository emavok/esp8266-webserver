// ------------------------------------------------------------------------------------------------
/*
  ESP8266 WebServer example with

  * connect to existing wlan
  * mDNS responder setup for http://esp.local
  * serving files from LittleFS file system

*/
// ------------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <ArduinoOTA.h>
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

// ------------------------------------------------------------------------------------------------
// WebSocket functions
// ------------------------------------------------------------------------------------------------
void onWsEventData(AsyncWebSocket * server, AsyncWebSocketClient * client, char *msg, size_t len){
    Serial.printf("ws[%s][%u] Message received: %s\n", server->url(), client->id(), msg);
    client->printf("Echo from server: %s", msg);
}

void onWsClientConnect(AsyncWebSocket * server, AsyncWebSocketClient * client){
    Serial.printf("ws[%s][%u] Client connected\n", server->url(), client->id());
    client->printf("Hello client, your id is %u", client->id());
}
void onWsClientDisconnect(AsyncWebSocket * server, AsyncWebSocketClient * client){
    Serial.printf("ws[%s][%u] Client disconnected\n", server->url(), client->id());
}
void onWsEventPong(AsyncWebSocket * server, AsyncWebSocketClient * client, char *msg, size_t len){
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? msg : "");
}
void onWsEventError(AsyncWebSocket * server, AsyncWebSocketClient * client, uint16_t errCode, char *errMsg, size_t errMsgLen) {
    Serial.printf("ws[%s][%u] ERROR %u: %s\n", server->url(), client->id(), errCode, errMsg);
}

void onWsEventRawData(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsFrameInfo *info, char *data, size_t len){
    if (info->final && info->index == 0 && info->len == len) {
        // whole message received in a single frame
        if (info->opcode != WS_TEXT) {
            Serial.printf("ws[%s][%u] ERROR: only text data allowed as web socket data\n", server->url(), client->id());
            return;
        }
        data[len] = 0;
        onWsEventData(server, client, data, len);
        return;
    }

    if(info->index == 0){
        if(info->num == 0) {
            Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        }
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
    }

    if (info->opcode != WS_TEXT) {
        Serial.printf("ws[%s][%u] ERROR: only text data allowed as web socket data\n", server->url(), client->id());
        return;
    }

    Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);
    // if(info->message_opcode == WS_TEXT){
    data[len] = 0;
    Serial.printf("%s\n", (char*)data);

    if((info->index + len) == info->len){
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if(info->final){
            Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        }
    }
}

void otaStartCallback() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
    else // U_FS
        type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
    if (zcWifi.m_pWebSocket->enabled()) {
        Serial.println("- shutting down web sockets");
        zcWifi.m_pWebSocket->enable(false);
        zcWifi.m_pWebSocket->textAll("OTA Update started");
        zcWifi.m_pWebSocket->closeAll();
    }
    Serial.println("- shutting down file system");
    LittleFS.end();
}
void otaEndCallback() {
    Serial.println("\nEnd");
}
void otaProgressCallback(unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
}
void otaErrorCallback(ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
};

// ------------------------------------------------------------------------------------------------
// OTA Config
// ------------------------------------------------------------------------------------------------
void setupOTA() {
    // Port defaults to 3232
    // ArduinoOTA.setPort(3232);

    // Hostname defaults to esp3232-[MAC]
    // ArduinoOTA.setHostname("myesp32");

    // No authentication by default
    // ArduinoOTA.setPassword("admin");

    // Password can be set with it's md5 value as well
    // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
    // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

    ArduinoOTA.setHostname(zcWifi.m_sHostname.c_str());
    ArduinoOTA.onStart(otaStartCallback);
    ArduinoOTA.onEnd(otaEndCallback);
    ArduinoOTA.onProgress(otaProgressCallback);
    ArduinoOTA.onError(otaErrorCallback);

    ArduinoOTA.begin();
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
    Serial.println("WS message received");
    //Handle WebSocket event
    switch (type) {
        case WS_EVT_CONNECT:
            onWsClientConnect(server, client);
            break;
        case WS_EVT_DISCONNECT:
            onWsClientDisconnect(server, client);
            break;
        case WS_EVT_PONG:
            onWsEventPong(server, client, (char*)data, len);
            break;
        case WS_EVT_DATA:
            onWsEventRawData(server, client, (AwsFrameInfo*) arg, (char*) data, len);
            break;
        case WS_EVT_ERROR:
            onWsEventError(server, client, *(uint16_t*) arg, (char*)data, len);
            break;
        default:
            Serial.printf("AsyncWebSocket: client %u triggered unknown event\n", client->id());
    }
}

// ------------------------------------------------------------------------------------------------
// Additional web server setup
// ------------------------------------------------------------------------------------------------
void setupWebServer() {
    zcWifi.m_aWebServer
        .serveStatic("/", LittleFS, "/www/")
        .setDefaultFile("index.html")
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
        request->redirect("/www-ap/reset.html");
    });

    // add web socket handler
    zcWifi.m_pWebSocket->onEvent(onWsEvent);
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

    // start ota
    setupOTA();

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
    ArduinoOTA.handle();
}
