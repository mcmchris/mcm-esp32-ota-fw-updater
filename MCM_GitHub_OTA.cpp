#include "MCM_GitHub_OTA.h"

extern "C" {
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
}

<<<<<<< HEAD
// Buffer global compartido
=======
// ==========================================================
// BUFFER GLOBAL COMPARTIDO (4KB) - ESTÁTICO
// ==========================================================
// Esto es clave para ahorrar RAM y permitir el buffer SSL grande
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
static uint8_t _global_buf[4096];

// ==========================================================
// Constructor
// ==========================================================
MCM_GitHub_OTA::MCM_GitHub_OTA(bool enableEthernet, bool enableWiFi) 
    : _useEthernet(enableEthernet), 
<<<<<<< HEAD
      _useWiFi(enableWiFi)
{
    // YA NO CREAMOS NADA AQUÍ. AHORRAMOS 36KB DE RAM AL INICIO.
}

MCM_GitHub_OTA::~MCM_GitHub_OTA() {
    // Nada que borrar
=======
      _useWiFi(enableWiFi),
      // Inicializamos SSLClient con el buffer optimizado de 18200 bytes
      _ssl_eth(_eth_client, TAs, TAs_NUM, -1, 1, 18200, SSLClient::SSL_NONE),
      _ssl_wifi(_wifi_client, TAs, TAs_NUM, -1, 1, 18200, SSLClient::SSL_NONE)
{
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
}

// ==========================================================
// Setup
// ==========================================================
void MCM_GitHub_OTA::begin(const char* owner, const char* repo, const char* currentVersion, const char* token) {
    _owner = owner;
    _repo = repo;
    _currentVersion = currentVersion;
    _token = token;
<<<<<<< HEAD
=======

>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
    confirmOtaIfPending();
}

// ==========================================================
// Helpers
// ==========================================================
<<<<<<< HEAD
String MCM_GitHub_OTA::ua() { return String("esp32-ota/") + _currentVersion; }

String MCM_GitHub_OTA::latestReleaseUrl() {
    String u = "https://api.github.com/repos/";
    u += _owner; u += '/'; u += _repo; u += "/releases/latest";
=======
String MCM_GitHub_OTA::ua() {
    return String("esp32-ota/") + _currentVersion;
}

String MCM_GitHub_OTA::latestReleaseUrl() {
    String u = "https://api.github.com/repos/";
    u += _owner;
    u += '/';
    u += _repo;
    u += "/releases/latest";
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
    return u;
}

String MCM_GitHub_OTA::hostOf(const String& url) {
    int p = url.indexOf("://");
    int s = (p >= 0) ? p + 3 : 0;
    int slash = url.indexOf('/', s);
    String hostPort = (slash > 0) ? url.substring(s, slash) : url.substring(s);
    int colon = hostPort.indexOf(':');
    return (colon >= 0) ? hostPort.substring(0, colon) : hostPort;
}

int MCM_GitHub_OTA::portOf(const String& url) {
    int p = url.indexOf("://");
    int s = (p >= 0) ? p + 3 : 0;
    int slash = url.indexOf('/', s);
    String hostPort = (slash > 0) ? url.substring(s, slash) : url.substring(s);
    int colon = hostPort.indexOf(':');
    return (colon >= 0) ? hostPort.substring(colon + 1).toInt() : 443;
}

String MCM_GitHub_OTA::pathOf(const String& url) {
    int p = url.indexOf("://");
    int s = (p >= 0) ? p + 3 : 0;
    int slash = url.indexOf('/', s);
    return (slash >= 0) ? url.substring(slash) : String("/");
}

void MCM_GitHub_OTA::confirmOtaIfPending() {
    const esp_partition_t* cur = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(cur, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_ota_mark_app_valid_cancel_rollback();
        Serial.printf("[MCM-OTA] OTA confirmed valid\n");
    }
}

bool MCM_GitHub_OTA::canFit(size_t content_len) {
    const esp_partition_t* upd = esp_ota_get_next_update_partition(NULL);
    return upd && content_len > 0 && content_len <= upd->size;
}

// ==========================================================
<<<<<<< HEAD
// Selección de Red
// ==========================================================
MCM_NetType MCM_GitHub_OTA::pickNetFast() {
    if (_useEthernet) {
        if (Ethernet.linkStatus() == LinkON) {
=======
// Selección de Red (No intrusiva)
// ==========================================================
MCM_NetType MCM_GitHub_OTA::pickNetFast() {
    // Prioridad Ethernet
    if (_useEthernet) {
        // Verificamos estado sin llamar a begin()
        if (Ethernet.linkStatus() == LinkON) {
             // Asumimos que si hay Link y IP válida, está listo.
             // La librería externa es responsable de mantener el DHCP lease.
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
             if (Ethernet.localIP() != IPAddress(0,0,0,0)) {
                 return MCM_NET_ETH;
             }
        }
    }
<<<<<<< HEAD
=======

>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
    if (_useWiFi) {
        if (WiFi.status() == WL_CONNECTED) {
            return MCM_NET_WIFI;
        }
    }
<<<<<<< HEAD
=======

>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
    return MCM_NET_NONE;
}

// ==========================================================
<<<<<<< HEAD
// Lógica Principal (OPTIMIZADA RAM)
// ==========================================================
void MCM_GitHub_OTA::checkForUpdate() {
    MCM_NetType net = pickNetFast();
    
    if (net == MCM_NET_NONE) {
        return;
    }

    const char* netName = (net == MCM_NET_ETH) ? "ETH" : "WIFI";
    
    // ============================================================
    // GESTIÓN DE MEMORIA DINÁMICA
    // Creamos el cliente SSL (18KB) AHORA MISMO, no antes.
    // ============================================================
    SSLClient* clientPtr = nullptr;
    
    if (net == MCM_NET_ETH) {
        // Envolver Ethernet
        clientPtr = new SSLClient(_eth_client, TAs, TAs_NUM, -1, 1, 18200, SSLClient::SSL_NONE);
    } else {
        // Envolver WiFi
        clientPtr = new SSLClient(_wifi_client, TAs, TAs_NUM, -1, 1, 18200, SSLClient::SSL_NONE);
    }

    // Verificamos que se haya podido crear (que haya RAM)
    if (!clientPtr) {
        Serial.println("[MCM-OTA] OOM: Could not allocate SSL Buffer!");
        return;
    }
=======
// Lógica Principal de Check
// ==========================================================
void MCM_GitHub_OTA::checkForUpdate() {
    MCM_NetType net = pickNetFast();
    if (net == MCM_NET_NONE) {
        Serial.println("[MCM-OTA] No network available for update check.");
        return;
    }

    SSLClient* clientPtr = (net == MCM_NET_ETH) ? &_ssl_eth : &_ssl_wifi;
    const char* netName = (net == MCM_NET_ETH) ? "ETH" : "WIFI";
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557

    // --- Paso 1: Obtener JSON ---
    String body;
    Serial.printf("[MCM-OTA-%s] Checking for updates...\n", netName);
<<<<<<< HEAD
    if (!getJson(clientPtr, latestReleaseUrl(), body, netName)) {
        Serial.println("[MCM-OTA] Failed to fetch release JSON");
        delete clientPtr; // ¡IMPORTANTE! Liberar memoria
=======
    if (!getJson(*clientPtr, latestReleaseUrl(), body, netName)) {
        Serial.println("[MCM-OTA] Failed to fetch release JSON");
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
        return;
    }

    // --- Paso 2: Parsear JSON ---
    JsonDocument doc;
    auto err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[MCM-OTA] JSON Error: %s\n", err.c_str());
<<<<<<< HEAD
        delete clientPtr;
=======
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
        return;
    }

    String remoteVersion = doc["tag_name"] | "";
    if (remoteVersion.length() == 0) {
        Serial.println("[MCM-OTA] No tag_name found");
<<<<<<< HEAD
        delete clientPtr;
=======
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
        return;
    }

    Serial.printf("[MCM-OTA] Local: %s, Remote: %s\n", _currentVersion.c_str(), remoteVersion.c_str());
    if (remoteVersion == _currentVersion) {
        Serial.println("[MCM-OTA] Device is up to date.");
<<<<<<< HEAD
        delete clientPtr;
=======
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
        return;
    }

    // --- Paso 3: Encontrar el Asset (.bin) ---
    int assetId = -1;
    size_t assetSize = 0;
    String browserUrl = "";
    
    for (JsonObject a : doc["assets"].as<JsonArray>()) {
        String name = a["name"] | "";
        if (name.endsWith(".bin")) {
            assetId = (int)a["id"];
            assetSize = (size_t)(a["size"] | 0);
            browserUrl = a["browser_download_url"] | "";
            break;
        }
    }

    if (assetId < 0) {
        Serial.println("[MCM-OTA] No .bin asset found in release.");
<<<<<<< HEAD
        delete clientPtr;
=======
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
        return;
    }

    if (!canFit(assetSize)) {
        Serial.printf("[MCM-OTA] Update image too big (%u bytes)\n", (unsigned)assetSize);
<<<<<<< HEAD
        delete clientPtr;
        return;
    }

    String assetUrl = String("https://api.github.com/repos/") + _owner + "/" + _repo + "/releases/assets/" + String(assetId);
=======
        return;
    }

    // Construir URL de API
    String assetUrl = String("https://api.github.com/repos/") + _owner + "/" + _repo + "/releases/assets/" + String(assetId);
    
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
    Serial.printf("[MCM-OTA] Downloading update from: %s\n", assetUrl.c_str());

    // --- Paso 4: Descargar y Flashear ---
    bool useAuth = (_token.length() > 0);
<<<<<<< HEAD
    bool success = performUpdate(clientPtr, assetUrl, useAuth, netName);

    if (!success && !useAuth && browserUrl.length() > 0) {
        Serial.println("[MCM-OTA] Retrying via browser_download_url...");
        performUpdate(clientPtr, browserUrl, false, netName);
    }
    
    // ============================================================
    // LIMPIEZA FINAL
    // Liberamos los 18KB de RAM para que el resto del programa respire
    // ============================================================
    delete clientPtr;
}

// ==========================================================
// Funciones de Red (Sin cambios, ya usan punteros)
// ==========================================================

bool MCM_GitHub_OTA::getJson(SSLClient* client, const String& url, String& bodyOut, const char* netName) {
=======
    bool success = performUpdate(*clientPtr, assetUrl, useAuth, netName);

    // Fallback a browser_download_url si falla y no hay token (repos públicos a veces redirigen raro)
    if (!success && !useAuth && browserUrl.length() > 0) {
        Serial.println("[MCM-OTA] Retrying via browser_download_url...");
        performUpdate(*clientPtr, browserUrl, false, netName);
    }
}

// ==========================================================
// Funciones de Red (Adaptadas para usar SSLClient genérico)
// ==========================================================

bool MCM_GitHub_OTA::getJson(SSLClient& client, const String& url, String& bodyOut, const char* netName) {
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
    String host = hostOf(url);
    String path = pathOf(url);
    int port = portOf(url);

<<<<<<< HEAD
    client->setTimeout(15000); 
    
    if (!client->connect(host.c_str(), port)) {
        Serial.printf("[MCM-OTA-%s] TLS Connect failed to %s:%d\n", netName, host.c_str(), port);
        client->stop();
        return false;
    }

    client->print(F("GET ")); client->print(path); client->println(F(" HTTP/1.1"));
    client->print(F("Host: ")); client->println(host);
    client->print(F("User-Agent: ")); client->println(ua());
    client->println(F("Accept: application/vnd.github+json"));
    if (_token.length() > 0) {
        client->print(F("Authorization: Bearer ")); client->println(_token);
    }
    client->println(F("Connection: close"));
    client->println();

    RespHdrBin h;
    if (!readHeadersBin(client, h)) {
        client->stop();
=======
    client.setTimeout(15000); // 15s para JSON
    
    // Debug de Heap
    // Serial.printf("[MCM-OTA-%s] Free Heap before SSL: %u\n", netName, ESP.getFreeHeap());

    if (!client.connect(host.c_str(), port)) {
        Serial.printf("[MCM-OTA-%s] TLS Connect failed to %s:%d\n", netName, host.c_str(), port);
        client.stop();
        return false;
    }

    client.print(F("GET ")); client.print(path); client.println(F(" HTTP/1.1"));
    client.print(F("Host: ")); client.println(host);
    client.print(F("User-Agent: ")); client.println(ua());
    client.println(F("Accept: application/vnd.github+json"));
    if (_token.length() > 0) {
        client.print(F("Authorization: Bearer ")); client.println(_token);
    }
    client.println(F("Connection: close"));
    client.println();

    RespHdrBin h;
    if (!readHeadersBin(client, h)) {
        client.stop();
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
        return false;
    }

    if (h.status != 200) {
        Serial.printf("[MCM-OTA-%s] JSON HTTP Status %d\n", netName, h.status);
<<<<<<< HEAD
        client->stop();
=======
        client.stop();
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
        return false;
    }

    bodyOut.reserve((h.contentLen > 0) ? (size_t)h.contentLen : 4096);
    
<<<<<<< HEAD
    for (;;) {
        size_t n = client->readBytes(_global_buf, sizeof(_global_buf));
        if (n == 0) {
            if (client->connected()) continue;
=======
    // Leer cuerpo usando buffer global
    for (;;) {
        size_t n = client.readBytes(_global_buf, sizeof(_global_buf));
        if (n == 0) {
            if (client.connected()) continue;
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
            break;
        }
        bodyOut.concat(String((char*)_global_buf).substring(0, n));
    }
<<<<<<< HEAD
    client->stop();
    return true;
}

bool MCM_GitHub_OTA::performUpdate(SSLClient* client, const String& startUrl, bool addAuth, const char* netName) {
=======
    client.stop();
    return true;
}

bool MCM_GitHub_OTA::performUpdate(SSLClient& client, const String& startUrl, bool addAuth, const char* netName) {
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
    String url = startUrl;
    size_t wroteTotal = 0;
    size_t expectedTotal = 0;
    bool haveExpected = false;

    for (int hop = 0; hop < 6; ++hop) {
        String host = hostOf(url);
        int port = portOf(url);
        String path = pathOf(url);

        Serial.printf("[MCM-OTA-%s] Connecting %s:%d\n", netName, host.c_str(), port);

        RECONNECT:
<<<<<<< HEAD
        client->setTimeout(120000);
        if (!client->connect(host.c_str(), port)) {
            Serial.printf("[MCM-OTA-%s] TLS Connect failed\n", netName);
            client->stop();
            return false;
        }

        client->print(F("GET ")); client->print(path); client->println(F(" HTTP/1.1"));
        client->print(F("Host: ")); client->println(host);
        client->print(F("User-Agent: ")); client->println(ua());
        client->println(F("Accept: application/octet-stream"));
        client->println(F("Accept-Encoding: identity"));
        client->println(F("Connection: close"));
        
        if (addAuth && _token.length() > 0) {
            client->print(F("Authorization: Bearer ")); client->println(_token);
        }
        
        if (wroteTotal > 0) {
            client->print(F("Range: bytes="));
            client->print((unsigned long)wroteTotal);
            client->println(F("-"));
        }
        client->println();

        RespHdrBin h;
        if (!readHeadersBin(client, h)) {
            client->stop();
=======
        client.setTimeout(120000); // 120s para binarios
        if (!client.connect(host.c_str(), port)) {
            Serial.printf("[MCM-OTA-%s] TLS Connect failed\n", netName);
            client.stop();
            return false;
        }

        client.print(F("GET ")); client.print(path); client.println(F(" HTTP/1.1"));
        client.print(F("Host: ")); client.println(host);
        client.print(F("User-Agent: ")); client.println(ua());
        client.println(F("Accept: application/octet-stream"));
        client.println(F("Accept-Encoding: identity"));
        client.println(F("Connection: close"));
        
        if (addAuth && _token.length() > 0) {
            client.print(F("Authorization: Bearer ")); client.println(_token);
        }
        
        if (wroteTotal > 0) {
            client.print(F("Range: bytes="));
            client.print((unsigned long)wroteTotal);
            client.println(F("-"));
        }
        client.println();

        RespHdrBin h;
        if (!readHeadersBin(client, h)) {
            client.stop();
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
            Serial.println("[MCM-OTA] Header parse fail");
            return false;
        }

<<<<<<< HEAD
        if (h.status >= 300 && h.status < 400 && h.location.length()) {
            Serial.printf("[MCM-OTA] Redirect -> %s\n", h.location.c_str());
            client->stop();
=======
        // Handle Redirects
        if (h.status >= 300 && h.status < 400 && h.location.length()) {
            Serial.printf("[MCM-OTA] Redirect -> %s\n", h.location.c_str());
            client.stop();
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
            url = h.location;
            continue;
        }

<<<<<<< HEAD
        if (!((h.status == 200 && wroteTotal == 0) || (h.status == 206 && wroteTotal > 0))) {
            Serial.printf("[MCM-OTA] HTTP Status %d\n", h.status);
            client->stop();
            return false;
        }

=======
        // Check Status (200 OK or 206 Partial)
        if (!((h.status == 200 && wroteTotal == 0) || (h.status == 206 && wroteTotal > 0))) {
            Serial.printf("[MCM-OTA] HTTP Status %d\n", h.status);
            client.stop();
            return false;
        }

        // Size Check
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
        if (!haveExpected) {
            if (h.contentLen > 0 && h.status == 200) {
                expectedTotal = (size_t)h.contentLen;
                haveExpected = true;
            } else if (h.status == 206) {
                haveExpected = (h.contentLen > 0);
                if (haveExpected) expectedTotal = wroteTotal + (size_t)h.contentLen;
            }
        }

<<<<<<< HEAD
=======
        // Update Begin
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
        if (wroteTotal == 0) {
            size_t beginSize = haveExpected ? expectedTotal : (size_t)UPDATE_SIZE_UNKNOWN;
            if (!Update.begin(beginSize)) {
                Serial.printf("[MCM-OTA] Update.begin failed err=%u\n", Update.getError());
<<<<<<< HEAD
                client->stop();
=======
                client.stop();
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
                return false;
            }
        }

<<<<<<< HEAD
=======
        // Stream Body
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
        size_t wroteNow = 0;
        bool ok = false;
        if (h.chunked) {
            ok = pipeChunkedToUpdate(client);
        } else {
            long long remaining = h.contentLen;
            size_t totalBefore = wroteTotal;
            ok = pipeFixedToUpdate(client, remaining, haveExpected ? expectedTotal : 0, wroteTotal, wroteTotal);
            wroteNow = wroteTotal - totalBefore;

            if (!ok) {
<<<<<<< HEAD
                client->stop();
=======
                client.stop();
                // Resume logic
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
                if (haveExpected && wroteTotal < expectedTotal) {
                    Serial.printf("\n[MCM-OTA] Connection closed @ %u / %u. Resuming...\n", (unsigned)wroteTotal, (unsigned)expectedTotal);
                    delay(1000);
                    goto RECONNECT;
                }
            }
        }

<<<<<<< HEAD
        client->stop();
=======
        client.stop();
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557

        if (!ok) {
            Update.end();
            return false;
        }

        if (haveExpected && wroteTotal >= expectedTotal) {
            if (!Update.end()) {
                Serial.printf("[MCM-OTA] Update.end failed err=%u\n", Update.getError());
                return false;
            }
            if (!Update.isFinished()) {
                Serial.println("[MCM-OTA] Update not finished?");
                return false;
            }
            Serial.println("[MCM-OTA] Update Success! Rebooting...");
            delay(300);
            ESP.restart();
            return true;
        }
        
<<<<<<< HEAD
=======
        // Si no tenemos expected size pero terminó ok la descarga (raro sin chunked)
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
        if (!haveExpected) {
             if(Update.end()) {
                 Serial.println("[MCM-OTA] Update Success (Unknown Size)! Rebooting...");
                 delay(300);
                 ESP.restart();
                 return true;
             }
        }
    }
    Serial.println("[MCM-OTA] Too many redirects.");
    return false;
}


// ==========================================================
<<<<<<< HEAD
// Internal Pipeline Utilities
// ==========================================================

bool MCM_GitHub_OTA::readLine(SSLClient* c, String& out, unsigned long timeoutMs) {
=======
// Internal Pipeline Utilities (Buffer Global)
// ==========================================================

bool MCM_GitHub_OTA::readLine(SSLClient& c, String& out, unsigned long timeoutMs) {
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
    out = "";
    unsigned long t0 = millis();
    bool gotCR = false;
    while (true) {
<<<<<<< HEAD
        if (c->available()) {
            int ch = c->read();
=======
        if (c.available()) {
            int ch = c.read();
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
            if (ch < 0) continue;
            char cch = (char)ch;
            if (gotCR && cch == '\n') return true;
            if (cch == '\r') {
                gotCR = true;
                continue;
            }
            gotCR = false;
            out += cch;
            if (out.length() > 8192) return false; 
        } else {
            if ((millis() - t0) > timeoutMs) return false;
            delay(1);
        }
    }
}

<<<<<<< HEAD
bool MCM_GitHub_OTA::readHeadersBin(SSLClient* c, RespHdrBin& h) {
=======
bool MCM_GitHub_OTA::readHeadersBin(SSLClient& c, RespHdrBin& h) {
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
    String line;
    if (!readLine(c, line)) return false;
    if (!line.startsWith("HTTP/1.1 ")) return false;
    h.status = line.substring(9, 12).toInt();

    while (true) {
        if (!readLine(c, line)) return false;
        if (line.length() == 0) break;
        int colon = line.indexOf(':');
        if (colon <= 0) continue;
        String name = line.substring(0, colon);
        name.trim();
        String val = line.substring(colon + 1);
        val.trim();
        name.toLowerCase();

        if (name == "transfer-encoding" && val.equalsIgnoreCase("chunked")) h.chunked = true;
        else if (name == "content-length") h.contentLen = atoll(val.c_str());
        else if (name == "content-type") h.contentType = val;
        else if (name == "location") h.location = val;
    }
    return true;
}

bool MCM_GitHub_OTA::writeAllToUpdate(uint8_t* data, size_t len) {
    size_t off = 0;
    while (off < len) {
        size_t w = Update.write(data + off, len - off);
        if (w == 0) return false;
        off += w;
        delay(0);
        yield();
    }
    return true;
}

<<<<<<< HEAD
bool MCM_GitHub_OTA::pipeFixedToUpdate(SSLClient* c, long long contentLen, size_t displayTotal, size_t alreadyWrote, size_t& totalWrote) {
=======
bool MCM_GitHub_OTA::pipeFixedToUpdate(SSLClient& c, long long contentLen, size_t displayTotal, size_t alreadyWrote, size_t& totalWrote) {
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
    long long remaining = contentLen;
    totalWrote = alreadyWrote;
    size_t lastPct = 0;

    while (remaining > 0) {
        delay(0); yield();
        size_t toRead = (remaining > (long long)sizeof(_global_buf)) ? sizeof(_global_buf) : (size_t)remaining;
<<<<<<< HEAD
        size_t n = c->readBytes(_global_buf, toRead);
=======
        size_t n = c.readBytes(_global_buf, toRead);
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
        if (n == 0) return false;

        if (!writeAllToUpdate(_global_buf, n)) return false;
        remaining -= n;
        totalWrote += n;

        if (displayTotal > 0) {
            size_t pct = (totalWrote * 100) / displayTotal;
            if (pct != lastPct && (pct % 5) == 0) {
                lastPct = pct;
                Serial.printf("[%u%%]", (unsigned)pct);
                fflush(stdout);
            }
        } else {
<<<<<<< HEAD
=======
            // Simple progress indicator
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
            if ((totalWrote / 10240) != ((totalWrote - n) / 10240)) {
                 Serial.print("."); fflush(stdout);
            }
        }
    }
    Serial.println();
    return true;
}

<<<<<<< HEAD
bool MCM_GitHub_OTA::pipeChunkedToUpdate(SSLClient* c) {
=======
bool MCM_GitHub_OTA::pipeChunkedToUpdate(SSLClient& c) {
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
    while (true) {
        String szLine;
        if (!readLine(c, szLine)) return false;
        long chunk = strtol(szLine.c_str(), nullptr, 16);
        if (chunk <= 0) {
            String dummy; readLine(c, dummy); // consume trailing CRLF
            break;
        }

        long remaining = chunk;
        while (remaining > 0) {
            size_t want = (remaining > (long)sizeof(_global_buf)) ? sizeof(_global_buf) : (size_t)remaining;
<<<<<<< HEAD
            size_t n = c->readBytes(_global_buf, want);
=======
            size_t n = c.readBytes(_global_buf, want);
>>>>>>> 9dd72c7a9cd7fcd69037ca2a62c21df29e2eb557
            if (n == 0) return false;
            if (!writeAllToUpdate(_global_buf, n)) return false;
            remaining -= n;
            
            if ((chunk - remaining) % 10240 < 4096) { Serial.print("."); fflush(stdout); }
        }
        String crlf;
        if (!readLine(c, crlf)) return false;
    }
    Serial.println();
    return true;
}