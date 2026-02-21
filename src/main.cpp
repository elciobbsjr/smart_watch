#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

#include "tasks/task_sensors.h"
#include "tasks/task_heart.h"   // <<< NOVA TASK
#include "config.h"
#include "i2c_bus.h"
#include "app_config.h"
#include "mqtt_mgr.h"
#include "tasks/task_telemetry.h"
#include "drivers/max30105_driver.h"
#include "tasks/task_sensor_acq.h"
#include "tasks/task_spo2.h"


volatile bool g_systemReady = false;
// ===============================
// Mutex global do barramento I2C
// ===============================

SemaphoreHandle_t g_i2cMutex = nullptr;
SemaphoreHandle_t g_telemetryMutex = nullptr;



QueueHandle_t g_ppgHeartQueue;
QueueHandle_t g_ppgSpo2Queue;


// ===============================
// FUNÇÃO CONEXÃO WIFI
// ===============================
static void wifi_connect() {

#if DEBUG_MODE
    Serial.println("[WIFI] Conectando...");
#endif

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
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
// FUNÇÃO CONEXÃO MQTT
// ===============================
static void mqtt_connect_blocking() {

    if (WiFi.status() != WL_CONNECTED)
        return;

#if DEBUG_MODE
    Serial.println("[MQTT] Conectando...");
#endif

    if (mqtt_connect()) {
#if DEBUG_MODE
        Serial.println("[MQTT] Conectado ao broker!");
#endif

        mqtt_publish("smartwatch/status", "online");
    } else {
#if DEBUG_MODE
        Serial.println("[MQTT] Falha na conexão.");
#endif
    }
}

// ===============================
// SETUP
// ===============================
void setup() {

    g_ppgHeartQueue = xQueueCreate(32, sizeof(ppg_sample_t));
    g_ppgSpo2Queue  = xQueueCreate(32, sizeof(ppg_sample_t));

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
    // CRIA MUTEX DA TELEMETRY
    // =========================
    g_telemetryMutex = xSemaphoreCreateMutex();

    if (g_telemetryMutex == NULL) {
    #if DEBUG_MODE
        Serial.println("Erro ao criar mutex Telemetry!");
    #endif
        while (1);
    }

    // =========================
    // INICIALIZA I2C
    // =========================
    Wire.begin(21, 22);
    Wire.setClock(100000);
    Wire.setTimeout(50);

    #if DEBUG_MODE
        Serial.println("I2C inicializado");
    #endif

        delay(200);

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
    // CONECTA MQTT
    // =========================
    mqtt_connect_blocking();

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
        1
    );

    // =========================
    // CRIA TASK DO HEART RATE
    // =========================
    xTaskCreatePinnedToCore(
        taskHeart,
        "TaskHeart",
        4096,
        NULL,
        1,
        NULL,
        1
    );

    xTaskCreatePinnedToCore(
    taskSensorAcq,
    "SensorAcq",
    4096,
    NULL,
    2,
    NULL,
    1
    );

    xTaskCreatePinnedToCore(
    taskSpO2,
    "TaskSpO2",
    4096,
    NULL,
    1,
    NULL,
    1
    );

    xTaskCreatePinnedToCore(
    task_telemetry,
    "TaskTelemetry",
    4096,
    NULL,
    1,
    NULL,
    1
);
}

// ===============================
// LOOP PRINCIPAL
// ===============================
void loop() {

    // Reconecta WiFi se cair
    if (WiFi.status() != WL_CONNECTED) {
    #if DEBUG_MODE
            Serial.println("[WIFI] Reconectando...");
    #endif
            wifi_connect();
        }

        // Reconecta MQTT se cair
        if (!mqtt_is_connected()) {
    #if DEBUG_MODE
            Serial.println("[MQTT] Reconectando...");
    #endif
            mqtt_connect_blocking();
        }

    mqtt_loop();

    delay(2000);
}