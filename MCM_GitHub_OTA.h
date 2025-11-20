#ifndef MCM_GITHUB_OTA_H
#define MCM_GITHUB_OTA_H

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>   // Arduino Standard Ethernet Library
#include <WiFi.h>
#include <SSLClient.h>  // BearSSL (Software SSL)
#include <ArduinoJson.h>
#include <Update.h>

#include "MCM_CertStore.h" // Tus certificados

// Tipos de red interna
enum MCM_NetType {
    MCM_NET_NONE,
    MCM_NET_ETH,
    MCM_NET_WIFI
};

// Estructura interna para headers
struct RespHdrBin {
    int status = 0;
    long long contentLen = 0;
    bool chunked = false;
    String contentType;
    String location;
};

class MCM_GitHub_OTA {
public:
    // Constructor: Define qué interfaces quieres habilitar
    MCM_GitHub_OTA(bool enableEthernet, bool enableWiFi);

    // Configura los datos del repositorio
    void begin(const char* owner, const char* repo, const char* currentVersion, const char* token = "");

    // Llama a esto periódicamente para verificar actualizaciones
    void checkForUpdate();

private:
    // Configuración
    String _owner;
    String _repo;
    String _currentVersion;
    String _token;

    // Flags de hardware
    bool _useEthernet;
    bool _useWiFi;

    // Objetos de Red (Propios de la librería para gestionar SSL)
    EthernetClient _eth_client;
    WiFiClient     _wifi_client;

    // Envoltorios SSL (BearSSL)
    // Nota: Se inicializan en el constructor con el buffer de 18200 bytes
    SSLClient _ssl_eth;
    SSLClient _ssl_wifi;

    // Helpers internos
    String ua();
    String latestReleaseUrl();
    String hostOf(const String& url);
    int portOf(const String& url);
    String pathOf(const String& url);
    
    MCM_NetType pickNetFast();
    bool readLine(SSLClient& c, String& out, unsigned long timeoutMs = 45000);
    bool readHeadersBin(SSLClient& c, RespHdrBin& h);
    bool writeAllToUpdate(uint8_t* data, size_t len);
    bool pipeFixedToUpdate(SSLClient& c, long long contentLen, size_t displayTotal, size_t alreadyWrote, size_t& totalWrote);
    bool pipeChunkedToUpdate(SSLClient& c);
    
    bool performUpdate(SSLClient& client, const String& startUrl, bool addAuth, const char* netName);
    bool getJson(SSLClient& client, const String& url, String& bodyOut, const char* netName);
    
    bool canFit(size_t content_len);
    void confirmOtaIfPending();
};

#endif