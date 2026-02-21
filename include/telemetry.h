#pragma once

typedef struct {
    float pitch;
    float roll;
    float accMag;
    float gyroMag;
    float jerk;
    int   state_beta1;
    float bpm;
} telemetry_data_t;

extern telemetry_data_t g_telemetry;