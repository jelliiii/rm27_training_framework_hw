#include "main.h"
#include "tx_api.h"
#include "M2006.hpp"
#include "DJIMotorHandler.hpp"
#include "bsp_can.hpp"



TX_THREAD motor_thread;
uint8_t motor_thread_stack[1024]={0};



void motor_thread_entry(ULONG thread_input){
    UNUSED(thread_input);
    static M2006 motor;
    auto *handler = DJIMotorHandler::Instance();    // 以 CAN1、反馈 ID 0x201 为例，按实际接线和电调 ID 修改。
    handler->registerMotor(&motor, &hcan1, 0x201);
    motor.speedPid.Tuning(50,0,0);
    motor.speedSet = 10.0f;
    motor.controlMode = DJIMotor::SPD_MODE;

    if(CAN_Init(&hcan1) != HAL_OK)
    {
        Error_Handler();
    };


    while(1)
    {
        tx_thread_sleep(1);

        motor.AliveCheck();

        motor.setOutput();
        handler->sendControlData();
     
    }

}

// bool startedOnce = false;

//     while (1)
//     {
//         // 先给接收中断时间更新反馈。
//         tx_thread_sleep(1); // 当前为一个 10 ms tick

//         // 单电机实验：短临界区，防止反馈在计算中途被修改。
//         const uint32_t savedPrimask = __get_PRIMASK();
//         __disable_irq();

//         motor.AliveCheck();

//         // 首次确认在线后，启动一次。
//         if (!startedOnce &&
//             motor.MotorState == DJIMotor::MOTOR_ONLINE)
//         {
//             motor.controlMode = DJIMotor::SPD_MODE;
//             startedOnce = true;
//         }

//         motor.setOutput();

//         __set_PRIMASK(savedPrimask);

//         // CAN 发送放在临界区外。
//         handler->sendControlData();
//     }