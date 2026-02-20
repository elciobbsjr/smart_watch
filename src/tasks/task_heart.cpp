#include <Arduino.h>
#include <Wire.h>
#include "task_heart.h"
#include "../drivers/max30105_driver.h"

extern SemaphoreHandle_t g_i2cMutex;  // <<< IMPORTANTE

static MAX30105Driver heartSensor;

void taskHeart(void *pvParameters)
{
    // NÃO chamar Wire.begin() aqui!
    // Já foi inicializado no main()

    // Protege inicialização do sensor
    if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(100))) {
        if (!heartSensor.begin(Wire)) {
            Serial.println("MAX30105 nao encontrado!");
            xSemaphoreGive(g_i2cMutex);
            vTaskDelete(NULL);
        }
        xSemaphoreGive(g_i2cMutex);
    }

    Serial.println("Heart task iniciada.");

    while (true) {

        if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(20))) {

            heartSensor.update();

            xSemaphoreGive(g_i2cMutex);
        }

        if (heartSensor.fingerDetected()) {
            float bpm = heartSensor.getBPM();
            if (bpm > 0) {
                Serial.print("BPM: ");
                Serial.println(bpm, 1);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5)); // 200 Hz
    }
}