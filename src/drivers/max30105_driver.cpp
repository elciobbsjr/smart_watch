#include "max30105_driver.h"

bool MAX30105Driver::begin(TwoWire &wirePort)
{
    if (!sensor.begin(wirePort, I2C_SPEED_STANDARD))
        return false;

    sensor.setup(
        40,   // LED brightness
        4,    // sample average
        2,    // mode (Red + IR)
        200,  // sample rate
        411,  // pulse width
        16384 // ADC range
    );

    sensor.clearFIFO();

    return true;
}

bool MAX30105Driver::readSample(ppg_sample_t &sample)
{
    // Atualiza o sensor e FIFO
    sensor.check();

    // Enquanto houver dados no FIFO
    if (sensor.available())
    {
        sample.ir  = sensor.getIR();
        sample.red = sensor.getRed();
        sample.timestamp = millis();

        sensor.nextSample();
        return true;
    }

    return false;
}
bool MAX30105Driver::fingerDetected(uint32_t irValue)
{
    return irValue > fingerThreshold;
}