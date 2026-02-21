#include <Arduino.h>
#include "task_spo2.h"
#include "drivers/max30105_driver.h"
#include "telemetry.h"

extern QueueHandle_t g_ppgSpo2Queue;
extern SemaphoreHandle_t g_telemetryMutex;

#define WINDOW_SIZE      200      // ~1 s com 200 Hz
#define UPDATE_INTERVAL  2000     // recalcula a cada 2 s
#define MIN_IR_FINGER    30000    // dedo bem apoiado
#define MIN_BPM_VALIDO   45.0f    // só calcula SpO2 se já tiver FC estável

static float irBuffer[WINDOW_SIZE];
static float redBuffer[WINDOW_SIZE];

static int  bufferIndex       = 0;
static bool bufferFilled      = false;

static float smoothedSpO2     = 0.0f;
static float lastPrintedSpO2  = 0.0f;
static uint32_t lastUpdateTime  = 0;
static uint32_t lastPrintTime   = 0;

void taskSpO2(void *pvParameters)
{
    Serial.println("SpO2 task iniciada.");

    ppg_sample_t sample;

    while (true)
    {
        if (xQueueReceive(g_ppgSpo2Queue, &sample, portMAX_DELAY))
        {
            // 1) Só roda se já tiver BPM válido
            float currentBPM = 0.0f;
            if (xSemaphoreTake(g_telemetryMutex, pdMS_TO_TICKS(5)))
            {
                currentBPM = g_telemetry.bpm;
                xSemaphoreGive(g_telemetryMutex);
            }

            if (currentBPM < MIN_BPM_VALIDO)
            {
                // ainda não estabilizou o batimento — ignora por enquanto
                continue;
            }

            // 2) Dedo bem apoiado no sensor
            if (sample.ir < MIN_IR_FINGER)
                continue;

            // 3) Alimenta a janela deslizante
            irBuffer[bufferIndex]  = sample.ir;
            redBuffer[bufferIndex] = sample.red;

            bufferIndex++;
            if (bufferIndex >= WINDOW_SIZE)
            {
                bufferIndex = 0;
                bufferFilled = true;
            }

            if (!bufferFilled)
                continue;  // ainda não temos janela cheia

            uint32_t now = millis();
            if (now - lastUpdateTime < UPDATE_INTERVAL)
                continue;  // respeita intervalo mínimo entre cálculos

            lastUpdateTime = now;

            // 4) Calcula componente DC (média)
            float irDC = 0.0f, redDC = 0.0f;
            for (int i = 0; i < WINDOW_SIZE; i++)
            {
                irDC  += irBuffer[i];
                redDC += redBuffer[i];
            }
            irDC  /= WINDOW_SIZE;
            redDC /= WINDOW_SIZE;

            // 5) Calcula componente AC (RMS)
            float irAC = 0.0f, redAC = 0.0f;
            for (int i = 0; i < WINDOW_SIZE; i++)
            {
                float dir  = irBuffer[i]  - irDC;
                float dred = redBuffer[i] - redDC;
                irAC  += dir  * dir;
                redAC += dred * dred;
            }
            irAC  = sqrt(irAC  / WINDOW_SIZE);
            redAC = sqrt(redAC / WINDOW_SIZE);

            if (irAC <= 0.0f || irDC <= 0.0f || redDC <= 0.0f)
                continue;

            // 6) Calcula R e SpO2 (modelo simplificado)
            float R    = (redAC / redDC) / (irAC / irDC);
            float spo2 = 110.0f - 25.0f * R;

            // Limita faixa razoável
            if (spo2 > 100.0f) spo2 = 100.0f;
            if (spo2 < 88.0f)  spo2 = 88.0f;  // evita valores absurdos pra baixo

            // 7) Filtro exponencial forte (bem suave)
            if (smoothedSpO2 == 0.0f)
                smoothedSpO2 = spo2;
            else
                smoothedSpO2 = 0.08f * spo2 + 0.92f * smoothedSpO2;

            // 8) Atualiza na estrutura de telemetria
            if (xSemaphoreTake(g_telemetryMutex, pdMS_TO_TICKS(5)))
            {
                g_telemetry.spo2 = smoothedSpO2;
                xSemaphoreGive(g_telemetryMutex);
            }

            // 9) Anti-spam no Serial: só mostra se mudar o suficiente
            //    E no máximo a cada 2 s
            if ((now - lastPrintTime) > 2000 &&
                fabsf(smoothedSpO2 - lastPrintedSpO2) > 0.7f)
            {
                Serial.print("SpO2: ");
                Serial.print(smoothedSpO2, 1);
                Serial.println(" %");

                lastPrintedSpO2 = smoothedSpO2;
                lastPrintTime   = now;
            }
        }
    }
}