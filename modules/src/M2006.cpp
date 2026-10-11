#include "M2006.hpp"

M2006::M2006()
{
        controlMode = RELAX_MODE;

        speedSet = 0;
        positionSet = 0;
        currentSet = 0;

        maxCurrent = 0;

        motorFeedback = {};
        canId = 0;
        hcan = nullptr;
        AliveFlag = Pre_AliveFlag = 0;
        gearBox = GearBox_None;
        MotorState = MOTOR_OFFLINE;

    // TODO: 根据 M2006 手册设置减速比、允许电流和速度/位置环 PID 参数。
    // 基类初始化为松开模式、零电流；完成配置前不允许驱动电机。
}

void M2006::setOutput()
{
    // TODO: 分别实现松开、速度和位置模式；位置模式需要串联位置环与速度环。
    // TODO: 将 PID 输出限制在 maxCurrent 范围内后写入 currentSet。
    currentSet = 0;
}

M2006::MotorStateTypedef M2006::AliveCheck()
{
    // TODO: 比较 AliveFlag 与 Pre_AliveFlag，更新在线状态并保存本次计数。
    MotorState = MOTOR_OFFLINE;
    return MotorState;
}
