#ifndef MPU6500_H
#define MPU6500_H

typedef struct {
    float ax, ay, az;
    float gx, gy, gz;
} mpu_data_t;

bool mpu_init();

bool mpu_read_raw(mpu_data_t *data);


#endif
