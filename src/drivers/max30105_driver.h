#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"

class MAX30105Driver {
public:
    bool begin(TwoWire &wirePort = Wire);
    void update();
    float getBPM();
    bool fingerDetected();

private:
    MAX30105 sensor;

    static const int RATE_SIZE = 4;
    float rates[RATE_SIZE];
    byte rateSpot = 0;

    long lastBeat = 0;
    float smoothedBPM = 0;

    long irAvg = 0;

    const float alphaFast = 0.4;
    const float alphaSlow = 0.15;

    const long fingerThreshold = 5000;

    void processBeat(long irValue);
};