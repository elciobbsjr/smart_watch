#include <Arduino.h>
#include <Wire.h>
#include "tasks/task_sensors.h"
#include "config.h"
#include "i2c_bus.h"

// Mutex global do barramento I2C
SemaphoreHandle_t g_i2cMutex = nullptr;

void setup() {

#if DEBUG_MODE
    Serial.begin(115200);
    delay(2000);
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
    // INICIALIZA I2C UMA VEZ
    // =========================
    Wire.begin(21, 22);      // SDA, SCL
    Wire.setClock(100000);   // 100kHz estável
    Wire.setTimeout(50);     // evita travamentos longos

#if DEBUG_MODE
    Serial.println("I2C inicializado");
#endif

    delay(200);

    // =========================
    // SCANNER I2C
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

void loop() {
    // Nada aqui (FreeRTOS controla tudo)
}
