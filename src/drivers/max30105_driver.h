#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <MAX30105.h>

typedef struct {
    uint32_t ir;
    uint32_t red;
    uint32_t timestamp;
} ppg_sample_t;

class MAX30105Driver
{
public:
    bool begin(TwoWire &wirePort);

    // Lê uma amostra do FIFO
    bool readSample(ppg_sample_t &sample);

    // Verifica presença de dedo (IR bruto)
    bool fingerDetected(uint32_t irValue);

private:
    MAX30105 sensor;

    const uint32_t fingerThreshold = 50000; // Ajustável
};