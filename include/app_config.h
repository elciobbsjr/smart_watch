#pragma once

extern QueueHandle_t g_ppgQueue;

// ===== I2C =====
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define I2C_FREQ_HZ 400000

// ===== MPU6500 =====
#define MPU6500_I2C_ADDR 0x68  // AD0=0 normalmente

// ===== Sistema =====
#define SENSOR_SAMPLE_HZ 50    // 50 Hz
#define LOGGER_FLUSH_EVERY_N 10 // flush a cada N amostras (melhor segurança)



#pragma once

// ===== Wi-Fi =====
#define WIFI_SSID "ELCIO J"
#define WIFI_PASS "elc10jun10r"

// Tempo máximo tentando conectar (ms)
#define WIFI_CONNECT_TIMEOUT_MS 15000


// ===== MQTT =====
#define MQTT_HOST "87a490b8ab324342ab84c48829e097ce.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883
#define MQTT_USER "elciobsjr"
#define MQTT_PASS "9Fsp2T!PKmKWRPr"

