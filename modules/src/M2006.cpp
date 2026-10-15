#include "M2006.hpp"
#include <cmath>
#include <cstdint>

namespace{
    constexpr float PI = 3.14159265358979323846f;
    //C610协议规定最大电流 10A，对应 10000
    constexpr uint16_t PROTOCOL_CURRENT_LIMIT = 10000;

    //测试最大电流 1A，1000
    constexpr uint16_t INITIAL_CURRENT_LIMIT = 1000;   
    
    //输出轴速度限幅，单位rad/s
    //仅测试
    constexpr float OUTPUT_SPEED_LIMIT = 10.0f;


    float Clamp(float value, float low, float high)
    {
        if (value < low) return low;
        if (value > high) return high;
        return value;
    }

    void RelaxMotor(M2006 & motor)
    {
        motor.currentSet = 0;
        motor.positionPid.Clear();
        motor.speedPid.Clear();
        motor.controlMode = DJIMotor::RELAX_MODE;
       
    }

}
M2006::M2006()
{
    gearBox = GearBox_M2006;
    maxCurrent = INITIAL_CURRENT_LIMIT;

    //速度环 PID
    speedPid = PID(0.0f, 0.0f, 0.0f,  static_cast<float>(maxCurrent), 0.0f, PID_POSITION);
    speedPid.Clear();

    //位置环 PID

    positionPid = PID(0.0f, 0.0f, 0.0f, static_cast<float>(maxCurrent), 0.0f,  PID_POSITION);
    positionPid.Clear();
    // TODO: 根据 M2006 手册设置减速比、允许电流和速度/位置环 PID 参数。
    // 基类初始化为松开模式、零电流；完成配置前不允许驱动电机。
}

// maxCurrent设置
// 根据电机反馈实现新的currentSet设置

void M2006::setOutput()
{
    // 默认本次无输出，只有全部检查通过才赋新值。
    currentSet = 0;

    //模式检查
    if (controlMode == RELAX_MODE)
    {
        RelaxMotor(*this);
        return;
    }

    // 未注册、离线或未设置允许电流时，不进行控制。
    if (hcan == nullptr ||
        canId < 0x201U || canId > 0x208U ||
        MotorState != MOTOR_ONLINE ||
        maxCurrent == 0U)
    {
        RelaxMotor(*this);
        return;
    }

    //检验速度反馈是否有效
    if (!std::isfinite(motorFeedback.speedFdb))
    {
        RelaxMotor(*this);
        return;
    }

    // 软件配置再大，也不能超过 C610 协议上限。
    const float currentLimit = static_cast<float>(
        maxCurrent > PROTOCOL_CURRENT_LIMIT
            ? PROTOCOL_CURRENT_LIMIT
            : maxCurrent);

    // 让 PID 内部输出限幅与最终电流限幅一致。
    speedPid.maxOut = currentLimit;

    //检验速度环输出限幅设置是否有效（积分项输出+最终输出）
    if (!std::isfinite(speedPid.maxIOut) ||
        speedPid.maxIOut < 0.0f)
    {
        RelaxMotor(*this);
        return;
    }

    speedPid.maxIOut =
        Clamp(speedPid.maxIOut, 0.0f, currentLimit);

    //开始设置目标速度，先清零
    float targetSpeed = 0.0f;

    switch (controlMode)
    {
    case SPD_MODE:
        if (!std::isfinite(speedSet))
        {
            RelaxMotor(*this);
            return;
        }

        // 速度模式不使用位置环。
        positionPid.Clear();

        targetSpeed = Clamp(
            speedSet,
            -OUTPUT_SPEED_LIMIT,
             OUTPUT_SPEED_LIMIT);
        break;

    case POS_MODE:
        if (!std::isfinite(positionSet) ||
            !std::isfinite(motorFeedback.positionFdb))
        {
            RelaxMotor(*this);
            return;
        }

        // 沿用父类声明的目标位置范围 [-π, π]。
        // positionFdb 应为跨圈累计后的连续位置。
        positionPid.ref = Clamp(positionSet, -PI, PI);
        positionPid.fdb = motorFeedback.positionFdb;

        positionPid.maxOut = OUTPUT_SPEED_LIMIT;

        if (!std::isfinite(positionPid.maxIOut) ||
            positionPid.maxIOut < 0.0f)
        {
            RelaxMotor(*this);
            return;
        }

        positionPid.maxIOut = Clamp(
            positionPid.maxIOut, 0.0f, OUTPUT_SPEED_LIMIT);

        positionPid.UpdateResult();

        if (!std::isfinite(positionPid.result))
        {
            RelaxMotor(*this);
            return;
        }

        // 位置环算出的结果，是速度环的目标。
        targetSpeed = Clamp(
            positionPid.result,
            -OUTPUT_SPEED_LIMIT,
             OUTPUT_SPEED_LIMIT);
        break;

    default:
        RelaxMotor(*this);
        return;
    }

    // 速度环：目标速度与实际速度 -> 电流指令。
    speedPid.ref = targetSpeed;
    speedPid.fdb = motorFeedback.speedFdb;
    speedPid.UpdateResult();

    if (!std::isfinite(speedPid.result))
    {
        RelaxMotor(*this);
        return;
    }

    const float command = Clamp(
        speedPid.result,
        -currentLimit,
         currentLimit);

    currentSet = static_cast<int16_t>(command);
    // TODO: 分别实现松开、速度和位置模式；位置模式需要串联位置环与速度环。
    // TODO: 将 PID 输出限制在 maxCurrent 范围内后写入 currentSet。
}

M2006::MotorStateTypedef M2006::AliveCheck()
{
    // 接收并成功解析一帧本电机反馈后，更新 AliveFlag。
    const uint32_t currentAlive = AliveFlag;

    MotorState = (currentAlive != Pre_AliveFlag)
        ? MOTOR_ONLINE
        : MOTOR_OFFLINE;

    Pre_AliveFlag = currentAlive;

    if (MotorState == MOTOR_OFFLINE)
    {
        // 断联后撤销控制模式。
        // 恢复反馈后，需要上层重新选择速度/位置模式。
        RelaxMotor(*this);
    }

    return MotorState;
    // TODO: 比较 AliveFlag 与 Pre_AliveFlag，更新在线状态并保存本次计数。
}

// |             变量             |         单位或含义            |
// |.            ---              |.           ---              |
// |   `speedSet`、`speedFdb`     |    减速后输出轴角速度，rad/s    |
// | `positionSet`、`positionFdb` |    输出轴相对参考点的位置，rad  |
// | `currentSet`、`maxCurrent`.  |.  C610 指令整数，1000 对应 1 A |
// |           `gearBox`          | 减速箱类型；实际除以 36 在反馈解析中完成 |
