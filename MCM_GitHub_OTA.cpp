#include "MCM_GitHub_OTA.h"

extern "C" {
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
}

// Global Shared Buffer
static uint8_t _global_buf[4096];

// ==========================================================
// Constructor
// ==========================================================
MCM_GitHub_OTA::MCM_GitHub_OTA(bool enableEthernet, bool enableWiFi) 
    : _useEthernet(enableEthernet), 
      _useWiFi(enableWiFi)
{
    _isUpToDate = true; 
}

MCM_GitHub_OTA::~MCM_GitHub_OTA() { }

// ==========================================================
// Setup
// ==========================================================
void MCM_GitHub_OTA::begin(const char* owner, const char* repo, const char* currentVersion, const char* token) {
    _owner = owner;
    _repo = repo;
    _currentVersion = currentVersion;
    _token = token;
    confirmOtaIfPending();
}

void MCM_GitHub_OTA::setSSLDebug(SSLClient::DebugLevel level) {
    _sslDebugLevel = level;
}

// ==========================================================
// Main Logic (Optimized with Late Fallback & Token Rescue)
// ==========================================================
void MCM_GitHub_OTA::checkForUpdate() {
    MCM_NetType net = pickNetFast();
    
    if (net == MCM_NET_NONE) return;

    const char* netName = (net == MCM_NET_ETH) ? "ETH" : "WIFI";
    
    SSLClient* clientPtr = nullptr;
    String body;
    int httpCode = 0; // To save the connection result
    bool fetchSuccess = false;
    
    // Flag to know if we are operating securely or not.
    bool isSecureSession = true; 

    // ------------------------------------------------------------
    // ATTEMPT 1: SECURE MODE (With Trust Anchors + Original Token)
    // ------------------------------------------------------------
    Serial.printf("[MCM-OTA] Attempting SECURE connection via %s...\n", netName);
    
    // SSL client configuration (same as before)
    if (net == MCM_NET_ETH) {
        clientPtr = new SSLClient(_eth_client, TAs, TAs_NUM, -1, 1, 18200, _sslDebugLevel);
    } else {
        clientPtr = new SSLClient(_wifi_client, TAs, TAs_NUM, -1, 1, 18200, _sslDebugLevel);
    }

    if (clientPtr) {
        // We use modified getJson that returns the HTTP code in 'httpCode'
        // We pass _token by default
        if (getJson(clientPtr, latestReleaseUrl(), body, netName, _token, httpCode)) {
            fetchSuccess = true;
        } else {
            // If it failed, we check why
            Serial.printf("[MCM-OTA] Connection failed. HTTP Code: %d\n", httpCode);
            
            // --- RESCUE LOGIC (FAILED TOKEN 401) ---
            if (httpCode == 401) {
                Serial.println("[MCM-OTA] !!! TOKEN REVOKED/INVALID (401) DETECTED !!!");
                Serial.println("[MCM-OTA] !!! ATTEMPTING RESCUE: RETRYING WITHOUT TOKEN !!!");
                
                // We restart the client to clean state
                clientPtr->stop();
                
                // We retry the request but passing an empty String "" as token
                int rescueCode = 0;
                String rescueBody;
                if (getJson(clientPtr, latestReleaseUrl(), rescueBody, netName, "", rescueCode)) {
                    Serial.println("[MCM-OTA] Rescue successful! Repository is likely Public.");
                    body = rescueBody;
                    fetchSuccess = true;
                    // IMPORTANT: For the .bin download we must also ignore the token
                    _token = ""; 
                } else {
                     Serial.printf("[MCM-OTA] Rescue failed too. HTTP: %d\n", rescueCode);
                }
            }
            
            if (!fetchSuccess) {
                Serial.println("[MCM-OTA] Secure connection failed (Possible Cert Issue or Rescue Failed).");
                delete clientPtr; 
                clientPtr = nullptr;
            }
        }
    } else {
        Serial.println("[MCM-OTA] OOM: Could not allocate SSL Buffer (Secure)!");
        return;
    }

    // ------------------------------------------------------------
    // ATTEMPT 2: EARLY PARACHUTE (If JSON failed due to certificates)
    // ------------------------------------------------------------
    if (!fetchSuccess && httpCode != 401) { // Only if it was NOT a token error (we already tried to rescue it)
        Serial.println("[MCM-OTA] !!! ACTIVATING INSECURE FALLBACK (JSON FETCH) !!!");
        delay(500); 

        isSecureSession = false;

        if (net == MCM_NET_ETH) {
            clientPtr = new SSLClient(_eth_client, TAs, 0, -1, 1, 18200, _sslDebugLevel);
        } else {
            clientPtr = new SSLClient(_wifi_client, TAs, 0, -1, 1, 18200, _sslDebugLevel);
        }

        if (clientPtr) {
            clientPtr->setInsecure(); 

            Serial.printf("[MCM-OTA-%s] Retrying connection INSECURELY...\n", netName);
            // We try insecure with the original token first
            if (getJson(clientPtr, latestReleaseUrl(), body, netName, _token, httpCode)) {
                Serial.println("[MCM-OTA] Insecure connection successful.");
                fetchSuccess = true;
            } 
            // If it fails with 401 in insecure mode, we also try rescue
            else if (httpCode == 401) {
                 Serial.println("[MCM-OTA] (Insecure) 401 Detected. Retrying WITHOUT TOKEN...");
                 clientPtr->stop();
                 if (getJson(clientPtr, latestReleaseUrl(), body, netName, "", httpCode)) {
                     Serial.println("[MCM-OTA] (Insecure) Rescue successful without token.");
                     fetchSuccess = true;
                     _token = ""; // We clear token for the subsequent download
                 }
            }
            
            if (!fetchSuccess) {
                Serial.println("[MCM-OTA] Insecure connection also failed. Aborting.");
                delete clientPtr;
                return;
            }
        } else {
            Serial.println("[MCM-OTA] OOM: Could not allocate SSL Buffer (Insecure)!");
            return;
        }
    }

    // --- COMMON PROCESSING ---
    JsonDocument doc;
    auto err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[MCM-OTA] JSON Error: %s\n", err.c_str());
        delete clientPtr;
        return;
    }
    
    String remoteVersion = doc["tag_name"] | "";
    if (remoteVersion.length() == 0) {
        Serial.println("[MCM-OTA] No tag_name found");
        delete clientPtr;
        return;
    }

    Serial.printf("[MCM-OTA] Local: %s, Remote: %s\n", _currentVersion.c_str(), remoteVersion.c_str());
    if (remoteVersion == _currentVersion) {
        _isUpToDate = true;
        Serial.println("[MCM-OTA] Device is up to date.");
        delete clientPtr;
        return;
    } else {
        _isUpToDate = false;
        Serial.println("[MCM-OTA] Update available!");
    }

    // Buscar asset .bin
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
        delete clientPtr;
        return;
    }

    if (!canFit(assetSize)) {
        Serial.printf("[MCM-OTA] Update image too big (%u bytes)\n", (unsigned)assetSize);
        delete clientPtr;
        return;
    }

    String assetUrl = String("https://api.github.com/repos/") + _owner + "/" + _repo + "/releases/assets/" + String(assetId);
    Serial.printf("[MCM-OTA] Downloading update from: %s\n", assetUrl.c_str());

    // We use the current token (which may have been erased by the rescue)
    bool useAuth = (_token.length() > 0);
    
    // --- DOWNLOAD (MAIN ATTEMPT) ---
    bool success = performUpdate(clientPtr, assetUrl, useAuth, netName);

    // ... [REST OF THE LATE FALLBACK LOGIC] ...
    if (!success && isSecureSession) {
        Serial.println("[MCM-OTA] !!! DOWNLOAD FAILED SECURELY (Likely CDN Cert Issue) !!!");
        Serial.println("[MCM-OTA] !!! ACTIVATING INSECURE FALLBACK (DOWNLOAD PHASE) !!!");
        
        delete clientPtr;
        clientPtr = nullptr;
        delay(500);

        if (net == MCM_NET_ETH) {
            clientPtr = new SSLClient(_eth_client, TAs, 0, -1, 1, 18200, _sslDebugLevel);
        } else {
            clientPtr = new SSLClient(_wifi_client, TAs, 0, -1, 1, 18200, _sslDebugLevel);
        }

        if (clientPtr) {
            clientPtr->setInsecure();
            Serial.println("[MCM-OTA] Retrying download INSECURELY...");
            success = performUpdate(clientPtr, assetUrl, useAuth, netName);
        }
    }

    if (!success && !useAuth && browserUrl.length() > 0) {
        Serial.println("[MCM-OTA] Retrying via browser_download_url...");
        if(clientPtr) clientPtr->stop(); 
        performUpdate(clientPtr, browserUrl, false, netName);
    }
    
    delete clientPtr;
}

// ==========================================================
// Helpers 
// ==========================================================
String MCM_GitHub_OTA::ua() { return String("esp32-ota/") + _currentVersion; }

String MCM_GitHub_OTA::latestReleaseUrl() {
    String u = "https://api.github.com/repos/";
    u += _owner; u += '/'; u += _repo; u += "/releases/latest";
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

bool MCM_GitHub_OTA::isUpdated() {
    return _isUpToDate;
}

MCM_NetType MCM_GitHub_OTA::pickNetFast() {
    if (_useEthernet) {
        if (Ethernet.linkStatus() == LinkON) {
             if (Ethernet.localIP() != IPAddress(0,0,0,0)) return MCM_NET_ETH;
        }
    }
    if (_useWiFi) {
        if (WiFi.status() == WL_CONNECTED) return MCM_NET_WIFI;
    }
    return MCM_NET_NONE;
}

bool MCM_GitHub_OTA::getJson(SSLClient* client, const String& url, String& bodyOut, const char* netName, String overrideToken, int& httpCodeOut) {
    String host = hostOf(url);
    String path = pathOf(url);
    int port = portOf(url);

    client->setTimeout(15000); 
    
    if (!client->connect(host.c_str(), port)) {
        Serial.printf("[MCM-OTA-%s] TLS Connect failed to %s:%d\n", netName, host.c_str(), port);
        client->stop();
        httpCodeOut = -1; 
        return false;
    }

    client->print(F("GET ")); client->print(path); client->println(F(" HTTP/1.1"));
    client->print(F("Host: ")); client->println(host);
    client->print(F("User-Agent: ")); client->println(ua());
    client->println(F("Accept: application/vnd.github+json"));
    
    if (overrideToken.length() > 0) {
        client->print(F("Authorization: Bearer ")); client->println(overrideToken);
    }
    
    client->println(F("Connection: close"));
    client->println();

    RespHdrBin h;
    if (!readHeadersBin(client, h)) {
        client->stop();
        httpCodeOut = -2;
        return false;
    }
    
    httpCodeOut = h.status;

    if (h.status != 200) {
        client->stop();
        return false;
    }

    bodyOut.reserve((h.contentLen > 0) ? (size_t)h.contentLen : 4096);
    
    unsigned long lastData = millis();
    while (client->connected() || client->available()) {
        int avail = client->available();
        if (avail > 0) {
            int toRead = (avail > (int)sizeof(_global_buf)) ? (int)sizeof(_global_buf) : avail;
            int n = client->read(_global_buf, toRead);
            if (n > 0) {
                bodyOut.concat(String((char*)_global_buf).substring(0, n));
                lastData = millis();
            }
        } else {
            delay(1);

            if (millis() - lastData > 5000) break; 
        }
    }
    // -----------------------

    client->stop();
    return true;
}

bool MCM_GitHub_OTA::performUpdate(SSLClient* client, const String& startUrl, bool addAuth, const char* netName) {
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
            Serial.println("[MCM-OTA] Header parse fail");
            return false;
        }

        if (h.status >= 300 && h.status < 400 && h.location.length()) {
            Serial.printf("[MCM-OTA] Redirect -> %s\n", h.location.c_str());
            client->stop();
            url = h.location;
            continue;
        }

        if (!((h.status == 200 && wroteTotal == 0) || (h.status == 206 && wroteTotal > 0))) {
            Serial.printf("[MCM-OTA] HTTP Status %d\n", h.status);
            client->stop();
            return false;
        }

        if (!haveExpected) {
            if (h.contentLen > 0 && h.status == 200) {
                expectedTotal = (size_t)h.contentLen;
                haveExpected = true;
            } else if (h.status == 206) {
                haveExpected = (h.contentLen > 0);
                if (haveExpected) expectedTotal = wroteTotal + (size_t)h.contentLen;
            }
        }

        if (wroteTotal == 0) {
            size_t beginSize = haveExpected ? expectedTotal : (size_t)UPDATE_SIZE_UNKNOWN;
            if (!Update.begin(beginSize)) {
                Serial.printf("[MCM-OTA] Update.begin failed err=%u\n", Update.getError());
                client->stop();
                return false;
            }
        }

        bool ok = false;
        if (h.chunked) {
            ok = pipeChunkedToUpdate(client);
        } else {
            long long remaining = h.contentLen;
            size_t totalBefore = wroteTotal;
            ok = pipeFixedToUpdate(client, remaining, haveExpected ? expectedTotal : 0, wroteTotal, wroteTotal);
            
            if (!ok) {
                client->stop();
                if (haveExpected && wroteTotal < expectedTotal) {
                    Serial.printf("\n[MCM-OTA] Connection closed @ %u / %u. Resuming...\n", (unsigned)wroteTotal, (unsigned)expectedTotal);
                    delay(1000);
                    goto RECONNECT;
                }
            }
        }

        client->stop();

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
// Utilities
// ==========================================================

bool MCM_GitHub_OTA::readLine(SSLClient* c, String& out, unsigned long timeoutMs) {
    out = "";
    unsigned long t0 = millis();
    bool gotCR = false;
    
    while ((millis() - t0) < timeoutMs) {
        int ch = c->read(); 
        
        if (ch >= 0) {
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
            if (!c->connected() && c->available() == 0) {
                return false; 
            }
            delay(1); 
        }
    }
    return false; // Timeout
}

bool MCM_GitHub_OTA::readHeadersBin(SSLClient* c, RespHdrBin& h) {
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

bool MCM_GitHub_OTA::pipeFixedToUpdate(SSLClient* c, long long contentLen, size_t displayTotal, size_t alreadyWrote, size_t& totalWrote) {
    long long remaining = contentLen;
    totalWrote = alreadyWrote;
    size_t lastPct = 0;

    while (remaining > 0) {
        delay(0); yield();
        size_t toRead = (remaining > (long long)sizeof(_global_buf)) ? sizeof(_global_buf) : (size_t)remaining;
        size_t n = c->readBytes(_global_buf, toRead);
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
            if ((totalWrote / 10240) != ((totalWrote - n) / 10240)) {
                 Serial.print("."); fflush(stdout);
            }
        }
    }
    Serial.println();
    return true;
}

bool MCM_GitHub_OTA::pipeChunkedToUpdate(SSLClient* c) {
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
            size_t n = c->readBytes(_global_buf, want);
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