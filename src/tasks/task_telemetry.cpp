#include <Arduino.h>
#include "telemetry.h"
#include "mqtt_mgr.h"

void task_telemetry(void *pvParameters)
{
    const uint32_t SEND_INTERVAL_MS = 500;

    while (true) {

        if (mqtt_is_connected()) {

            String payload = "{";
            payload += "\"device_id\":\"operador_001\",";
            payload += "\"timestamp\":" + String(millis()) + ",";
            payload += "\"pitch\":" + String(g_telemetry.pitch, 2) + ",";
            payload += "\"roll\":" + String(g_telemetry.roll, 2) + ",";
            payload += "\"accMag\":" + String(g_telemetry.accMag, 2) + ",";
            payload += "\"gyroMag\":" + String(g_telemetry.gyroMag, 2) + ",";
            payload += "\"jerk\":" + String(g_telemetry.jerk, 2) + ",";
            payload += "\"bpm\":" + String(g_telemetry.bpm, 1) + ",";
            payload += "\"spo2\":" + String(g_telemetry.spo2, 1) + ",";
            payload += "\"state_beta1\":" + String(g_telemetry.state_beta1);
            
            payload += "}";

            mqtt_publish("smartwatch/telemetry", payload);
        }

        vTaskDelay(pdMS_TO_TICKS(SEND_INTERVAL_MS));
    }
}