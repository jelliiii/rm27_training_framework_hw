#include "pid.hpp"
#include <cmath>

namespace
{
float Clamp(float value, float low, float high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}
}

PID::PID(float kp, float ki, float kd, float maxOut, float maxIOut, int mode)
    : mode(mode), kp(kp), ki(ki), kd(kd), maxOut(maxOut), maxIOut(maxIOut)
{
    Clear();
}

void PID::Tuning(float tuning_kp, float tuning_ki, float tuning_kd)
{
    kp = tuning_kp;
    ki = tuning_ki;
    kd = tuning_kd;
}

void PID::UpdateResult()
{
    err[2] = err[1];
    err[1] = err[0];
    err[0] = ref - fdb;


    const bool invalid =
        !std::isfinite(ref) ||
        !std::isfinite(fdb) ||
        !std::isfinite(kp) ||
        !std::isfinite(ki) ||
        !std::isfinite(kd) ||
        !std::isfinite(maxOut) ||
        !std::isfinite(maxIOut) ||
        !std::isfinite(iResult) ||
        !std::isfinite(result) ||
        maxOut < 0.0f ||
        maxIOut < 0.0f ||
        (mode != PID_POSITION && mode != PID_DELTA) ||
        !std::isfinite(err[0]); //先判断各数据是否有效及模式是否有效

    if (invalid)
    {
        Clear();
        return; //若无效则直接返回
    }

    if (mode == PID_POSITION)
    {
        // 位置式：
        // 输出 = 比例 + 累积积分 + 微分
        pResult = kp * err[0];
        dResult = kd * (err[0] - err[1]);

        float nextIntegral = Clamp(
            iResult + ki * err[0],
            -maxIOut,
            maxIOut);

        float candidate =
            pResult + nextIntegral + dResult;

        if (!std::isfinite(candidate))
        {
            Clear();
            return;
        }

        // 饱和时，阻止积分继续把输出推向饱和方向。
        float integralChange = nextIntegral - iResult;

        bool pushingUpper =
            candidate > maxOut && integralChange > 0.0f;

        bool pushingLower =
            candidate < -maxOut && integralChange < 0.0f;

        if (!pushingUpper && !pushingLower)
            iResult = nextIntegral;

        result = Clamp(
            pResult + iResult + dResult,
            -maxOut,
            maxOut);
    }
    else
    {
        // 增量式：
        // 本次输出 = 上次输出 + 本次输出变化量
        pResult = kp * (err[0] - err[1]);

        // 增量式下，iResult 表示本次积分增量。
        // maxIOut 限制单次积分增量，不是累计积分。
        iResult = Clamp(
            ki * err[0],
            -maxIOut,
            maxIOut);

        dResult = kd *
            (err[0] - 2.0f * err[1] + err[2]);

        float candidate =
            result + pResult + iResult + dResult;

        if (!std::isfinite(candidate))
        {
            Clear();
            return;
        }

        result = Clamp(candidate, -maxOut, maxOut);
    }

    // TODO: 按 mode 实现位置式或增量式 PID，并限制积分和最终输出。
    // 完成前始终保持零输出。
}

void PID::Clear()
{
    ref = fdb = 0.0f;
    err[0] = err[1] = err[2] = 0.0f;
    pResult = iResult = dResult = result = 0.0f;
}
