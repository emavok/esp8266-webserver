// ------------------------------------------------------------------------------------------------
// Wrapper to simplify wifi access
// ------------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <WiFiClient.h>

#define DEFAULT_SSID "my-wlan-ssid"
#define DEFAULT_PASSWORD "my-wlan-password"
#define DEFAULT_HOSTNAME "esp8266"
#define DEFAULT_NETNAME "esp8266-net"

#define DEFAULT_AP_IP "8.8.8.8"
#define DEFAULT_GW_IP "8.8.8.8"
#define DEFAULT_SUBNET "255.255.255.0"

#define DEFAULT_WLAN_CONFIG_FILENAME "/config/wlan.cfg"

// ------------------------------------------------------------------------------------------------
/** Class to perform relevant wifi tasks */
// ------------------------------------------------------------------------------------------------
class Wifi {

    protected:

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

    // --------------------------------------------------------------------------------------------
    /** Constructor */
    // --------------------------------------------------------------------------------------------
    Wifi();

    // --------------------------------------------------------------------------------------------
    /** Destructor */
    // --------------------------------------------------------------------------------------------
    ~Wifi ();

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
};
