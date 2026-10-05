/**
 * @brief 初始化CAN滤波器配置。
 * 设置CAN硬件的滤波器，用于优化接收数据的处理。
 * 更多信息，请参考原文，链接：https://blog.csdn.net/weixin_54448108/article/details/128570593
 */

/**
 * @brief 发送CAN数据。
 * @param hcan 指向CAN句柄的指针，用于配置CAN传输。
 * @param StdId CAN消息的标准标识符
 * @param msg 发送的数据
 * @param len 数据长度
 * @note 该函数用于发送CAN数据，目前只支持标准帧
 */

#ifndef BSP_CAN_H
#define BSP_CAN_H

#include "can.h"
#include "main.h"


#ifdef __cplusplus
extern "C" {
#endif

HAL_StatusTypeDef CAN_Init(CAN_HandleTypeDef* hcan);
HAL_StatusTypeDef CAN_Transmit(CAN_HandleTypeDef *hcan, uint32_t Id, uint8_t *msg, uint16_t len);

#ifdef CAN_DEBUG
// 单次回环测试：0 未收到结果，1 匹配，-1 收到的测试帧不匹配。
extern volatile int32_t can_debug_result;
HAL_StatusTypeDef CAN_DebugSend(void);
#endif
#ifdef __cplusplus
}
#endif
#endif