#include "main.h"
#include "tx_api.h"
#include "BMI088.hpp"
#include "IMU.hpp"

TX_THREAD imu_thread;
uint8_t imu_thread_stack[1024]={0};
BMI088::cBMI088 imu_sensor;



[[noreturn]] void imu_thread_entry(ULONG thread_input){

    UNUSED(thread_input);

    imu_sensor.Config();


    while (1) {


        tx_thread_sleep(1);
    }
}