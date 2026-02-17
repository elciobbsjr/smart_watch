#pragma once

// ===== I2C =====
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define I2C_FREQ_HZ 400000

// ===== MPU6500 =====
#define MPU6500_I2C_ADDR 0x68  // AD0=0 normalmente

// ===== Sistema =====
#define SENSOR_SAMPLE_HZ 50    // 50 Hz
#define LOGGER_FLUSH_EVERY_N 10 // flush a cada N amostras (melhor segurança)
