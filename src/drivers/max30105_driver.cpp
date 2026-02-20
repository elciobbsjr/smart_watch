#include "max30105_driver.h"
#include "heartRate.h"

bool MAX30105Driver::begin(TwoWire &wirePort)
{
    if (!sensor.begin(wirePort, I2C_SPEED_STANDARD))
        return false;

    sensor.setup(
        40,   // LED brightness
        4,    // average
        2,    // mode (Red + IR)
        200,  // sample rate
        411,  // pulse width
        16384 // ADC range
    );

    return true;
}

bool MAX30105Driver::fingerDetected()
{
    return irAvg > fingerThreshold;
}

float MAX30105Driver::getBPM()
{
    return smoothedBPM;
}

void MAX30105Driver::update()
{
    long irValue = sensor.getIR();

    // Média exponencial do IR
    irAvg = (irAvg * 0.9) + (irValue * 0.1);

    if (!fingerDetected()) {
        smoothedBPM = 0;
        return;
    }

    if (checkForBeat(irValue)) {
        processBeat(irValue);
    }
}

void MAX30105Driver::processBeat(long irValue)
{
    long now = millis();
    long delta = now - lastBeat;
    lastBeat = now;

    if (delta < 300 || delta > 2000)
        return;

    float rawBPM = 60.0 / (delta / 1000.0);

    if (rawBPM < 45 || rawBPM > 150)
        return;

    rates[rateSpot++] = rawBPM;
    rateSpot %= RATE_SIZE;

    float avgBPM = 0;
    for (byte i = 0; i < RATE_SIZE; i++)
        avgBPM += rates[i];

    avgBPM /= RATE_SIZE;

    if (smoothedBPM == 0) {
        smoothedBPM = avgBPM;
        return;
    }

    float variation = abs(avgBPM - smoothedBPM);
    float alpha;

    if (variation < 5)
        alpha = alphaFast;
    else if (variation < 15)
        alpha = alphaSlow;
    else
        return;

    smoothedBPM = alpha * avgBPM + (1 - alpha) * smoothedBPM;
}