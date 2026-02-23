#include <Arduino.h>
#include "../include/infraestrutura.h"
#include "tasks/task_sensors.h"
#include "tasks/task_heart.h"
#include "tasks/task_sensor_acq.h"
#include "tasks/task_spo2.h"
#include "tasks/task_telemetry.h"
#include "config.h"

void setup() {

#if DEBUG_MODE
    Serial.begin(115200);
    delay(1500);
    Serial.println("\n=== BOOT SISTEMA ===");
#endif

    infraestrutura_inicializar();

    // =========================
    // CRIA TASKS (fica aqui)
    // =========================

    xTaskCreatePinnedToCore(task_sensors,"TaskSensors",4096,NULL,1,NULL,1);
    xTaskCreatePinnedToCore(taskHeart,"TaskHeart",4096,NULL,1,NULL,1);
    xTaskCreatePinnedToCore(taskSensorAcq,"SensorAcq",4096,NULL,2,NULL,1);
    xTaskCreatePinnedToCore(taskSpO2,"TaskSpO2",4096,NULL,1,NULL,1);
    xTaskCreatePinnedToCore(task_telemetry,"TaskTelemetry",4096,NULL,1,NULL,1);
}

void loop() {
    infraestrutura_loop();
    delay(2000);
}