#include "main.h"
#include "tx_api.h"
#include "M2006.hpp"
#include "DJIMotorHandler.hpp"
#include "bsp_can.hpp"



TX_THREAD motor_thread;
uint8_t motor_thread_stack[1024]={0};

void motor_thread_entry(ULONG thread_input){
    M2006 motor;
    auto *handler = DJIMotorHandler::Instance();    // 以 CAN1、反馈 ID 0x201 为例，按实际接线和电调 ID 修改。
    handler->registerMotor(&motor, &hcan1, 0x201);
    motor.speedPid.Tuning(50,0,0);
    motor.speedSet = 2.0f;

    if (CAN_Init(&hcan1) != HAL_OK)
    {
        Error_Handler();
    }

    UNUSED(thread_input);
    while(1)
    {
        motor.setOutput();
        handler->sendControlData();
        tx_thread_sleep(1);
    }

}
