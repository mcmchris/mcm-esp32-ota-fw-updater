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
#define WIFI_SSID "**********" // Your SSID
#define WIFI_PASS "**********" // Your Password

// --- GitHub OTA ---
#define GH_OWNER "**********" // Your GitHub User or Organization
#define GH_REPO "****************" // Your Repository
#define FW_VERSION "v1.0.2" // Current Firmware Version
#define GH_TOKEN "ghp_****************"  // Your GitHub Token (if needed)

// ========================================
// INSTANCE
// ========================================
// By passing the constants here, the library optimizes internal memory
MCM_GitHub_OTA ota(ENABLE_ETH, ENABLE_WIFI);

// Variable to track previous Ethernet state for hot-swapping
bool ethWasConnected = false;

void setup() {
  Serial.begin(115200);
  while(!Serial){}
  delay(2000);

  pinMode(LED_PIN, OUTPUT);

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
      Serial.print("[WIFI] Starting WiFi connection to ");
      Serial.println(WIFI_SSID);
      
      WiFi.mode(WIFI_STA);
      WiFi.begin(WIFI_SSID, WIFI_PASS);

      // === NEW: WAIT FOR CONNECTION LOOP ===
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
      // =====================================
      
    } else {
      Serial.println("[WIFI] Ethernet active. WiFi in standby.");
      // We don't turn it off completely if you want hot-swap, 
      // but keeping it disconnected saves interference/power.
      // WiFi.mode(WIFI_OFF); 
    }
  }

  // ---------------------------------------------------------
  // 3. CONFIGURE OTA
  // ---------------------------------------------------------
  ota.begin(GH_OWNER, GH_REPO, FW_VERSION, GH_TOKEN);
  ota.setSSLDebug(SSLClient::SSL_WARN);
  // ============================================================
  // TIME LOGIC: Schedule the FIRST execution
  // ============================================================
  // "millis() + 10000" means: Execute 10 seconds from now.
  nextOtaCheck = millis() + OTA_INITIAL_DELAY;

  Serial.printf("=== READY. First OTA check in %lu sec ===\n", OTA_INITIAL_DELAY / 1000);
}

void loop() {
  
  // 1. Manage Network Switching (Hot-Swap)
  manageNetworkRedundancy();

  // 2. Ethernet Maintenance (DHCP renewal) - Only if cable is connected
  if (ENABLE_ETH && ethWasConnected) {
    Ethernet.maintain();
  }

  // 3. OTA Timer
  if ((long)(millis() - nextOtaCheck) >= 0) {
    ota.checkForUpdate();
    nextOtaCheck = millis() + OTA_PERIODIC_INTERVAL;
    Serial.println("[LOOP] Next check scheduled in 1 hour.");
  }

  // 4. Blink
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > BLINK_MS) {
    lastBlink = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}

// Helper function to handle Redundancy logic
void manageNetworkRedundancy() {
  static unsigned long lastRedundancyCheck = 0;
  static unsigned long lastWifiAttempt = 0; // Para no saturar intentos de WiFi
  
  // Revisar estado físico cada 2 segundos (para detectar cable rápido)
  if (millis() - lastRedundancyCheck > 2000) {
    lastRedundancyCheck = millis();

    // --- ETHERNET CHECK ---
    if (ENABLE_ETH) {
      EthernetLinkStatus link = Ethernet.linkStatus();
      
      // CASE 1: Cable connected (Priority)
      if (link == LinkON) {
        if (!ethWasConnected) {
           Serial.println("[NET] Ethernet Cable Plugged in. Requesting DHCP...");
           
           // Attempt to bring up Ethernet
           Ethernet.begin(mac); 
           
           if (Ethernet.localIP() != IPAddress(0,0,0,0)) {
               Serial.print("[ETH] IP Obtained: ");
               Serial.println(Ethernet.localIP());
               ethWasConnected = true;
               
               // Optional: Turn off WiFi to save power and avoid conflicts
               Serial.println("[NET] Stopping WiFi to save power.");
               WiFi.disconnect();
               WiFi.mode(WIFI_OFF); 
           } else {
               Serial.println("[ETH] DHCP Failed.");
           }
        }
      }
      // CASE 2: Cable disconnected (Fallback)
      else if (link == LinkOFF) {
        if (ethWasConnected) {
          Serial.println("[NET] Ethernet Cable Unplugged. Switching to WiFi...");
          ethWasConnected = false;
          
          // IMPORTANT: Turn on the radio immediately
          WiFi.mode(WIFI_STA);
          WiFi.begin(WIFI_SSID, WIFI_PASS);
          lastWifiAttempt = millis(); // Reset attempt counter
        }
      }
    }

    // --- WIFI CHECK (Only if no Ethernet) ---
    if (ENABLE_WIFI && !ethWasConnected) {
      
      // If not connected...
      if (WiFi.status() != WL_CONNECTED) {
        
        // ... And 10 seconds have passed since the last attempt
        if (millis() - lastWifiAttempt > 10000) {
          lastWifiAttempt = millis(); // Save current time
          
          Serial.println("[NET] WiFi disconnected. Retrying connection...");
          
          // Ensure the radio is on
          if (WiFi.getMode() != WIFI_STA) {
             WiFi.mode(WIFI_STA);
          }
          
          // Using reconnect() is smoother than disconnect/begin
          // Do not call disconnect() here, it breaks the ongoing process.
          WiFi.reconnect(); 
        }
      }
    }
  }
}