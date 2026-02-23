#include "../include/infraestrutura.h"
#include <Wire.h>
#include <WiFi.h>
#include "app_config.h"
#include "mqtt_mgr.h"
#include "drivers/max30105_driver.h"
#include "config.h"

extern SemaphoreHandle_t g_i2cMutex;
extern SemaphoreHandle_t g_telemetryMutex;
extern QueueHandle_t g_ppgHeartQueue;
extern QueueHandle_t g_ppgSpo2Queue;


// ============================
// DEFINIÇÃO REAL DAS GLOBAIS
// ============================

SemaphoreHandle_t g_i2cMutex = nullptr;
SemaphoreHandle_t g_telemetryMutex = nullptr;

QueueHandle_t g_ppgHeartQueue = nullptr;
QueueHandle_t g_ppgSpo2Queue  = nullptr;

volatile bool g_systemReady = false;

// =========================
// Inicialização geral
// =========================
void infraestrutura_inicializar() {

    // Filas
    g_ppgHeartQueue = xQueueCreate(32, sizeof(ppg_sample_t));
    g_ppgSpo2Queue  = xQueueCreate(32, sizeof(ppg_sample_t));

    // Mutex
    g_i2cMutex = xSemaphoreCreateMutex();
    g_telemetryMutex = xSemaphoreCreateMutex();

    // I2C
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_FREQ_HZ);
    Wire.setTimeout(50);

#if DEBUG_MODE
    Serial.println("I2C inicializado");
#endif

    // WiFi
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long inicio = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - inicio < WIFI_CONNECT_TIMEOUT_MS) {
        delay(300);
    }

    // MQTT
    if (WiFi.status() == WL_CONNECTED) {
        if (mqtt_connect()) {
            mqtt_publish("smartwatch/status", "online");
        }
    }
}

// =========================
// Loop da infraestrutura
// =========================
void infraestrutura_loop() {

    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(WIFI_SSID, WIFI_PASS);
    }

    if (!mqtt_is_connected()) {
        mqtt_connect();
    }

    mqtt_loop();
}