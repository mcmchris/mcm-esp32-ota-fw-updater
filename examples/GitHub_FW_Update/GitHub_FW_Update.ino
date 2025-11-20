#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <WiFi.h>

// ===== 1. INCLUIR LIBRERÍA =====
#include <MCM_GitHub_OTA.h>

// ========================================
// CONFIGURACIÓN DE PROTOCOLOS (¡MODIFICA AQUÍ!)
// ========================================
// Define qué hardware quieres usar.
// La librería solo reservará RAM para lo que actives aquí.
const bool ENABLE_ETH = true;
const bool ENABLE_WIFI = true;

// --- Tiempos OTA ---
const unsigned long OTA_INITIAL_DELAY = 10000;        // 10 segundos después del boot
const unsigned long OTA_PERIODIC_INTERVAL = 3600000;  // 1 hora (60 * 60 * 1000)

// Variable para controlar cuándo toca la siguiente revisión
unsigned long nextOtaCheck = 0;


// --- Blink Control ---
#define LED_PIN 1
static uint32_t BLINK_MS = 1000;

// ========================================
// CONFIGURACIÓN DE HARDWARE & RED
// ========================================

// --- Pines SPI Ethernet (W5500) ---
#define ETH_CS 33
#define ETH_SCLK 36
#define ETH_MISO 37
#define ETH_MOSI 35
#define ETH_RST -1

uint8_t mac[] = { 0x02, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };

// --- Credenciales WiFi ---
#define WIFI_SSID "CLARO403"
#define WIFI_PASS "2336879809"

// --- OTA GitHub ---
#define GH_OWNER "mcmchris"
#define GH_REPO "esp32-s2-ota-blink-ethernet"
#define FW_VERSION "v1.0.1"
#define GH_TOKEN "ghp_***************"

// ========================================
// INSTANCIA
// ========================================
// Al pasar las constantes aquí, la librería optimiza la memoria interna
MCM_GitHub_OTA ota(ENABLE_ETH, ENABLE_WIFI);

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== ARRANQUE INTELIGENTE DE RED ===");

  bool internetReady = false;

  // ---------------------------------------------------------
  // 1. BLOQUE ETHERNET (Solo se ejecuta si ENABLE_ETH es true)
  // ---------------------------------------------------------
  if (ENABLE_ETH) {
    Serial.println("[ETH] Iniciando Hardware Ethernet...");

    // A. Configurar SPI solo si usamos Ethernet
    SPI.begin(ETH_SCLK, ETH_MISO, ETH_MOSI);
    Ethernet.init(ETH_CS);

    // B. Reset Manual W5500
    if (ETH_RST >= 0) {
      pinMode(ETH_RST, OUTPUT);
      digitalWrite(ETH_RST, HIGH);
      delay(10);
      digitalWrite(ETH_RST, LOW);
      delay(10);
      digitalWrite(ETH_RST, HIGH);
      delay(10);
    }

    // C. VERIFICACIÓN DE CABLE (LINK STATUS)
    EthernetLinkStatus link = Ethernet.linkStatus();

    if (link == LinkOFF) {
      Serial.println("[ETH] Cable desconectado (LinkOFF). Saltando DHCP.");
    } else if (link == Unknown) {
      // A veces el W5500 tarda un instante en levantar el PHY, damos un mini respiro
      delay(100);
      if (Ethernet.linkStatus() == LinkOFF) {
        Serial.println("[ETH] Cable desconectado. Saltando DHCP.");
      } else {
        goto TRY_DHCP;  // Cable detectado
      }
    } else {
TRY_DHCP:
      Serial.println("[ETH] Cable detectado. Intentando DHCP...");
      // Solo entramos al timeout de DHCP si el cable está puesto
      if (Ethernet.begin(mac) == 0) {
        Serial.println("[ETH] Falló configuración DHCP.");
      } else {
        Serial.print("[ETH] Conectado! IP: ");
        Serial.println(Ethernet.localIP());
        internetReady = true;
      }
    }
  }

  // ---------------------------------------------------------
  // 2. BLOQUE WIFI (Lógica de respaldo o principal)
  // ---------------------------------------------------------
  // Entramos aquí si:
  // a) ENABLE_WIFI es true Y no hay Ethernet definido.
  // b) ENABLE_WIFI es true Y Ethernet falló (cable desconectado o error DHCP).
  if (ENABLE_WIFI) {
    if (!internetReady) {
      Serial.println("[WIFI] Iniciando conexión WiFi...");
      WiFi.mode(WIFI_STA);
      WiFi.begin(WIFI_SSID, WIFI_PASS);

      // No bloqueamos aquí con un while loop eterno.
      // Dejamos que el WiFi conecte en segundo plano.
      // La librería verificará WiFi.status() cuando sea necesario.
      Serial.println("[WIFI] Conexión solicitada en segundo plano.");
    } else {
      Serial.println("[WIFI] Ethernet activo. WiFi en standby (apagado) para ahorrar energía.");
      WiFi.mode(WIFI_OFF);
    }
  }

  // ---------------------------------------------------------
  // 3. CONFIGURAR OTA
  // ---------------------------------------------------------
  ota.begin(GH_OWNER, GH_REPO, FW_VERSION, GH_TOKEN);

  // ============================================================
  // LÓGICA DE TIEMPO: Programar la PRIMERA ejecución
  // ============================================================
  // "millis() + 10000" significa: Ejecutar 10 segundos después de ahora.
  nextOtaCheck = millis() + OTA_INITIAL_DELAY;

  Serial.printf("=== LISTO. Primera revisión OTA en %lu seg ===\n", OTA_INITIAL_DELAY / 1000);
}

void loop() {
  // Mantenimiento de Ethernet solo si está activo y conectado
  if (ENABLE_ETH) {
    Ethernet.maintain();
  }

  // ============================================================
  // TEMPORIZADOR OTA
  // ============================================================
  // Usamos casting a (long) para manejar el desbordamiento de millis() correctamente
  if ((long)(millis() - nextOtaCheck) >= 0) {

    // 1. Ejecutar la revisión
    ota.checkForUpdate();

    // 2. Programar la SIGUIENTE revisión (dentro de 1 hora)
    nextOtaCheck = millis() + OTA_PERIODIC_INTERVAL;

    Serial.println("[LOOP] Próxima revisión programada en 1 hora.");
  }

  // Tu código bloqueante o blink
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > BLINK_MS) {
    lastBlink = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}