# 一.常见函数
## 1. 时钟树 (RCC)

CubeMX 会自动生成 SystemClock_Config()，但内部通常调用以下函数：

HAL_RCC_OscConfig()：配置 HSE、HSI、PLL 等振荡器。
HAL_RCC_ClockConfig()：配置 AHB、APB 总线分频系数和系统主频。
HAL_RCC_GetHCLKFreq() / HAL_RCC_GetPCLK1Freq()：获取当前总线频率（常用于计算波特率或定时器周期）。
HAL_Delay()：基于 SysTick 的毫秒级阻塞延时。
## 2. GPIO (三色灯)

HAL_GPIO_Init()：初始化引脚（CubeMX 生成在 MX_GPIO_Init 中）。
HAL_GPIO_WritePin()：设置引脚高低电平（如亮灭灯）。
HAL_GPIO_TogglePin()：翻转引脚电平（常用于闪烁 LED）。
HAL_GPIO_ReadPin()：读取引脚电平（如读取按键状态）。
## 3. PWM (控制舵机)

HAL_TIM_PWM_Init()：初始化定时器 PWM 模式。
HAL_TIM_PWM_Start() / HAL_TIM_PWM_Stop()：开启/停止 PWM 输出。
__HAL_TIM_SET_COMPARE()：核心函数，修改 CCR 寄存器的值，从而改变 PWM 占空比（控制舵机角度）。
__HAL_TIM_SET_AUTORELOAD()：修改 ARR 寄存器，改变 PWM 频率。
HAL_TIM_PWM_Start_DMA()：如果需要 DMA 传输占空比数据。
## 4. UART (两个串口)

HAL_UART_Init()：初始化串口（波特率、字长、停止位等）。
阻塞方式：HAL_UART_Transmit()、HAL_UART_Receive()。
中断方式 (IT)：HAL_UART_Transmit_IT()、HAL_UART_Receive_IT()、HAL_UART_IRQHandler()。
DMA 方式：HAL_UART_Transmit_DMA()、HAL_UART_Receive_DMA()、HAL_UARTEx_ReceiveToIdle_DMA()（常用于接收不定长数据）。
回调函数：HAL_UART_RxCpltCallback()、HAL_UARTEx_RxEventCallback()（数据接收完成后自动调用）。
__HAL_UART_ENABLE_IT()：使能空闲中断（IDLE）等。
## 5. CAN (配置所有 CAN)

HAL_CAN_Init()：初始化 CAN 外设（位时序、工作模式）。
HAL_CAN_ConfigFilter()：配置过滤器（非常重要，不配置收不到数据）。
HAL_CAN_Start() / HAL_CAN_Stop()：启动/停止 CAN。
HAL_CAN_ActivateNotification()：使能接收中断（如 CAN_IT_RX_FIFO0_MSG_PENDING）。
HAL_CAN_AddTxMessage()：发送 CAN 报文。
HAL_CAN_GetRxMessage()：从 FIFO 读取接收到的报文。
回调函数：HAL_CAN_RxFifo0MsgPendingCallback()（接收中断回调）。
## 6. SPI (根据数据手册)

HAL_SPI_Init()：初始化 SPI（主从模式、时钟极性/相位 CPOL/CPHA、数据大小）。
HAL_SPI_Transmit() / HAL_SPI_Receive()：阻塞收发。
HAL_SPI_TransmitReceive()：全双工收发。
HAL_SPI_TransmitReceive_IT() / _DMA()：中断/DMA 收发。
HAL_SPI_TxRxCpltCallback()：收发完成回调。
__HAL_SPI_ENABLE() / __HAL_SPI_DISABLE()：手动控制 SPI 使能（常用于软件片选 CS）。
## 7. I²C (根据数据手册)

HAL_I2C_Init()：初始化 I2C（时钟频率、占空比、寻址模式）。
HAL_I2C_Master_Transmit() / HAL_I2C_Master_Receive()：主机阻塞收发。
HAL_I2C_Mem_Write() / HAL_I2C_Mem_Read()：读写特定寄存器（最常用于传感器）。
HAL_I2C_Master_Transmit_DMA() / _IT()：中断/DMA 收发。
HAL_I2C_MemRxCpltCallback()：读写完成回调。
HAL_I2C_IsDeviceReady()：检测设备是否在位。
## 8. Thread (RTOS任务、优先级、栈空间、内存分配)

假设使用的是 FreeRTOS（CubeMX 中通常以 CMSIS-RTOS v1/v2 封装）：

线程创建：

osThreadNew() (CMSIS-RTOS v2) / xTaskCreate() (原生 FreeRTOS)。
参数中需指定：任务函数、任务名、栈空间大小 (Stack Depth)、传入参数、优先级 (Priority)、任务句柄。
任务控制：

osDelay() / vTaskDelay()：任务延时（释放 CPU）。
osThreadTerminate() / vTaskDelete()：删除任务。
osThreadSuspend() / vTaskSuspend()：挂起任务。
内存分配：

pvPortMalloc() / vPortFree()：FreeRTOS 自带的内存管理（对应 heap_1~5.c）。
xPortGetFreeHeapSize()：获取当前剩余堆空间（用于调试内存是否耗尽）。
osMemoryPoolNew() / osMessageQueueNew()：CMSIS-RTOS v2 的内存池和消息队列创建。
## 9. 整体的堆栈空间 (Heap & Stack)

这部分通常不涉及具体的 C 函数调用，而是在 启动文件 (startup_xxx.s) 和 链接脚本 (xxx.ld) 中配置：

_estack：栈顶地址（在启动文件中定义）。
_Min_Heap_Size / _Min_Stack_Size：在链接脚本中定义的堆和栈的最小大小。
如果在代码中动态分配，会用到标准库的 malloc() / free()，或者 FreeRTOS 的 pvPortMalloc()。
