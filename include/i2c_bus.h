#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern SemaphoreHandle_t g_i2cMutex;

static inline void i2c_lock()   { if(g_i2cMutex) xSemaphoreTake(g_i2cMutex, portMAX_DELAY); }
static inline void i2c_unlock() { if(g_i2cMutex) xSemaphoreGive(g_i2cMutex); }
