#include <Arduino.h>
#include <Wire.h>
#include "task_heart.h"
#include "../drivers/max30105_driver.h"
#include "config.h"

extern SemaphoreHandle_t g_i2cMutex;

static MAX30105Driver heartSensor;

void taskHeart(void *pvParameters)
{
    if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(100))) {
        if (!heartSensor.begin(Wire)) {
            Serial.println("MAX30105 nao encontrado!");
            xSemaphoreGive(g_i2cMutex);
            vTaskDelete(NULL);
        }
        xSemaphoreGive(g_i2cMutex);
    }

    Serial.println("Heart task iniciada.");

    // ==============================
    // CONTROLE DE INTERVALO SERIAL
    // ==============================
#if DEBUG_MODE
    const uint32_t PRINT_INTERVAL_MS = 500;   // Debug mais rápido
#else
    const uint32_t PRINT_INTERVAL_MS = 300;  // Produção mais limpo
#endif

    uint32_t lastPrint = 0;

    while (true) {

        if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(20))) {
            heartSensor.update();
            xSemaphoreGive(g_i2cMutex);
        }

        if (heartSensor.fingerDetected()) {

            float bpm = heartSensor.getBPM();

            if (bpm > 0) {

                uint32_t now = millis();

                if (now - lastPrint >= PRINT_INTERVAL_MS) {

                    Serial.print("BPM: ");
                    Serial.println(bpm, 1);

                    lastPrint = now;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}