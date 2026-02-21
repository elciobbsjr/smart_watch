#include <Arduino.h>
#include <Wire.h>
#include "task_sensor_acq.h"
#include "../drivers/max30105_driver.h"
#include "app_config.h"

extern SemaphoreHandle_t g_i2cMutex;
extern QueueHandle_t g_ppgQueue;

static MAX30105Driver heartSensor;

void taskSensorAcq(void *pvParameters)
{
    // ============================
    // Inicializa sensor (SOMENTE AQUI)
    // ============================
    if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(100))) {
        if (!heartSensor.begin(Wire)) {
            Serial.println("MAX30105 nao encontrado!");
            xSemaphoreGive(g_i2cMutex);
            vTaskDelete(NULL);
        }
        xSemaphoreGive(g_i2cMutex);
    }

    Serial.println("Sensor acquisition task iniciada.");

    uint32_t debugCounter = 0;

    while (true)
    {
        if (xSemaphoreTake(g_i2cMutex, pdMS_TO_TICKS(20)))
        {
            ppg_sample_t sample;

            if (heartSensor.readSample(sample))
            {
                // Envia para queue
                xQueueSend(g_ppgHeartQueue, &sample, 0);
                xQueueSend(g_ppgSpo2Queue,  &sample, 0);

                /* DEBUG temporário a cada ~1s
                debugCounter++;
                if (debugCounter >= 200) { // 200 * 5ms ≈ 1s
                    Serial.print("IR: ");
                    Serial.print(sample.ir);
                    Serial.print("  RED: ");
                    Serial.println(sample.red);
                    debugCounter = 0;
                }*/
            }

            xSemaphoreGive(g_i2cMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(5));  // ~200Hz
    }
}