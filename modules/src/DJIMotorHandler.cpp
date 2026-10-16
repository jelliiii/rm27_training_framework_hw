#include "DJIMotorHandler.hpp"
#include "bsp_can.hpp"
#include <cstring>
#include <limits>

namespace
{
    constexpr float TWO_PI = 6.28318530718f;
    constexpr float M2006_RATIO = 36.0f;
    constexpr int32_t ENCODER_RANGE = 8192;
    constexpr int32_t HALF_ENCODER_RANGE = 4096;

    int GetBusIndex(CAN_HandleTypeDef * hcan)
    {
        if(hcan == &hcan1) return 0;
        if(hcan == &hcan2) return 1;
        return -1;
    }

uint16_t ReadU16BE(const uint8_t *data)
{
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(data[0]) << 8) | data[1]);
}

int16_t ReadI16BE(const uint8_t *data)
{
    const uint16_t value = ReadU16BE(data);

    // 显式处理补码负数。
    const int32_t signedValue =
        (value & 0x8000U) ? static_cast<int32_t>(value) - 65536
                          : static_cast<int32_t>(value);

    return static_cast<int16_t>(signedValue);
}

}
void DJIMotorHandler::registerMotor(DJIMotor *motor, CAN_HandleTypeDef *hcan, uint16_t canId)
{

    //检验句柄
    const int bus = GetBusIndex(hcan);

    if(bus < 0 || motor == nullptr 
        || canId < 0x201U || canId > 0x208U)
    {
        return;
    }

     // 当前实现只处理本工程中的 M2006 + C610。
    if (motor->gearBox != GearBox_M2006)
    {
        return;
    }

    //根据ID放置
    const int index = static_cast<int>(canId - 0x201U);

    //同一位重复注册电机，无需初始化
    if(DJIMotorList[bus][index] == motor)
    {
        return;
    }

    //不覆盖已经注册的电机
    if(DJIMotorList[bus][index] != nullptr)
    {
        return;
    }

    //同一对象不能重复登记到不同位置
    for (int b = 0; b < 2; ++b)
    {
        for (int i = 0; i < 8; ++i)
        {
            if (DJIMotorList[b][i] == motor)
            {
                return;
            }
        }
    }

    motor->hcan = hcan;
    motor->canId = canId;
    motor->currentSet = 0;
    motor->controlMode = DJIMotor::RELAX_MODE;
    motor->MotorState = DJIMotor::MOTOR_OFFLINE;

    motor->motorFeedback = {};
    // -1 表示还没有接收过编码器数据。
    motor->motorFeedback.last_ecd = -1;
    motor->motorFeedback.temperatureFdb =
        std::numeric_limits<float>::quiet_NaN();

    motor->AliveFlag = 0;
    motor->Pre_AliveFlag = 0;

    DJIMotorList[bus][index] = motor;

    //放置报文存在标志
    if(bus == 0)
    {
        if (index < 4) CAN1_0x200_Exist = true;
        else CAN1_0x1FF_Exist = true;
    }
    else{
        if (index < 4) CAN2_0x200_Exist = true;
        else CAN2_0x1FF_Exist = true;
    }
    
    // TODO: 校验 CAN 句柄和 0x201~0x208 ID，再登记到对应总线与槽位。
    // TODO: 按 ID 设置对应的 0x200/0x1FF 报文存在标志。
}

void DJIMotorHandler::sendControlData()

{
    uint8_t *frames[2][2] = {
        {can1_send_data_0, can1_send_data_1},
        {can2_send_data_0, can2_send_data_1}
    };

    CAN_HandleTypeDef *buses[2] = {&hcan1, &hcan2};

    const bool enabled[2][2] = {
        {CAN1_0x200_Exist, CAN1_0x1FF_Exist},
        {CAN2_0x200_Exist, CAN2_0x1FF_Exist}
    };

    for (int bus = 0; bus < 2; ++bus)
    {
        for (int group = 0; group < 2; ++group)
        {
            uint8_t *data = frames[bus][group];
            std::memset(data, 0, 8);

            for (int slot = 0; slot < 4; ++slot)
            {
                const int index = group * 4 + slot;
                DJIMotor *motor = DJIMotorList[bus][index];

                if (motor == nullptr)
                {
                    continue;
                }

                int32_t command = 0;

                if (motor->MotorState == DJIMotor::MOTOR_ONLINE &&
                    (motor->controlMode == DJIMotor::SPD_MODE ||
                     motor->controlMode == DJIMotor::POS_MODE))
                {
                    // C610 指令限制，同时遵守电机的软件限幅。
                    const int32_t limit =
                        motor->maxCurrent < 10000U
                            ? motor->maxCurrent
                            : 10000;

                    command = motor->currentSet;

                    if (command > limit)  command = limit;
                    if (command < -limit) command = -limit;
                }

                // 转成无符号数，保留负电流的补码位模式。
                const uint16_t raw = static_cast<uint16_t>(command);

                data[slot * 2] =
                    static_cast<uint8_t>(raw >> 8);
                data[slot * 2 + 1] =
                    static_cast<uint8_t>(raw & 0xFFU);
            }

            if (enabled[bus][group])
            {
                const uint32_t txId =
                    (group == 0) ? 0x200U : 0x1FFU;

                // 保留现有 void 接口：
                // 本次失败不阻塞，等待下一控制周期发送最新指令。
                const HAL_StatusTypeDef status =
                    CAN_Transmit(buses[bus], txId, data, 8);

                if (status != HAL_OK)
                {
                    continue;
                }
            }
        }
    }


    // TODO: 将各电机 currentSet 按大端序放入正确的 8 字节控制帧。
    // TODO: 仅发送包含已注册电机的报文；未完成前不发送 CAN 电流命令。
}

void DJIMotorHandler::updateFeedback(CAN_HandleTypeDef *hcan, uint8_t *rx_data, int index)
{
    const int bus = GetBusIndex(hcan);

    if (bus < 0 || rx_data == nullptr ||
        index < 0 || index >= 8)
    {
        return;
    }

    DJIMotor *motor = DJIMotorList[bus][index];

    if (motor != nullptr)
    {
        UpdateSensorData(motor, rx_data);
    }
    // TODO: 校验总线、反馈数据和索引，再找到对应电机并调用 UpdateSensorData。
    (void)hcan;
    (void)rx_data;
    (void)index;
}

void DJIMotorHandler::UpdateSensorData(DJIMotor *motor, uint8_t *can_data)
{
    if (motor == nullptr || can_data == nullptr ||
        motor->gearBox != GearBox_M2006)
    {
        return;
    }

    const uint16_t encoder = ReadU16BE(&can_data[0]);//机械角度

    if (encoder >= ENCODER_RANGE)
    {
        return;
    }

    auto &feedback = motor->motorFeedback;

    feedback.lastSpeedFdb = feedback.speedFdb;
    feedback.lastPositionFdb = feedback.positionFdb;

    if (feedback.last_ecd < 0)
    {
        // 第一帧只建立参考点，不将上电角度计入位移。
        feedback.positionFdb = 0.0f;
    }
    else
    {
        int32_t delta =
            static_cast<int32_t>(encoder) - feedback.ecd;

        // 处理编码器经过 8191 / 0 边界的情况。
        if (delta > HALF_ENCODER_RANGE)
        {
            delta -= ENCODER_RANGE;
        }
        else if (delta < -HALF_ENCODER_RANGE)
        {
            delta += ENCODER_RANGE;
        }

        feedback.positionFdb +=
            static_cast<float>(delta) * TWO_PI /
            (static_cast<float>(ENCODER_RANGE) * M2006_RATIO);
    }

    feedback.last_ecd =
        (feedback.last_ecd < 0)
            ? static_cast<int16_t>(encoder)
            : static_cast<int16_t>(feedback.ecd);

    feedback.ecd = encoder;
    feedback.speed_rpm = ReadI16BE(&can_data[2]);
    feedback.currentFdb =
        static_cast<float>(ReadI16BE(&can_data[4]));

    // 转子 rpm -> 输出轴 rad/s。
    feedback.speedFdb =
        static_cast<float>(feedback.speed_rpm) *
        TWO_PI / (60.0f * M2006_RATIO);

    // C610 不提供这里所需的温度反馈。
    feedback.temperatureFdb =
        std::numeric_limits<float>::quiet_NaN();

    ++motor->AliveFlag;
    // TODO: 从 CAN 数据解析编码器、转速、电流和温度。
    // TODO: 处理编码器回绕，并按减速比换算输出轴位置和角速度。
    (void)motor;
    (void)can_data;
}

void DJIMotorHandler::AllMotorAliveCheck()
{
    for (int bus = 0; bus < 2; ++bus)
    {
        for (int index = 0; index < 8; ++index)
        {
            DJIMotor *motor = DJIMotorList[bus][index];

            if (motor != nullptr)
            {
                motor->AliveCheck();
            }
        }
    }

    // TODO: 遍历已注册电机并调用 AliveCheck。
}
