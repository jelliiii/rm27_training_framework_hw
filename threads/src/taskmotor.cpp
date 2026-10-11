#include "main.h"
#include "tx_api.h"
#include "M2006.hpp"



TX_THREAD motor_thread;
uint8_t motor_thread_stack[1024]={0};

[[noreturn]] void motor_thread_entry(ULONG thread_input){

    UNUSED(thread_input);

    while (1) {
    }
}
