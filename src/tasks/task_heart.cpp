#include <Arduino.h>
#include "task_heart.h"
#include "telemetry.h"
#include "heartRate.h"
#include "../drivers/max30105_driver.h"
#include "app_config.h"

extern QueueHandle_t g_ppgQueue;
extern SemaphoreHandle_t g_telemetryMutex;

#define RATE_SIZE 8

static float rates[RATE_SIZE];
static byte rateSpot = 0;

static float smoothedBPM = 0;
static float irAvg = 0;

static long lastBeat = 0;

static const float alphaFast = 0.3f;
static const float alphaSlow = 0.1f;

static const uint32_t fingerThreshold = 20000;

// ===============================
// CONTROLE DE EXIBIÇÃO
// ===============================
static float lastDisplayedBPM = 0;
static uint32_t lastDisplayTime = 0;
static int validBeatCount = 0;

void taskHeart(void *pvParameters)
{
    Serial.println("Heart task iniciada.");

    ppg_sample_t sample;

    while (true)
    {
        if (xQueueReceive(g_ppgHeartQueue, &sample, portMAX_DELAY))
        {
            long irValue = sample.ir;

            // ==========================
            // Média exponencial IR
            // ==========================
            irAvg = (irAvg * 0.9f) + (irValue * 0.1f);

            // ==========================
            // DETECÇÃO DE DEDO
            // ==========================
            if (irAvg < fingerThreshold)
            {
                smoothedBPM = 0;
                validBeatCount = 0;   // reset estabilidade

                if (xSemaphoreTake(g_telemetryMutex, pdMS_TO_TICKS(5))) {
                    g_telemetry.bpm = 0;
                    xSemaphoreGive(g_telemetryMutex);
                }

                continue;
            }

            // ==========================
            // DETECÇÃO DE BATIMENTO
            // ==========================
            if (checkForBeat(irValue))
            {
                long now = millis();
                long delta = now - lastBeat;
                lastBeat = now;

                if (delta < 300 || delta > 2000)
                    continue;

                float rawBPM = 60.0f / (delta / 1000.0f);

                if (rawBPM < 45 || rawBPM > 150)
                    continue;

                rates[rateSpot++] = rawBPM;
                rateSpot %= RATE_SIZE;

                float avgBPM = 0;
                for (byte i = 0; i < RATE_SIZE; i++)
                    avgBPM += rates[i];

                avgBPM /= RATE_SIZE;

                if (smoothedBPM == 0)
                {
                    smoothedBPM = avgBPM;
                }
                else
                {
                    float variation = abs(avgBPM - smoothedBPM);
                    float alpha;

                    if (variation < 5)
                        alpha = alphaFast;
                    else if (variation < 15)
                        alpha = alphaSlow;
                    else
                        continue;

                    smoothedBPM =
                        alpha * avgBPM +
                        (1 - alpha) * smoothedBPM;
                }

                // ==========================
                // Aguarda estabilidade inicial
                // ==========================
                validBeatCount++;

                if (validBeatCount < 5)
                    continue;

                // ==========================
                // Atualiza TELEMETRIA
                // ==========================
                if (xSemaphoreTake(g_telemetryMutex, pdMS_TO_TICKS(5))) {
                    g_telemetry.bpm = smoothedBPM;
                    xSemaphoreGive(g_telemetryMutex);
                }

                // ==========================
                // CONTROLE DE EXIBIÇÃO SERIAL
                // ==========================
                uint32_t nowPrint = millis();

                bool significantChange =
                    abs(smoothedBPM - lastDisplayedBPM) > 2.0f;

                bool timeoutReached =
                    (nowPrint - lastDisplayTime) > 5000;

                if (significantChange || timeoutReached)
                {
                    Serial.print("BPM: ");
                    Serial.println(smoothedBPM, 1);

                    lastDisplayedBPM = smoothedBPM;
                    lastDisplayTime = nowPrint;
                }
            }
        }
    }
}