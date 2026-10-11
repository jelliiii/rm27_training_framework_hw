#include "main.h"
#include "tx_api.h"
#include "BMI088.hpp"
#include "IMU.hpp"

TX_THREAD imu_thread;
uint8_t imu_thread_stack[1024]={0};

uint32_t imu_stage = 0;
uint32_t imu_error_count = 0;
uint32_t imu_read_count = 0;


[[noreturn]] void imu_thread_entry(ULONG thread_input){
    UNUSED(thread_input);

    imu_stage = 1;//第一阶段——开始初始化
    BMI088::cBMI088 imu_sensor;

    imu_sensor.Config();

    if(imu_sensor.self_test.INIT_ERR){
        imu_stage = 2; //初始化失败——第二阶段
        while(1){
            tx_thread_sleep(10);
        }
    }

    imu_stage = 3;


    while (1) {
        imu_sensor.VerifyAccChipID();
        imu_sensor.VerifyGyroChipID();

        imu_sensor.ReadAccData(&imu_sensor.acc_data);

        imu_sensor.ReadGyroData(&imu_sensor.gyro_data);
       

        imu_read_count++;
        
        if(imu_sensor.self_test.ACC_CHIP_ID_ERR || imu_sensor.self_test.GYRO_CHIP_ID_ERR){
            imu_error_count++;
        }

        tx_thread_sleep(1);
    }
}