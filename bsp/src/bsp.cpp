#include "bsp.hpp"
#include "main.h"
#include "bsp_can.hpp"
#include "bsp_usart.hpp"

void bsp_Init() {
    USART_Init();
    if(CAN_Init() != HAL_OK)
    {
        Error_Handler();
    }
}
