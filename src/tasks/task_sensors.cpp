#include <Arduino.h>
#include "task_sensors.h"
#include "drivers/mpu6500.h"
#include "config.h"
#include "mqtt_mgr.h"


#if DEBUG_MODE
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif


static unsigned long freeFallStart = 0;

// =========================
// CONFIGURAÇÃO
// =========================
#define SAMPLE_RATE_MS 10

/*#define FREE_FALL_THRESHOLD_G   0.85
#define IMPACT_THRESHOLD_G      1.4
#define JERK_THRESHOLD          3.0
#define GYRO_THRESHOLD          80.0

#define IMMOBILITY_TIME_MS      3000
#define IMMOBILITY_GYRO_LIMIT   6.0 valores proximos da realidade*/

#define FREE_FALL_THRESHOLD_G   0.95   // praticamente qualquer alívio vira free fall
#define IMPACT_THRESHOLD_G      1.2    // impacto muito leve já conta
#define JERK_THRESHOLD          0.8    // qualquer variação registra
#define GYRO_THRESHOLD          30.0   // giros leves contam
#define IMMOBILITY_TIME_MS      500    // 0.2s parado já confirma
#define IMMOBILITY_GYRO_LIMIT   50.0   // quase qualquer redução de giro confirma





#define DEBUG_INTERVAL_MS       2000
#define EVENT_BUFFER_SIZE       200

// =========================
// Estrutura Evento
// =========================
typedef struct {
    unsigned long timestamp;
    float pitch;
    float roll;
    float accMag;
    float gyroMag;
    float jerk;
    int state;
} imu_event_t;

static imu_event_t eventBuffer[EVENT_BUFFER_SIZE];
static int eventIndex = 0;

// =========================
// Offsets
// =========================
static float gyro_offset_x = 0;
static float gyro_offset_y = 0;
static float gyro_offset_z = 0;

static float acc_offset_x = 0;
static float acc_offset_y = 0;
static float acc_offset_z = 0;

// =========================
// Estados
// =========================
enum FallState {
    NORMAL,
    FREE_FALL,
    IMPACT,
    IMMOBILITY_CHECK
};

static FallState fallState = NORMAL;
static unsigned long immobilityStart = 0;

// =========================
// Armazenar evento silencioso
// =========================
static void store_event(float pitch,
                        float roll,
                        float accMag,
                        float gyroMag,
                        float jerk,
                        int state)
{
    eventBuffer[eventIndex].timestamp = millis();
    eventBuffer[eventIndex].pitch = pitch;
    eventBuffer[eventIndex].roll = roll;
    eventBuffer[eventIndex].accMag = accMag;
    eventBuffer[eventIndex].gyroMag = gyroMag;
    eventBuffer[eventIndex].jerk = jerk;
    eventBuffer[eventIndex].state = state;

    eventIndex++;
    if (eventIndex >= EVENT_BUFFER_SIZE)
        eventIndex = 0;
}

// =========================
// Calibração
// =========================
static void calibrate_mpu() {
  const int samples = 1500;
  int ok = 0;

  float sum_gx=0, sum_gy=0, sum_gz=0;
  float sum_ax=0, sum_ay=0, sum_az=0;

  mpu_data_t d;

  Serial.println("=== CALIBRANDO MPU ===");
  vTaskDelay(pdMS_TO_TICKS(500));

  for (int i = 0; i < samples; i++) {
    if (!mpu_read_raw(&d)) {
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    sum_gx += d.gx; sum_gy += d.gy; sum_gz += d.gz;
    sum_ax += d.ax; sum_ay += d.ay; sum_az += d.az;

    ok++;
    vTaskDelay(pdMS_TO_TICKS(3));
  }

  if (ok < 200) { // mínimo aceitável
    Serial.println("[MPU] Calibracao falhou (poucas leituras validas).");
    gyro_offset_x = gyro_offset_y = gyro_offset_z = 0;
    acc_offset_x = acc_offset_y = acc_offset_z = 0;
    return;
  }

  gyro_offset_x = sum_gx / ok;
  gyro_offset_y = sum_gy / ok;
  gyro_offset_z = sum_gz / ok;

  acc_offset_x = sum_ax / ok;
  acc_offset_y = sum_ay / ok;
  acc_offset_z = (sum_az / ok) - 1.0f;

  Serial.println("Calibracao concluida!");
}

// =========================
// TASK PRINCIPAL
// =========================
void task_sensors(void *pvParameters) {

    mpu_data_t d;

    if (!mpu_init()) {
    #if DEBUG_MODE
        Serial.println("[MPU] Falha ao iniciar MPU!");
    #endif
        vTaskDelete(NULL);
    }

    calibrate_mpu();

    float pitch = 0;
    float roll = 0;
    float prevAccMag = 1.0;

    unsigned long lastTime = millis();
    unsigned long lastDebug = 0;

    FallState previousState = NORMAL;


    while (true) {
        // =============================
        // LEITURA ÚNICA E VALIDADA
        // =============================
        static int failCount = 0;

        if (!mpu_read_raw(&d)) {

            failCount++;

            if (failCount % 20 == 0)
                Serial.println("[MPU] Falha I2C (retry)");

            vTaskDelay(pdMS_TO_TICKS(30));
            continue;
        }

        failCount = 0;

        // Aplicar offsets APÓS leitura válida
        d.gx -= gyro_offset_x;
        d.gy -= gyro_offset_y;
        d.gz -= gyro_offset_z;

        d.ax -= acc_offset_x;
        d.ay -= acc_offset_y;
        d.az -= acc_offset_z;

        // =============================
        // Delta tempo estável
        // =============================
        unsigned long now = millis();
        float dt = (now - lastTime) / 1000.0;
        if (dt <= 0.001) dt = 0.01;  // proteção contra dt zero
        lastTime = now;

        // =============================
        // Magnitudes
        // =============================
        float accMag = sqrt(d.ax*d.ax + d.ay*d.ay + d.az*d.az);
        float gyroMag = sqrt(d.gx*d.gx + d.gy*d.gy + d.gz*d.gz);

        // =============================
        // JERK
        // =============================
        float jerk = (accMag - prevAccMag) / dt;
        prevAccMag = accMag;

        // =============================
        // Filtro Complementar (estável)
        // =============================
        float accPitch = atan2(d.ay, sqrt(d.ax*d.ax + d.az*d.az)) * 180.0 / PI;
        float accRoll  = atan2(-d.ax, d.az) * 180.0 / PI;

        pitch = 0.96 * (pitch + d.gx * dt) + 0.04 * accPitch;
        roll  = 0.96 * (roll  + d.gy * dt) + 0.04 * accRoll;

        // Limitação física
        if (pitch > 180) pitch -= 360;
        if (pitch < -180) pitch += 360;
        if (roll > 180) roll -= 360;
        if (roll < -180) roll += 360;

        // =============================
        // Máquina de estados de queda
        // =============================
        switch (fallState) {

            case NORMAL:
                if (accMag < FREE_FALL_THRESHOLD_G) {
                    fallState = FREE_FALL;
                    freeFallStart = now;
                }
                break;

            case FREE_FALL:

                // Se detectar impacto dentro de 1 segundo
                if (accMag > IMPACT_THRESHOLD_G) {
                    fallState = IMPACT;
                    store_event(pitch, roll, accMag, gyroMag, jerk, fallState);
                }

                // Se passou muito tempo sem impacto → cancela
                else if (now - freeFallStart > 1000) {
                    fallState = NORMAL;
                }

                break;


            case IMPACT:
                fallState = IMMOBILITY_CHECK;
                immobilityStart = now;
                break;

            case IMMOBILITY_CHECK:
                if (gyroMag < IMMOBILITY_GYRO_LIMIT &&
                    accMag > 0.85 && accMag < 1.15)
                {
                    if (now - immobilityStart > IMMOBILITY_TIME_MS) {
                        #if DEBUG_MODE
                        Serial.println("QUEDA CONFIRMADA!");
                    #else
                        Serial.println("ALERTA: QUEDA CONFIRMADA!");
                    #endif

                    store_event(pitch, roll, accMag, gyroMag, jerk, fallState);
                    fallState = NORMAL;

                    }
                } else {
                    fallState = NORMAL;
                }
                break;
        }

        /*if (fallState != previousState) {

        #if DEBUG_MODE

            switch (fallState) {
                case NORMAL:
                    Serial.println("Estado -> NORMAL");
                    break;

                case FREE_FALL:
                    Serial.println("Estado -> POSSIVEL QUEDA (FREE FALL)");
                    break;

                case IMPACT:
                    Serial.println("Estado -> IMPACTO DETECTADO!");
                    break;

                case IMMOBILITY_CHECK:
                    Serial.println("Estado -> VERIFICANDO IMOBILIDADE...");
                    break;
            }

        #else
            // MODO PRODUÇÃO → só alertas importantes
            if (fallState == IMPACT) {
                Serial.println("ALERTA: IMPACTO DETECTADO!");
            }

            if (fallState == IMMOBILITY_CHECK) {
                Serial.println("ALERTA: POSSIVEL QUEDA CONFIRMADA!");
            }

        #endif

            previousState = fallState;
        }*/

        if (fallState != previousState) {

        #if DEBUG_MODE

            // MODO DEBUG → detalhado
            switch (fallState) {
                case NORMAL:
                    Serial.println("Estado -> NORMAL");
                    break;

                case FREE_FALL:
                    Serial.println("Estado -> POSSIVEL QUEDA (FREE FALL)");
                    break;

                case IMPACT:
                    Serial.println("Estado -> IMPACTO DETECTADO!");
                    break;

                case IMMOBILITY_CHECK:
                    Serial.println("Estado -> VERIFICANDO IMOBILIDADE...");
                    break;
            }

        #else

            // MODO PRODUÇÃO → apenas status importantes
            switch (fallState) {
                case FREE_FALL:
                    Serial.println("POSSIVEL QUEDA DETECTADA");
                    break;

                case IMPACT:
                    Serial.println("IMPACTO DETECTADO!");
                    break;

                case IMMOBILITY_CHECK:
                    Serial.println("VERIFICANDO IMOBILIDADE...");
                    break;

                default:
                    break; // não imprime NORMAL
            }

        #endif

            previousState = fallState;
        }




        // =============================
        // Eventos estilo Apple (Jerk + Giro)
        // =============================
        if (fabs(jerk) > JERK_THRESHOLD)
            store_event(pitch, roll, accMag, gyroMag, jerk, fallState);

        if (gyroMag > GYRO_THRESHOLD)
            store_event(pitch, roll, accMag, gyroMag, jerk, fallState);

        // =============================
        // DEBUG controlado
        // =============================
        if (DEBUG_MODE && (now - lastDebug > DEBUG_INTERVAL_MS)) {

            /*DEBUG_PRINTF(
            "Pitch: %.2f | Roll: %.2f | accMag: %.2f | gyroMag: %.2f | jerk: %.2f | State: %d\n",
            pitch, roll, accMag, gyroMag, jerk, fallState
            );

            /*DEBUG_PRINTF(
                "Pitch: %.2f | Roll: %.2f | State: %d\n",
                pitch, roll, fallState
            );*/

            lastDebug = now;
        }

        // =============================
        // ENVIO MQTT
        // =============================
        static unsigned long lastMqttSend = 0;

        if (millis() - lastMqttSend > 500) {

            if (mqtt_is_connected()) {

                String payload = "{";
                payload += "\"pitch\":" + String(pitch, 2) + ",";
                payload += "\"roll\":" + String(roll, 2) + ",";
                payload += "\"accMag\":" + String(accMag, 2) + ",";
                payload += "\"gyroMag\":" + String(gyroMag, 2) + ",";
                payload += "\"jerk\":" + String(jerk, 2) + ",";
                payload += "\"state\":" + String(fallState);
                payload += "}";

                mqtt_publish("smartwatch/imu", payload);
            }

            lastMqttSend = millis();
        }


        vTaskDelay(pdMS_TO_TICKS(SAMPLE_RATE_MS));
    }
}

