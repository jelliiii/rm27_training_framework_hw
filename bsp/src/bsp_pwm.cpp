//
// Created by cosmosmount on 2025/9/2.
//

#include "bsp_pwm.hpp"
#include <cmath>

namespace
{
    bool IsSupportedTimer(TIM_HandleTypeDef* htim)
    {
        return htim != nullptr &&
            (htim->Instance == TIM1 ||
                htim->Instance == TIM10);
    }

    bool IsSupportedChannel(TIM_HandleTypeDef* htim,
                            uint32_t channel)
    {
        return IsSupportedTimer(htim) &&
            channel == TIM_CHANNEL_1;
    }

    uint32_t GetTimerClockHz()
    {
        // 当前实现针对 STM32F407 的 TIM1/TIM10。
        // 两者位于 APB2。
        RCC_ClkInitTypeDef clockConfig = {};
        uint32_t flashLatency = 0;

        HAL_RCC_GetClockConfig(&clockConfig, &flashLatency);

        uint32_t clock = HAL_RCC_GetPCLK2Freq();

        if (clockConfig.APB2CLKDivider != RCC_HCLK_DIV1)
            clock *= 2U;

        return clock;
    }
}

void PWM_Init(void)
{
    // 必须先执行 CubeMX 生成的 MX_TIM10_Init()。
    // 此入口只启动加热 PWM，不自动启动 TIM1 舵机。
    PWM_SetDutyRatio(&htim10, 0.0f, TIM_CHANNEL_1);
    PWM_Start(&htim10, TIM_CHANNEL_1);
    // TODO: 根据实际使用的定时器和通道完成 PWM 初始化。
}

void PWM_Start(TIM_HandleTypeDef *htim, uint32_t Channel)
{
    if (!IsSupportedChannel(htim, Channel))
    {
        Error_Handler();
        return;
    }

    if (HAL_TIM_PWM_Start(htim, Channel) != HAL_OK)
    {
        Error_Handler();
        return;
    }
    // TODO: 调用 HAL 启动指定定时器通道的 PWM，并检查返回状态。
}

void PWM_Stop(TIM_HandleTypeDef *htim, uint32_t Channel)
{
    if (!IsSupportedChannel(htim, Channel))
    {
        Error_Handler();
        return;
    }

    __HAL_TIM_SET_COMPARE(htim, Channel, 0U);

    if (HAL_TIM_PWM_Stop(htim, Channel) != HAL_OK)
    {
        Error_Handler();
        return;
    }
    // TODO: 调用 HAL 停止指定定时器通道的 PWM。
}

void PWM_SetPeriod(TIM_HandleTypeDef *htim, float period_s)
{
    if (!IsSupportedTimer(htim) ||
        !std::isfinite(period_s) ||
        period_s <= 0.0f)
    {
        Error_Handler();
        return;
    }

    // TIM1 可能有多个通道共用周期。
    // 本接口要求整个定时器已停止。
    if ((htim->Instance->CR1 & TIM_CR1_CEN) != 0U)
    {
        Error_Handler();
        return;
    }

    // 只支持向上计数、边沿对齐。
    if ((htim->Instance->CR1 &
         (TIM_CR1_DIR | TIM_CR1_CMS)) != 0U)
    {
        Error_Handler();
        return;
    }

    const double timerClock =
        static_cast<double>(GetTimerClockHz());

    const double divider =
        static_cast<double>(htim->Instance->PSC) + 1.0;

    const double requestedCounts =
        timerClock * static_cast<double>(period_s) / divider;

    // TIM1/TIM10 为 16 位。
    // 限制 ARR <= 65534，使 ARR+1 也可写入 CCR，
    // 从而支持 PWM1 模式下真正的 100% 占空比。
    if (!std::isfinite(requestedCounts) ||
        requestedCounts < 1.0 ||
        requestedCounts > 65535.0)
    {
        Error_Handler();
        return;
    }

    const uint32_t counts =
        static_cast<uint32_t>(requestedCounts + 0.5);

    const uint32_t arr = counts - 1U;

    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, 0U);
    __HAL_TIM_SET_AUTORELOAD(htim, arr);
    htim->Init.Period = arr;

    // 将预分频、ARR 和比较值装载到有效寄存器。
    __HAL_TIM_GENERATE_EVENT(htim, TIM_EVENTSOURCE_UPDATE);
    __HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_UPDATE);
    __HAL_TIM_SET_COUNTER(htim, 0U);

    // TODO: 根据定时器时钟和预分频值，将秒转换为 ARR 并更新周期。
}

void PWM_SetDutyRatio(TIM_HandleTypeDef *htim, float duty, uint32_t channel)
{
    if (!IsSupportedChannel(htim, channel))
    {
        Error_Handler();
        return;
    }

    // 无效输入按关闭处理。
    if (!std::isfinite(duty))
        duty = 0.0f;

    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;

    uint32_t periodCounts =
        __HAL_TIM_GET_AUTORELOAD(htim) + 1U;

    uint32_t compare =
        static_cast<uint32_t>(duty * periodCounts + 0.5f);

    __HAL_TIM_SET_COMPARE(htim, channel, compare);
    // TODO: 将 [0, 1] 占空比换算为比较值，写入指定通道。
}
