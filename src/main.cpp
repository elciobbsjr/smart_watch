#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

#include "tasks/task_sensors.h"
#include "config.h"
#include "i2c_bus.h"
#include "app_config.h"   // SSID e senha

// ===============================
// Mutex global do barramento I2C
// ===============================
SemaphoreHandle_t g_i2cMutex = nullptr;

// ===============================
// FUNÇÃO CONEXÃO WIFI
// ===============================
static void wifi_connect() {

#if DEBUG_MODE
    Serial.println("[WIFI] Conectando...");
#endif

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);   // evita instabilidade em IoT
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long startAttempt = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - startAttempt < WIFI_CONNECT_TIMEOUT_MS) {

        delay(300);
#if DEBUG_MODE
        Serial.print(".");
#endif
    }

#if DEBUG_MODE
    Serial.println();
#endif

    if (WiFi.status() == WL_CONNECTED) {
#if DEBUG_MODE
        Serial.print("[WIFI] Conectado! IP: ");
        Serial.println(WiFi.localIP());
#endif
    } else {
#if DEBUG_MODE
        Serial.println("[WIFI] Falha ao conectar.");
#endif
    }
}

// ===============================
// SETUP
// ===============================
void setup() {

#if DEBUG_MODE
    Serial.begin(115200);
    delay(1500);
    Serial.println("\n=== BOOT SISTEMA ===");
#endif

    // =========================
    // CRIA MUTEX DO I2C
    // =========================
    g_i2cMutex = xSemaphoreCreateMutex();

    if (g_i2cMutex == NULL) {
#if DEBUG_MODE
        Serial.println("Erro ao criar mutex I2C!");
#endif
        while (1);
    }

    // =========================
    // INICIALIZA I2C
    // =========================
    Wire.begin(21, 22);      // SDA, SCL
    Wire.setClock(100000);   // 100kHz estável
    Wire.setTimeout(50);

#if DEBUG_MODE
    Serial.println("I2C inicializado");
#endif

    delay(200);

    // =========================
    // SCANNER I2C (DEBUG)
    // =========================
#if DEBUG_MODE
    Serial.println("=== Scanner I2C ===");

    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.print("Encontrado dispositivo em: 0x");
            Serial.println(address, HEX);
        }
    }

    Serial.println("=====================");
#endif

    // =========================
    // CONECTA WIFI
    // =========================
    wifi_connect();

    // =========================
    // CRIA TASK DOS SENSORES
    // =========================
    xTaskCreatePinnedToCore(
        task_sensors,
        "TaskSensors",
        4096,
        NULL,
        1,
        NULL,
        1   // Core 1
    );
}

// ===============================
// LOOP PRINCIPAL
// ===============================
void loop() {

    // Reconexão automática simples
    if (WiFi.status() != WL_CONNECTED) {
#if DEBUG_MODE
        Serial.println("[WIFI] Reconectando...");
#endif
        wifi_connect();
    }

    delay(5000);
}
