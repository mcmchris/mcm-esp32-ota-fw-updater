#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <WiFi.h>

// ===== 1. INCLUIR TU NUEVA LIBRERÍA =====
#include <MCM_GitHub_OTA.h>

// ========================================
// CONFIGURACIÓN DEL USUARIO
// ========================================

// --- Configuración de Hardware (W5500 para ESP32-S2) ---
// TÚ defines los pines aquí, la librería no los conoce.
#define ETH_CS   33
#define ETH_SCLK 36
#define ETH_MISO 37
#define ETH_MOSI 35
#define ETH_RST  -1 

// Dirección MAC (Debe ser única en tu red)
uint8_t mac[] = { 0x02, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };

// --- Configuración WiFi ---
#define WIFI_SSID "CLARO403"
#define WIFI_PASS "2336879809"

// --- Configuración OTA GitHub ---
#define GH_OWNER "mcmchris"
#define GH_REPO  "esp32-s2-ota-blink-ethernet"
#define FW_VERSION "v1.0.1" // Cambia esto en el repositorio para disparar la OTA
// Si el repo es privado, pon tu token aquí. Si es público, déjalo vacío "".
#define GH_TOKEN "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" 

// Intervalo de chequeo (para pruebas: 60 segundos)
const unsigned long OTA_CHECK_INTERVAL = 60000; 

// Led de estado (Blink)
#define LED_PIN 1

// ========================================
// INSTANCIA GLOBAL
// ========================================
// (true, true) -> Habilitar soporte tanto para Ethernet como para WiFi
MCM_GitHub_OTA ota(true, true);

void setup() {
  Serial.begin(115200);
  delay(2000); // Dar tiempo al monitor serie
  
  pinMode(LED_PIN, OUTPUT);
  Serial.println("\n=== INICIANDO SISTEMA CON MCM_GITHUB_OTA ===");

  // ============================================================
  // PASO 1: INICIALIZACIÓN DE HARDWARE (Tu Responsabilidad)
  // ============================================================
  
  // A. Configurar el Bus SPI con tus pines personalizados
  Serial.println("[HW] Configurando SPI...");
  SPI.begin(ETH_SCLK, ETH_MISO, ETH_MOSI);

  // B. Inicializar Ethernet (W5500)
  Serial.println("[HW] Inicializando W5500...");
  Ethernet.init(ETH_CS);

  // Reset manual del chip Ethernet (si aplica)
  if (ETH_RST >= 0) {
    pinMode(ETH_RST, OUTPUT);
    digitalWrite(ETH_RST, HIGH); delay(10);
    digitalWrite(ETH_RST, LOW);  delay(10);
    digitalWrite(ETH_RST, HIGH); delay(10);
  }

  // Intentar conectar Ethernet por DHCP
  if (Ethernet.begin(mac) == 0) {
    Serial.println("[NET-ETH] Fallo DHCP o cable desconectado.");
    // No detenemos el código, quizás el WiFi funcione.
  } else {
    Serial.print("[NET-ETH] Conectado! IP: ");
    Serial.println(Ethernet.localIP());
  }

  // C. Inicializar WiFi (sin bloquear)
  Serial.println("[HW] Conectando WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  // No esperamos el while(connected) aquí para no bloquear el arranque,
  // la librería verificará el estado cuando sea necesario.

  // ============================================================
  // PASO 2: INICIALIZACIÓN DE LA LIBRERÍA
  // ============================================================
  Serial.println("[OTA] Configurando MCM_GitHub_OTA...");
  
  // Pasamos los datos del repo. La librería detectará automáticamente
  // si hay red (Ethernet o WiFi) cuando llames a checkForUpdate().
  ota.begin(GH_OWNER, GH_REPO, FW_VERSION, GH_TOKEN);

  Serial.println("--- Setup Completo ---\n");
}

void loop() {
  // 1. Mantener el lease DHCP de Ethernet activo
  Ethernet.maintain();

  // 2. Lógica de OTA Periódica
  static unsigned long lastOtaCheck = 0;
  if (millis() - lastOtaCheck > OTA_CHECK_INTERVAL) {
    lastOtaCheck = millis();
    
    // Esta es la única línea que necesitas llamar periódicamente
    ota.checkForUpdate();
  }

  // 3. Tu código principal (Blink no bloqueante)
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 500) {
    lastBlink = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}