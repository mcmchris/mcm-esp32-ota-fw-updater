#ifndef MCM_GITHUB_OTA_H
#define MCM_GITHUB_OTA_H

#include <Arduino.h>

#if !defined(SPI_ETHERNET_SETTINGS)
  #define SPI_ETHERNET_SETTINGS SPISettings(40000000, MSBFIRST, SPI_MODE0)
#endif

#include <SPI.h>
#include <Ethernet.h>   
#include <WiFi.h>
#include <SSLClient.h>  
#include <ArduinoJson.h>
#include <Update.h>

#include "MCM_CertStore.h" 

enum MCM_NetType {
    MCM_NET_NONE,
    MCM_NET_ETH,
    MCM_NET_WIFI
};

struct RespHdrBin {
    int status = 0;
    long long contentLen = 0;
    bool chunked = false;
    String contentType;
    String location;
};

class MCM_GitHub_OTA {
public:
    MCM_GitHub_OTA(bool enableEthernet, bool enableWiFi);
    ~MCM_GitHub_OTA(); 

    void begin(const char* owner, const char* repo, const char* currentVersion, const char* token = "");

    void setSSLDebug(SSLClient::DebugLevel level);

    void checkForUpdate();
    bool isUpdated();
private:
    String _owner;
    String _repo;
    String _currentVersion;
    String _token;

    bool _useEthernet;
    bool _useWiFi;
    bool _isUpToDate = true; // Default to true until checked
    
    SSLClient::DebugLevel _sslDebugLevel = SSLClient::SSL_NONE;

    // Light clients
    EthernetClient _eth_client;
    WiFiClient     _wifi_client;

    
    // Helpers
    String ua();
    String latestReleaseUrl();
    String hostOf(const String& url);
    int portOf(const String& url);
    String pathOf(const String& url);
    
    MCM_NetType pickNetFast();
    bool readLine(SSLClient* c, String& out, unsigned long timeoutMs = 45000);
    bool readHeadersBin(SSLClient* c, RespHdrBin& h);
    bool writeAllToUpdate(uint8_t* data, size_t len);
    bool pipeFixedToUpdate(SSLClient* c, long long contentLen, size_t displayTotal, size_t alreadyWrote, size_t& totalWrote);
    bool pipeChunkedToUpdate(SSLClient* c);
    
    bool performUpdate(SSLClient* client, const String& startUrl, bool addAuth, const char* netName);
    bool getJson(SSLClient* client, const String& url, String& bodyOut, const char* netName, String overrideToken, int& httpCodeOut);
    
    bool canFit(size_t content_len);
    void confirmOtaIfPending();
};

#endif