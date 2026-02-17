#include "mpu6500.h"
#include <Wire.h>
#include "i2c_bus.h"

#define MPU_ADDR        0x68
#define REG_PWR_MGMT_1  0x6B
#define REG_WHO_AM_I    0x75
#define REG_ACCEL_XOUT  0x3B

static bool mpu_write_reg(uint8_t reg, uint8_t val) {
  i2c_lock();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  uint8_t err = Wire.endTransmission(true);

  i2c_unlock();
  return (err == 0);
}

static bool mpu_read_bytes(uint8_t reg, uint8_t *buf, size_t len) {
  i2c_lock();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  uint8_t err = Wire.endTransmission(false);   // repeated start
  if (err != 0) { i2c_unlock(); return false; }

  size_t got = Wire.requestFrom((uint8_t)MPU_ADDR, len, true);// stop
  if (got != len) { i2c_unlock(); return false; }

  for (size_t i = 0; i < len; i++) {
    if (!Wire.available()) { i2c_unlock(); return false; }
    buf[i] = Wire.read();
  }

  i2c_unlock();
  return true;
}

static void mpu_bus_recover() {
  // “reset lógico” do Wire: funciona em muitos casos de erro 263
  i2c_lock();
  Wire.end();
  delay(20);
  Wire.begin(21, 22);
  Wire.setClock(100000);
  Wire.setTimeout(50);
  i2c_unlock();
}

bool mpu_init() {
  // acorda
  if (!mpu_write_reg(REG_PWR_MGMT_1, 0x00)) {
    mpu_bus_recover();
    if (!mpu_write_reg(REG_PWR_MGMT_1, 0x00)) return false;
  }

  delay(50);

  // WHO_AM_I
  uint8_t who = 0x00;
  if (!mpu_read_bytes(REG_WHO_AM_I, &who, 1)) {
    mpu_bus_recover();
    if (!mpu_read_bytes(REG_WHO_AM_I, &who, 1)) return false;
  }

  // MPU6500 costuma retornar 0x70
  Serial.print("[MPU] WHO_AM_I = 0x");
  Serial.println(who, HEX);

  // Se quiser ser mais flexível:
  // 0x70 (MPU6500), 0x71 (MPU9250)
  if (who != 0x70 && who != 0x71) {
    Serial.println("[MPU] WHO_AM_I inesperado. Endereco/pinos/alimentacao?");
    return false;
  }

  // Configurações (opcional)
  // DLPF, gyro range, accel range - se falhar, tenta recuperar
  if (!mpu_write_reg(0x1A, 0x03)) { mpu_bus_recover(); mpu_write_reg(0x1A, 0x03); }
  if (!mpu_write_reg(0x1B, 0x00)) { mpu_bus_recover(); mpu_write_reg(0x1B, 0x00); }
  if (!mpu_write_reg(0x1C, 0x00)) { mpu_bus_recover(); mpu_write_reg(0x1C, 0x00); }

  return true;
}

bool mpu_read_raw(mpu_data_t *out) {
  uint8_t raw[14];

  // até 3 tentativas antes de declarar falha
  for (int attempt = 0; attempt < 3; attempt++) {
    if (mpu_read_bytes(REG_ACCEL_XOUT, raw, 14)) {
      int16_t ax = (raw[0] << 8) | raw[1];
      int16_t ay = (raw[2] << 8) | raw[3];
      int16_t az = (raw[4] << 8) | raw[5];

      int16_t gx = (raw[8] << 8) | raw[9];
      int16_t gy = (raw[10] << 8) | raw[11];
      int16_t gz = (raw[12] << 8) | raw[13];

      // se corromper, tenta de novo (não trava o sistema)
      if (abs(ax) > 32760 || abs(gx) > 32760) {
        vTaskDelay(pdMS_TO_TICKS(2));
        continue;
      }

      out->ax = ax / 16384.0f;
      out->ay = ay / 16384.0f;
      out->az = az / 16384.0f;

      out->gx = gx / 131.0f;
      out->gy = gy / 131.0f;
      out->gz = gz / 131.0f;

      return true;
    }

    // se falhou, tenta recuperar barramento
    mpu_bus_recover();
    vTaskDelay(pdMS_TO_TICKS(5));
  }

  return false;
}
