/*
 * timer.c
 *
 *  Created on: 29-Nov-2025
 *      Author: asus
 */


#include "timer.h"
#include "stm32f4xx_hal.h"


extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim5;


static volatile uint32_t ms_accumulator = 0;
static volatile uint32_t tim5_last = 0;
static volatile uint64_t tim5_overflow_acc = 0;


void timers_start(void){
HAL_TIM_Base_Start(&htim5);
HAL_TIM_Base_Start_IT(&htim2);
}


uint32_t micros(void){
return __HAL_TIM_GET_COUNTER(&htim5);
}


uint64_t micros64(void){
uint32_t now = __HAL_TIM_GET_COUNTER(&htim5);
__disable_irq();
if (now < tim5_last) tim5_overflow_acc += ((uint64_t)1<<32);
tim5_last = now;
uint64_t hi = tim5_overflow_acc;
__enable_irq();
return hi | (uint64_t)now;
}


void timers_ms_tick_handler(void){
ms_accumulator++;
}


int timers_poll_housekeeping(uint32_t interval_ms){
static uint32_t last = 0;
uint32_t now;
__disable_irq(); now = ms_accumulator; __enable_irq();
if ((now - last) >= interval_ms){ last = now; return 1; }
return 0;
}
