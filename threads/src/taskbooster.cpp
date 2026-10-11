// in taskbooster.cpp
#include "taskbooster.hpp"

#include "main.h"
#include "tx_api.h"

extern TX_THREAD led_thread;  //线程控制块
extern uint8_t led_thread_stack[1024];  //线程栈
extern void led_thread_entry(ULONG thread_input); //线程入口函数

#define TX_NAME(s) const_cast<CHAR*>(s)
extern "C" void taskbooster(void)
{
    //  /**************创建启动任务*********************/
    // tx_thread_create(&my_thread1,              /* 任务控制块地址 */   
    //                    TX_NAME("my_thread1"),         /* 任务名 */
    //                    my_thread_entry1,              /* 启动任务函数地址 */,                             /* 传递给任务的参数 */
    //                    0x1234,                    /* 堆栈基地址,默认即可 */
    //                    my_thread_stack1,            /* 堆栈名字 */ 
    //                    sizeof(my_thread_stack1)，    /* 堆栈空间大小 */  
    //                    10,                            /* 任务优先级，1-20*/
    //                    10,                            /* 任务抢占阀值 */
    //                    TX_NO_TIME_SLICE,               /* 不开启时间片 */
    //                    TX_AUTO_START);                 /* 创建后立即启动 */

    // Implement the task booster functionality here  
    UINT status = tx_thread_create(&led_thread, TX_NAME("LED Thread"), led_thread_entry, 0x1234,
                     led_thread_stack, sizeof(led_thread_stack),
                     10, 10, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (status != TX_SUCCESS)
    {
        Error_Handler();
    }
}
