#include "main.h"
#include "tx_api.h"
#include "BMI088.hpp"
#include "IMU.hpp"

TX_THREAD imu_thread;
uint8_t imu_thread_stack[1024]={0};

uint32_t imu_error_count = 0;
uint32_t imu_read_count = 0;


void imu_thread_entry(ULONG thread_input){
    UNUSED(thread_input);

    BMI088::cBMI088 imu_sensor;

    imu_sensor.Config();

    // //验证 accID ，各种配置是否正确
    // static uint8_t acc_check[5] = {};
    // static bool acc_read_ok[5] = {};

    // const uint8_t addresses[5] = {0x00, 0x7D, 0x7C, 0x40, 0x41};

    // for(int i = 0; i < 5; i++){
    //     acc_read_ok[i] = imu_sensor.ReadReg
    //     (BMI088::BMI088_CS_ACC, addresses[i], &acc_check[i], 1);
    // }

    if(imu_sensor.self_test.INIT_ERR){
        while(1){
            tx_thread_sleep(10);
        }
    }


    while(1)
    {


        // imu_sensor.VerifyAccChipID();
        // imu_sensor.VerifyGyroChipID();

        imu_sensor.ReadAccData(&imu_sensor.acc_data);

        imu_sensor.ReadGyroData(&imu_sensor.gyro_data);
       

        imu_read_count++;
        
        if(imu_sensor.self_test.ACC_CHIP_ID_ERR || imu_sensor.self_test.GYRO_CHIP_ID_ERR){
            imu_error_count++;
        }
        tx_thread_sleep(1);
    }
}