#include <Arduino.h>
#include <WiFi.h>

// ===== 1. INCLUDE LIBRARY =====
#include <MCM_GitHub_OTA.h>

// ========================================
// PROTOCOL CONFIGURATION (MODIFY HERE!)
// ========================================
// Define which hardware you want to use.
// The library will only reserve RAM for what you enable here.
const bool ENABLE_ETH = false;
const bool ENABLE_WIFI = true;

// --- OTA Timings ---
const unsigned long OTA_PERIODIC_INTERVAL = 3600000;  // 1 hour (60 * 60 * 1000)

// Variable to control when the next check is due
unsigned long nextOtaCheck = 0;

// --- Blink Control ---
#define LED_PIN 1
static uint32_t BLINK_MS = 1000;

// --- WiFi Credentials ---
#define WIFI_SSID "********" // Your SSID
#define WIFI_PASS "********" // Your Password

// --- GitHub OTA ---
#define GH_OWNER "********" // Your GitHub User or Organization
#define GH_REPO "esp32-s2-ota-blink" // Your Repository
#define FW_VERSION "v1.0.0" // Current Firmware Version
#define GH_TOKEN "ghp_**************"  // Your GitHub Token (if needed)

// ========================================
// INSTANCE
// ========================================
// By passing the constants here, the library optimizes internal memory
MCM_GitHub_OTA ota(ENABLE_ETH, ENABLE_WIFI);

void setup() {
  Serial.begin(115200);
  while(!Serial){}
  delay(2000);

  pinMode(LED_PIN, OUTPUT);

  bool internetReady = false;
  if (ENABLE_WIFI) {
    Serial.print("[WIFI] Starting WiFi connection to ");
    Serial.println(WIFI_SSID);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
      Serial.print(".");
      delay(500);
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[WIFI] Connected! IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("[WIFI] Connection Failed (Timeout). System will continue.");
    }
  }
  // ---------------------------------------------------------
  // 3. CONFIGURE OTA
  // ---------------------------------------------------------
  ota.begin(GH_OWNER, GH_REPO, FW_VERSION, GH_TOKEN);
  ota.setSSLDebug(SSLClient::SSL_NONE);

  Serial.println("\n=== Performing Initial OTA Check (Clean Heap) ===");
  ota.checkForUpdate();

  // TIME LOGIC: Schedule the FIRST lightweight execution
  nextOtaCheck = millis() + OTA_PERIODIC_INTERVAL;

  Serial.printf("=== READY. Next lightweight check in %lu sec ===\n", OTA_PERIODIC_INTERVAL / 1000);
}

void loop() {
  
  // OTA Timer
  if ((long)(millis() - nextOtaCheck) >= 0) {
    Serial.println("\n[LOOP] Performing lightweight version check...");
    
    // checkVersion() just download the JSON (~2KB) and compares versions, without downloading the firmware. It's a quick way to check for updates without risking running out of RAM.
    if (ota.checkVersion()) {
        Serial.println("[LOOP] New version found! Rebooting for a clean installation...");
        delay(1000);
        ESP.restart(); // After reboot the OTA will detect the new version and perform the update with a clean heap.
    } else {
        // If we are here, it means we are up to date. Schedule the next check.
        nextOtaCheck = millis() + OTA_PERIODIC_INTERVAL;
        Serial.println("[LOOP] Next check scheduled in 1 hour.");
    }
  }

  // Blink
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > BLINK_MS) {
    lastBlink = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}
