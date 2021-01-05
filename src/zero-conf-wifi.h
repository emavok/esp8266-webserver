// ------------------------------------------------------------------------------------------------
/** Zero Config ESP8266 wifi setup */
// ------------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <WiFiClient.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

// default ssid of network to connect to
#define DEFAULT_SSID "my-wlan-ssid"
// default password of network to connect to
#define DEFAULT_PASSWORD "my-wlan-password"
// default hostname (without .local)
#define DEFAULT_HOSTNAME "esp8266"
// maximum timeout in seconds to wait for successful wlan connection
#define DEFAULT_TIMEOUT 10

// default access point ip when spawning a network
#define DEFAULT_AP_IP "8.8.8.8"
// default gatewas ip when spawning a network
#define DEFAULT_GW_IP "8.8.8.8"
// default ip net mask when spawning a network
#define DEFAULT_SUBNET "255.255.255.0"
// default network name when spawning a network
#define DEFAULT_NETNAME "esp8266-net"

//! default web server port
#define DEFAULT_HTTP_PORT 80

// default SPIFFS wlan config file name
#define DEFAULT_WLAN_CONFIG_FILENAME "/config/wlan.cfg"

// ------------------------------------------------------------------------------------------------
/** Active wifi modes */
// ------------------------------------------------------------------------------------------------
enum EActiveMode {
    //! wifi inactive
    E_MODE_IDLE = 0,
    //! wifi connected to existing wlan (station mode)
    E_MODE_STA = 1,
    //! wifi acting as access point (ap mode)
    E_MODE_AP = 2
};

// ------------------------------------------------------------------------------------------------
/** Main Zero Config wifi class */
// ------------------------------------------------------------------------------------------------
class ZeroConfWifi {

    public:

        //! Wifi network name to connect to
        String m_sSSID = DEFAULT_SSID;
        //! Wifi network password to connect to
        String m_sPassword = DEFAULT_PASSWORD;
        //! mDNS name this ESP8266 should use (.local will be added automatically)
        String m_sHostname = DEFAULT_HOSTNAME;
        //! Wifi network name used when acting as access point
        String m_sNetName = DEFAULT_NETNAME;

        //! IP address when acting as access point
        IPAddress m_ipAP;
        //! IP address of gateway when acting as access point
        IPAddress m_ipGateway;
        //! IP net mask when acting as access point
        IPAddress m_ipSubnet;

        //! File name in SPIFFS for saving wlan configuration
        String m_sConfigFileName = DEFAULT_WLAN_CONFIG_FILENAME;

        //! active wlan mode
        EActiveMode m_eActiveMode = E_MODE_IDLE;

        //! web server
        AsyncWebServer m_aWebServer;

        //! timestamp when it should be rebootet
        uint_least64_t m_tRebootAt = 0;

        // ----------------------------------------------------------------------------------------
        /** Processor function for configuration website
         * @param parameter Template parameter name
         * @return Template parameter value
         */
        // ----------------------------------------------------------------------------------------
        String processor(const String& parameter);

        // ----------------------------------------------------------------------------------------
        /** Handles a POST request to save updated config
         * @return True on success, false otherwise
         */
        // ----------------------------------------------------------------------------------------
        void handleUpdateConfigRequest(AsyncWebServerRequest *request);

        // ----------------------------------------------------------------------------------------
        /** Schedules a reboot in x milliseconds
         * @param timeout Delay in milliseconds before reboot
         */
        // ----------------------------------------------------------------------------------------
        void scheduleReboot( uint64_t timeout );

    // --------------------------------------------------------------------------------------------
    /** Constructor */
    // --------------------------------------------------------------------------------------------
    ZeroConfWifi();

    // --------------------------------------------------------------------------------------------
    /** Destructor */
    // --------------------------------------------------------------------------------------------
    ~ZeroConfWifi();

    public:

        // ----------------------------------------------------------------------------------------
        /** Saves current configuration on SPIFFS
         * @return True on success, false otherwise
         */
        // ----------------------------------------------------------------------------------------
        bool    saveConfig();

        // ----------------------------------------------------------------------------------------
        /** Loads configuration on SPIFFS
         * @return True on success, false otherwise
         */
        // ----------------------------------------------------------------------------------------
        bool    loadConfig();

        // ----------------------------------------------------------------------------------------
        /** Resets configuration on SPIFFS to default values
         * @return True on success, false otherwise
         */
        // ----------------------------------------------------------------------------------------
        bool    resetConfig();

        // ----------------------------------------------------------------------------------------
        /** Starts the wifi connection - either connects to existing network or spawns an own network
         * @return True on success, false otherwise
         */
        // ----------------------------------------------------------------------------------------
        bool    start();

        // ----------------------------------------------------------------------------------------
        /** Connects to an existing network
         * @return True on success, false otherwise
         */
        // ----------------------------------------------------------------------------------------
        bool    startSTA();

        // ----------------------------------------------------------------------------------------
        /** Spawns its own network as access point
         * @return True on success, false otherwise
         */
        // ----------------------------------------------------------------------------------------
        bool    startAP();

        // ----------------------------------------------------------------------------------------
        /** Starts the mDNS responder
         * @return True on success, false otherwise
         */
        // ----------------------------------------------------------------------------------------
        bool    startMDNS();

        // ----------------------------------------------------------------------------------------
        /** Starts the web server
         * @return True on success, false otherwise
         */
        // ----------------------------------------------------------------------------------------
        bool    startWebServer();

        // ----------------------------------------------------------------------------------------
        /** Performs update tasks - call in loop function
         * @return True on success, false otherwise
         */
        // ----------------------------------------------------------------------------------------
        void    update();
};
