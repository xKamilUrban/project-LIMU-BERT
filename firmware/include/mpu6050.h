#pragma once

#include <stdint.h>

#define I2C_MASTER_SCL          GPIO_NUM_22
#define I2C_MASTER_SDA          GPIO_NUM_21
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_FREQ_HZ      400000
#define I2C_MASTER_TIMEOUT_MS   1000

#define MPU6050_ADDR            0x68
#define MPU6050_WHO_AM_I_ADDR   0x75
#define MPU6050_PWR_MGMT_1      0x6B
#define MPU6050_ACCEL_X_ADDR    0x3B

#define MPU6050_GYRO_CONFIG     0x1B
#define MPU6050_ACCEL_CONFIG    0x1C
#define MPU6050_DLPF_CONFIG     0x1A
#define MPU6050_SMPRT_DIV       0x19

void calibrate_mpu6050(int16_t offset_acc[], int16_t offset_gyro[]);
void mpu_data_task(void *pvParameters);
void mpu6050_init(void);