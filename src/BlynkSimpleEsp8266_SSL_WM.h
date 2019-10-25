/**
 * @file       BlynkSimpleEsp8266_SSL.h
 * @author     Volodymyr Shymanskyy
 * @license    This project is released under the MIT License (MIT)
 * @copyright  Copyright (c) 2015 Volodymyr Shymanskyy
 * @date       Jan 2016
 * @brief
 *
 */

#ifndef BlynkSimpleEsp8266_SSL_WM_h
#define BlynkSimpleEsp8266_SSL_WM_h

#ifndef ESP8266
#error This code is intended to run on the ESP8266 platform! Please check your Tools->Board setting.
#endif

#include <version.h>

#if ESP_SDK_VERSION_NUMBER < 0x020200
#error Please update your ESP8266 Arduino Core
#endif

// Fingerprint is not used by default
//#define BLYNK_DEFAULT_FINGERPRINT "FD C0 7D 8D 47 97 F7 E3 07 05 D3 4E E3 BB 8E 3D C0 EA BE 1C"

#if defined(BLYNK_SSL_USE_LETSENCRYPT)
  static const unsigned char BLYNK_DEFAULT_CERT_DER[] PROGMEM =
  #include <certs/dst_der.h>  // TODO: using DST Root CA X3 for now
  //#include <certs/isrgroot_der.h>
  //#include <certs/letsencrypt_der.h>
#else
  static const unsigned char BLYNK_DEFAULT_CERT_DER[] PROGMEM =
  #include <certs/blynkcloud_der.h>
#endif

#include <BlynkApiArduino.h>
#include <Blynk/BlynkProtocol.h>
#include <Adapters/BlynkArduinoClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include <ESP8266WebServer.h>
#include <EEPROM.h>

template <typename Client>
class BlynkArduinoClientSecure
    : public BlynkArduinoClientGen<Client>
{
public:
    BlynkArduinoClientSecure(Client& client)
        : BlynkArduinoClientGen<Client>(client)
        , fingerprint(NULL)
    {}

    void setFingerprint(const char* fp) { fingerprint = fp; }

    bool setCACert(const uint8_t* caCert, unsigned caCertLen) {
        bool res = this->client->setCACert(caCert, caCertLen);
        if (!res) {
          BLYNK_LOG1("Failed to load root CA certificate!");
        }
        return res;
    }

    bool setCACert_P(const uint8_t* caCert, unsigned caCertLen) {
        bool res = this->client->setCACert_P(caCert, caCertLen);
        if (!res) {
          BLYNK_LOG1("Failed to load root CA certificate!");
        }
        return res;
    }

    bool connect() 
    {
        //KH
        if (this->connected())
          return true;
        
        // Synchronize time useing SNTP. This is necessary to verify that
        // the TLS certificates offered by the server are currently valid.
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        time_t now = time(nullptr);

        int i = 0;
        while ( (i++ < 30) && (now < 100000) ) {
          delay(500);
          now = time(nullptr);
        }
        
        struct tm timeinfo;
        gmtime_r(&now, &timeinfo);
        String ntpTime = asctime(&timeinfo);
        ntpTime.trim();
        BLYNK_LOG2("NTP time: ", ntpTime);

        // Now try connecting
        if (BlynkArduinoClientGen<Client>::connect()) 
        {
          if (fingerprint && this->client->verify(fingerprint, this->domain)) 
          {
              BLYNK_LOG1(BLYNK_F("Fingerprint OK"));
              return true;
          } 
          else if (this->client->verifyCertChain(this->domain)) 
          {
              BLYNK_LOG1(BLYNK_F("Certificate OK"));
              return true;
          }
          BLYNK_LOG1(BLYNK_F("Certificate not validated"));
          return false;
        }
        return false;
    }

private:
    const char* fingerprint;
};

// Configurable items besides fixed Header
#define NUM_CONFIGURABLE_ITEMS    6
struct Configuration 
{
    char header         [16];
    char wifi_ssid      [32];
    char wifi_passphrase[32];
    char blynk_server   [32];
    int  blynk_port;
    char blynk_token    [36];
    char board_name     [16];
};

String root_html_template = " \
<!DOCTYPE html> \
<meta name=\"robots\" content=\"noindex\"> \
<html> \
<head> \
  <meta charset=\"utf-8\"> \
  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"> \
  <title>BlynkSimpleEsp8266_SSL_WM</title> \
</head> \
<body> \
  <div align=\"center\"> \
    <table> \
      <tbody> \
        <tr> \
          <th colspan=\"2\">WiFi</th> \
        </tr> \
        <tr> \
          <td>SSID</td> \
          <td><input type=\"text\" value=\"[[wifi_ssid]]\" size=20 maxlength=64 id=\"wifi_ssid\"></td> \
        </tr> \
        <tr> \
          <td>Passphrase</td> \
          <td><input type=\"text\" value=\"[[wifi_passphrase]]\" size=20 maxlength=64 id=\"wifi_passphrase\"></td> \
        </tr> \
        <tr> \
          <th colspan=\"2\">Blynk</th> \
        </tr> \
        <tr> \
          <td>Server</td> \
          <td><input type=\"text\" value=\"[[blynk_server]]\" size=20 maxlength=64 id=\"blynk_server\"></td> \
        </tr> \
        <tr> \
          <td>Port</td> \
          <td><input type=\"text\" value=\"[[blynk_port]]\" id=\"blynk_port\"></td> \
        </tr> \
        <tr> \
          <td>Token</td> \
          <td><input type=\"text\" value=\"[[blynk_token]]\" size=20 maxlength=64 id=\"blynk_token\"></td> \
        </tr> \
        <tr> \
          <th colspan=\"2\">Hardware</th> \
        </tr> \
        <tr> \
          <td>Name</td> \
          <td><input type=\"text\" value=\"[[board_name]]\" size=20 maxlength=32 id=\"board_name\"></td> \
        </tr> \
        <tr> \
          <td colspan=\"2\" align=\"center\"> \
            <button onclick=\"save()\">Save</button> \
          </td> \
        </tr> \
      </tbody> \
    </table> \
  </div> \
<script id=\"jsbin-javascript\"> \
function updateValue(key, value) { \
  var request = new XMLHttpRequest(); \
  var url = '/?key=' + key + '&value=' + value; \
  console.log('calling ' + url + '...'); \
  request.open('GET', url, false); \
  request.send(null); \
} \
function save() { \
  updateValue('wifi_ssid', document.getElementById('wifi_ssid').value); \
  updateValue('wifi_passphrase', document.getElementById('wifi_passphrase').value); \
  updateValue('blynk_server', document.getElementById('blynk_server').value); \
  updateValue('blynk_port', document.getElementById('blynk_port').value); \
  updateValue('blynk_token', document.getElementById('blynk_token').value); \
  updateValue('board_name', document.getElementById('board_name').value); \
  alert('Updated Configurations. Resetting board'); \
} \
</script> \
</body> \
</html>";

#define BLYNK_SERVER_HARDWARE_PORT    9443

template <typename Transport>
class BlynkWifi
    : public BlynkProtocol<Transport>
{
    typedef BlynkProtocol<Transport> Base;
public:
    BlynkWifi(Transport& transp)
        : Base(transp)
    {}

    void connectWiFi(const char* ssid, const char* pass)
    {
        BLYNK_LOG2(BLYNK_F("Connecting to "), ssid);
        WiFi.mode(WIFI_STA);
        if (WiFi.status() != WL_CONNECTED) {
            if (pass && strlen(pass)) {
                WiFi.begin(ssid, pass);
            } else {
                WiFi.begin(ssid);
            }
        }
        while (WiFi.status() != WL_CONNECTED) {
            BlynkDelay(500);
        }
        BLYNK_LOG1(BLYNK_F("Connected to WiFi"));

        IPAddress myip = WiFi.localIP();
        BLYNK_LOG_IP("IP: ", myip);
    }

    void config(const char* auth,
                const char* domain = BLYNK_DEFAULT_DOMAIN,
                uint16_t    port   = BLYNK_DEFAULT_PORT_SSL,
                const char* fingerprint = NULL)
    {
        Base::begin(auth);
        this->conn.begin(domain, port);

        if (fingerprint) {
          this->conn.setFingerprint(fingerprint);
        } else {
          this->conn.setCACert_P(BLYNK_DEFAULT_CERT_DER, sizeof(BLYNK_DEFAULT_CERT_DER));
        }
    }

    void config(const char* auth,
                IPAddress   ip,
                uint16_t    port = BLYNK_DEFAULT_PORT_SSL,
                const char* fingerprint = NULL)
    {
        Base::begin(auth);
        this->conn.begin(ip, port);

        if (fingerprint) {
          this->conn.setFingerprint(fingerprint);
        } else {
          this->conn.setCACert_P(BLYNK_DEFAULT_CERT_DER, sizeof(BLYNK_DEFAULT_CERT_DER));
        }
    }

    void begin(const char* auth,
               const char* ssid,
               const char* pass,
               const char* domain = BLYNK_DEFAULT_DOMAIN,
               uint16_t    port   = BLYNK_DEFAULT_PORT_SSL,
               const char* fingerprint = NULL)
    {
        connectWiFi(ssid, pass);
        config(auth, domain, port, fingerprint);
        while(this->connect() != true) {}
    }

    void begin(const char* auth,
               const char* ssid,
               const char* pass,
               IPAddress   ip,
               uint16_t    port   = BLYNK_DEFAULT_PORT_SSL,
               const char* fingerprint = NULL)
    {
        connectWiFi(ssid, pass);
        config(auth, ip, port, fingerprint);
        while(this->connect() != true) {}
    }

    void getEEPROM()
    {
      #define EEPROM_SIZE       512
      #define BOARD_TYPE        "SSL_ESP8266"
      
      EEPROM.begin(EEPROM_SIZE);
      EEPROM.get(0, Blynk8266_WM_config);

      if (strncmp(Blynk8266_WM_config.header, BOARD_TYPE, strlen(BOARD_TYPE)) != 0) 
      {
          memset(&Blynk8266_WM_config, 0, sizeof(Blynk8266_WM_config));
          
          EEPROM.put(0, Blynk8266_WM_config);
          EEPROM.commit();
                                    
          char no_config[] = "nothing";
          BLYNK_LOG2(BLYNK_F("Init new EEPROM, size = "), EEPROM.length());          
          // doesn't have any configuration
          strcpy(Blynk8266_WM_config.header,           BOARD_TYPE);
          strcpy(Blynk8266_WM_config.wifi_ssid,        no_config);
          strcpy(Blynk8266_WM_config.wifi_passphrase,  no_config);
          strcpy(Blynk8266_WM_config.blynk_server,     no_config);
          Blynk8266_WM_config.blynk_port = BLYNK_SERVER_HARDWARE_PORT;
          strcpy(Blynk8266_WM_config.blynk_token,      no_config);
          strcpy(Blynk8266_WM_config.board_name,       no_config);

          EEPROM.put(0, Blynk8266_WM_config);
          EEPROM.commit();
      }
  
      else
      {
        BLYNK_LOG6(BLYNK_F("Header = "), Blynk8266_WM_config.header, BLYNK_F(", SSID = "), Blynk8266_WM_config.wifi_ssid, 
                   BLYNK_F(", PW = "),     Blynk8266_WM_config.wifi_passphrase);
        BLYNK_LOG6(BLYNK_F("Server = "), Blynk8266_WM_config.blynk_server, BLYNK_F(", Port = "), Blynk8266_WM_config.blynk_port, 
                   BLYNK_F(", Token = "),  Blynk8266_WM_config.blynk_token);
        BLYNK_LOG2(BLYNK_F("Board Name = "), Blynk8266_WM_config.board_name);               
      }
    }      
        
    void begin(const char* fingerprint = NULL) 
    {
        #define TIMEOUT_CONNECT_WIFI			30000
               
        getEEPROM();

        Base::begin(Blynk8266_WM_config.blynk_token);
        this->conn.begin(Blynk8266_WM_config.blynk_server, Blynk8266_WM_config.blynk_port);
        
        if (fingerprint) 
        {
          this->conn.setFingerprint(fingerprint);
        } 
        else 
        {
          this->conn.setCACert_P(BLYNK_DEFAULT_CERT_DER, sizeof(BLYNK_DEFAULT_CERT_DER));
        }        

        if (connectToWifi(TIMEOUT_CONNECT_WIFI)) 
        {
          BLYNK_LOG1(BLYNK_F("begin: WiFi connected. Try connecting to Blynk"));
          
          int i = 0;
          while ( (i++ < 10) && !this->connect() )
          {
          }
          
          if  (this->connected())
          {
            BLYNK_LOG1(BLYNK_F("begin: WiFi and Blynk connected"));
          }
          else 
          {
            BLYNK_LOG1(BLYNK_F("begin: WiFi connected but Bynk not connected"));
            // failed to connect to Blynk server, will start configuration mode
            // turn the LED_BUILTIN ON to tell us we are in configuration mode.
            digitalWrite(LED_BUILTIN, LOW);
            startConfigurationMode();
          }
        } 
        else 
        {
            BLYNK_LOG1(BLYNK_F("begin: Fail to connect WiFi and Blynk"));
            // failed to connect to Blynk server, will start configuration mode
            // turn the LED_BUILTIN ON to tell us we are in configuration mode.
            digitalWrite(LED_BUILTIN, LOW);            
            startConfigurationMode();
        }   
    }    
   
    void run()
    {
      #define TIMEOUT_RECONNECT_WIFI			10000
      
      // Lost connection in running. Give chance to reconfig.
      if ( WiFi.status() != WL_CONNECTED || !this->connected() )
      {   
		    if (configuration_mode)
		    {
			    server.handleClient();		
			    return;
		    }
		    else
		    {
			    // Not in config mode, try reconnecting before force to config mode
			    if ( WiFi.status() != WL_CONNECTED )
			    {
				    BLYNK_LOG1(BLYNK_F("run: WiFi lost. Try reconnecting WiFi and Blynk"));
				    if (connectToWifi(TIMEOUT_RECONNECT_WIFI)) 
				    {
				      BLYNK_LOG1(BLYNK_F("run: WiFi reconnected. Trying connect to Blynk"));
				      
			        if (this->connect())
			        {
				        BLYNK_LOG1(BLYNK_F("run: WiFi and Blynk reconnected"));
				      }					    
				    }
			    }
			    else
			    {
				    BLYNK_LOG1(BLYNK_F("run: Blynk lost. Try connecting Blynk"));
			      if (this->connect()) 
			      {
				      BLYNK_LOG1(BLYNK_F("run: Blynk reconnected"));
			      }
			    }
					
			    //BLYNK_LOG1(BLYNK_F("run: Lost connection => configMode"));
			    //startConfigurationMode();
        }
      }
      else if (configuration_mode)
      {
      	configuration_mode = false;
      	BLYNK_LOG1(BLYNK_F("run: got WiFi/Blynk back, great"));
      	// turn the LED_BUILTIN OFF to tell us we exit configuration mode.
        digitalWrite(LED_BUILTIN, HIGH);      	
      }

      if (this->connected())
      {
        Base::run();
      }
    }
    
    String getBoardName()
    {
      return (String(Blynk8266_WM_config.board_name));
    }
    
private:
    ESP8266WebServer server;
    boolean configuration_mode = false;
    struct Configuration Blynk8266_WM_config;

    boolean connectToWifi(int timeout)
    {
      int sleep_time = 250;

      WiFi.mode(WIFI_STA);

	    BLYNK_LOG1(BLYNK_F("connectToWifi: start"));
	
      if (Blynk8266_WM_config.wifi_passphrase && strlen(Blynk8266_WM_config.wifi_passphrase))
      {
          WiFi.begin(Blynk8266_WM_config.wifi_ssid, Blynk8266_WM_config.wifi_passphrase);
      } 
      else 
      {
          WiFi.begin(Blynk8266_WM_config.wifi_ssid);
      }

      while (WiFi.status() != WL_CONNECTED && 0 < timeout) 
      {
          delay(sleep_time);
          timeout -= sleep_time;
      }

	    if (WiFi.status() == WL_CONNECTED)
	    {
		    BLYNK_LOG1(BLYNK_F("connectToWifi: connected OK"));
        IPAddress myip = WiFi.localIP();
        BLYNK_LOG_IP("IP: ", myip);		    
		  }
	    else
	    {
		    BLYNK_LOG1(BLYNK_F("connectToWifi: connected failed"));
		  }
	
      return WiFi.status() == WL_CONNECTED;    
    }

    
    void handleRequest()
    {
      String key = server.arg("key");
      String value = server.arg("value");
      
      static int number_items_Updated = 0;

      if (key == "" && value == "") 
      {
          String result = root_html_template;

          result.replace("[[wifi_ssid]]",       Blynk8266_WM_config.wifi_ssid);
          result.replace("[[wifi_passphrase]]", Blynk8266_WM_config.wifi_passphrase);
          result.replace("[[blynk_server]]",    Blynk8266_WM_config.blynk_server);
          result.replace("[[blynk_port]]",      String(Blynk8266_WM_config.blynk_port));
          result.replace("[[blynk_token]]",     Blynk8266_WM_config.blynk_token);
          result.replace("[[board_name]]",      Blynk8266_WM_config.board_name);

          server.send(200, "text/html", result);

          return;
      }
     
      if (key == "wifi_ssid")
      {
          number_items_Updated++;
          if (strlen(value.c_str()) < sizeof(Blynk8266_WM_config.wifi_ssid) -1)
            strcpy(Blynk8266_WM_config.wifi_ssid, value.c_str());
      }
      else if (key == "wifi_passphrase") 
      {
          number_items_Updated++;
          if (strlen(value.c_str()) < sizeof(Blynk8266_WM_config.wifi_passphrase) -1)
            strcpy(Blynk8266_WM_config.wifi_passphrase, value.c_str());
      }

      else if (key == "blynk_server") 
      {
          number_items_Updated++;
          if (strlen(value.c_str()) < sizeof(Blynk8266_WM_config.blynk_server) -1)
            strcpy(Blynk8266_WM_config.blynk_server, value.c_str());
      }
      else if (key == "blynk_port") 
      {
          number_items_Updated++;
          Blynk8266_WM_config.blynk_port = value.toInt();
      }
      else if (key == "blynk_token") 
      {
          number_items_Updated++;
          if (strlen(value.c_str()) < sizeof(Blynk8266_WM_config.blynk_token) -1)
            strcpy(Blynk8266_WM_config.blynk_token, value.c_str());
      }
      else if (key == "board_name") 
      {
          number_items_Updated++;
          if (strlen(value.c_str()) < sizeof(Blynk8266_WM_config.board_name) -1)
            strcpy(Blynk8266_WM_config.board_name, value.c_str());
      }

      server.send(200, "text/html", "OK");
      
      if (number_items_Updated == NUM_CONFIGURABLE_ITEMS)
      {
        BLYNK_LOG1(BLYNK_F("handleRequest: Updating data to EEPROM"));

        EEPROM.put(0, Blynk8266_WM_config);
        EEPROM.commit();

        BLYNK_LOG1(BLYNK_F("handleRequest: Resetting"));
        
        // Delay then reset the ESP8266 after save data
        delay(1000);
        ESP.reset();    
      }
    
    }
        
    void startConfigurationMode()
    {
      String chipID = String(ESP.getChipId(), HEX);
      chipID.toUpperCase();
      
	    String ssid = "ESP_" + chipID;

	    String pass = "MyESP_" + chipID;
	    
	    BLYNK_LOG4(BLYNK_F("startConfigurationMode with SSID = "), ssid, BLYNK_F(" and PW = "), pass);
	
      IPAddress apIP(192, 168, 4, 1);

      WiFi.mode(WIFI_AP);
      WiFi.softAP(ssid, pass);
      
      delay(100); // ref: https://github.com/espressif/arduino-esp32/issues/985#issuecomment-359157428
      WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
      
      //See https://stackoverflow.com/questions/39803135/c-unresolved-overloaded-function-type?rq=1
      server.on("/", [this](){ handleRequest(); });

      server.begin();
       
      configuration_mode = true;    
    }    
};

static WiFiClientSecure _blynkWifiClient;
static BlynkArduinoClientSecure<WiFiClientSecure> _blynkTransport(_blynkWifiClient);
BlynkWifi<BlynkArduinoClientSecure<WiFiClientSecure> > Blynk(_blynkTransport);

#include <BlynkWidgets.h>

#endif