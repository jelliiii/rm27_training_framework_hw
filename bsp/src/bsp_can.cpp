#include "bsp_can.hpp"
#include <cstring>

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

/**
 * @brief 初始化CAN滤波器配置。
 * 设置CAN硬件的滤波器，用于优化接收数据的处理。
 * 更多信息，请参考原文，链接：https://blog.csdn.net/weixin_54448108/article/details/128570593
 */
HAL_StatusTypeDef CAN_Init(CAN_HandleTypeDef* hcan)
{
    if (hcan != &hcan1 && hcan != &hcan2) return HAL_ERROR;

    CAN_FilterTypeDef can_filter_st;                   ///< 定义过滤器结构体
    can_filter_st.FilterActivation = ENABLE;           ///< ENABLE使能过滤器
    can_filter_st.SlaveStartFilterBank = 14;                           ///< 双CAN模式下规定CAN的主从模式的过滤器分配，从过滤器为14
    can_filter_st.FilterBank = (hcan == &hcan1) ? 0 : 14;                                     ///< 过滤器组-双CAN可指定0~27


    can_filter_st.FilterMode = CAN_FILTERMODE_IDMASK;  ///< 设置过滤器模式--标识符屏蔽位模式
    can_filter_st.FilterScale = CAN_FILTERSCALE_32BIT; ///< 过滤器的位宽 32 位
    
    can_filter_st.FilterIdHigh = 0x0000;               ///< ID高位
    can_filter_st.FilterIdLow = 0x0000;                ///< ID低位
    can_filter_st.FilterMaskIdHigh = 0x0000;           ///< 过滤器掩码高位
    can_filter_st.FilterMaskIdLow = 0x0000;            ///< 过滤器掩码低位

    can_filter_st.FilterFIFOAssignment = CAN_RX_FIFO0;                 ///< 与过滤器组管理的 FIFO
    
    HAL_StatusTypeDef status = HAL_CAN_ConfigFilter(hcan, &can_filter_st);
    if (status != HAL_OK) return status;
    status = HAL_CAN_Start(hcan);
    if (status != HAL_OK) return status;
    return HAL_CAN_ActivateNotification(hcan, CAN_IT_RX_FIFO0_MSG_PENDING);

    
}

HAL_StatusTypeDef CAN_Transmit(CAN_HandleTypeDef *hcan, uint32_t Id,  uint8_t *msg, uint16_t len)
{
    if ((hcan != &hcan1 && hcan != &hcan2) || Id > 0x7FFU ||
    len > 8U || (len != 0U && msg == nullptr))
    return HAL_ERROR;

    if (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0)
    {
        // 如果邮箱已满，可以选择等待或者采取其他操作
        // 例如，可以添加一个超时机制或者直接返回错误状态
        // Monitor::instance()->Log_Messages(Monitor::WARNING, (uint8_t *)"CAN Tx Mailbox is full\r\n");
        return HAL_BUSY;
    }
    if (HAL_CAN_GetState(hcan) != HAL_CAN_STATE_LISTENING)
        return HAL_ERROR;

    // TODO: 校验标准帧 ID 和 DLC (0~8)，构造帧头并调用 HAL_CAN_AddTxMessage。
    CAN_TxHeaderTypeDef tx_header = {};
    tx_header.StdId = Id;
    tx_header.IDE = CAN_ID_STD;
    tx_header.RTR = CAN_RTR_DATA;
    tx_header.DLC = len;
    tx_header.TransmitGlobalTime = DISABLE;

    // 当前 HAL 写邮箱时读取全部 8 字节，短帧也提供完整缓冲区。
    uint8_t payload[8] = {};
    if (len != 0U) std::memcpy(payload, msg, len);
    uint32_t mailbox = 0;
    return HAL_CAN_AddTxMessage(hcan, &tx_header, payload, &mailbox);
}

// TODO: 实现 CAN 接收回调，根据总线和标准帧 ID 分发电机反馈。

extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan){
    CAN_RxHeaderTypeDef rx_header={};
    uint8_t rx_data[8];
    HAL_CAN_GetRxFifoFillLevel(hcan,CAN_RX_FIFO0);
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) != HAL_OK)
    Error_Handler();

}

// bsp_can.cpp
#ifdef CAN_DEBUG

volatile int32_t can_debug_result = 0;
static uint8_t debug_data[8] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
};


HAL_StatusTypeDef CAN_DebugSend(void)
{

    return CAN_Transmit(&hcan1, 0x123, debug_data, sizeof(debug_data));
}

#endif