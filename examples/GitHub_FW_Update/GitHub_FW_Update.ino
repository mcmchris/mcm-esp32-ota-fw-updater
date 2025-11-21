#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <WiFi.h>

// ===== 1. INCLUDE LIBRARY =====
#include <MCM_GitHub_OTA.h>

// ========================================
// PROTOCOL CONFIGURATION (MODIFY HERE!)
// ========================================
// Define which hardware you want to use.
// The library will only reserve RAM for what you enable here.
const bool ENABLE_ETH = true;
const bool ENABLE_WIFI = true;

// --- OTA Timings ---
const unsigned long OTA_INITIAL_DELAY = 10000;        // 10 seconds after boot
const unsigned long OTA_PERIODIC_INTERVAL = 3600000;  // 1 hour (60 * 60 * 1000)

// Variable to control when the next check is due
unsigned long nextOtaCheck = 0;


// --- Blink Control ---
#define LED_PIN 1
static uint32_t BLINK_MS = 1000;

// ========================================
// HARDWARE & NETWORK CONFIGURATION
// ========================================

// --- Ethernet SPI Pins (W5500) ---
#define ETH_CS 33
#define ETH_SCLK 36
#define ETH_MISO 37
#define ETH_MOSI 35
#define ETH_RST -1

uint8_t mac[] = { 0x02, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };

// --- WiFi Credentials ---
#define WIFI_SSID "*******" // Your SSID
#define WIFI_PASS "*******" // Your Password

// --- GitHub OTA ---
#define GH_OWNER "********" // Your GitHub User or Organization
#define GH_REPO "******************" // Your Repository
#define FW_VERSION "v1.0.1" // Current Firmware Version
#define GH_TOKEN "ghp_***************"  // Your GitHub Token (if needed)

// ========================================
// INSTANCE
// ========================================
// By passing the constants here, the library optimizes internal memory
MCM_GitHub_OTA ota(ENABLE_ETH, ENABLE_WIFI);

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== SMART NETWORK START ===");

  bool internetReady = false;

  // ---------------------------------------------------------
  // 1. ETHERNET BLOCK (Only runs if ENABLE_ETH is true)
  // ---------------------------------------------------------
  if (ENABLE_ETH) {
    Serial.println("[ETH] Starting Ethernet Hardware...");

    // A. Configure SPI only if using Ethernet
    SPI.begin(ETH_SCLK, ETH_MISO, ETH_MOSI);
    Ethernet.init(ETH_CS);

    // B. W5500 Manual Reset
    if (ETH_RST >= 0) {
      pinMode(ETH_RST, OUTPUT);
      digitalWrite(ETH_RST, HIGH);
      delay(10);
      digitalWrite(ETH_RST, LOW);
      delay(10);
      digitalWrite(ETH_RST, HIGH);
      delay(10);
    }

    // C. CABLE VERIFICATION (LINK STATUS)
    EthernetLinkStatus link = Ethernet.linkStatus();

    if (link == LinkOFF) {
      Serial.println("[ETH] Cable disconnected (LinkOFF). Skipping DHCP.");
    } else if (link == Unknown) {
      // Sometimes the W5500 takes a moment to bring up the PHY, give it a small breather
      delay(100);
      if (Ethernet.linkStatus() == LinkOFF) {
        Serial.println("[ETH] Cable disconnected. Skipping DHCP.");
      } else {
        goto TRY_DHCP;  // Cable detected
      }
    } else {
TRY_DHCP:
      Serial.println("[ETH] Cable detected. Attempting DHCP...");
      // Only enter DHCP timeout if the cable is plugged in
      if (Ethernet.begin(mac) == 0) {
        Serial.println("[ETH] DHCP configuration failed.");
      } else {
        Serial.print("[ETH] Connected! IP: ");
        Serial.println(Ethernet.localIP());
        internetReady = true;
      }
    }
  }

  // ---------------------------------------------------------
  // 2. WIFI BLOCK (Backup or main logic)
  // ---------------------------------------------------------
  // We enter here if:
  // a) ENABLE_WIFI is true AND no Ethernet is defined.
  // b) ENABLE_WIFI is true AND Ethernet failed (cable disconnected or DHCP error).
  if (ENABLE_WIFI) {
    if (!internetReady) {
      Serial.println("[WIFI] Starting WiFi connection...");
      WiFi.mode(WIFI_STA);
      WiFi.begin(WIFI_SSID, WIFI_PASS);

      // We don't block here with an eternal while loop.
      // Let WiFi connect in the background.
      // The library will check WiFi.status() when necessary.
      Serial.println("[WIFI] Connection requested in background.");
    } else {
      Serial.println("[WIFI] Ethernet active. WiFi in standby (off) to save power.");
      WiFi.mode(WIFI_OFF);
    }
  }

  // ---------------------------------------------------------
  // 3. CONFIGURE OTA
  // ---------------------------------------------------------
  ota.begin(GH_OWNER, GH_REPO, FW_VERSION, GH_TOKEN);

  // ============================================================
  // TIME LOGIC: Schedule the FIRST execution
  // ============================================================
  // "millis() + 10000" means: Execute 10 seconds from now.
  nextOtaCheck = millis() + OTA_INITIAL_DELAY;

  Serial.printf("=== READY. First OTA check in %lu sec ===\n", OTA_INITIAL_DELAY / 1000);
}

void loop() {
  // Ethernet maintenance only if active and connected
  if (ENABLE_ETH) {
    Ethernet.maintain();
  }

  // ============================================================
  // OTA TIMER
  // ============================================================
  // Use casting to (long) to handle millis() overflow correctly
  if ((long)(millis() - nextOtaCheck) >= 0) {

    // 1. Execute check
    ota.checkForUpdate();

    // 2. Schedule the NEXT check (within 1 hour)
    nextOtaCheck = millis() + OTA_PERIODIC_INTERVAL;

    Serial.println("[LOOP] Next check scheduled in 1 hour.");
  }

  // Your blocking code or blink
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > BLINK_MS) {
    lastBlink = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}