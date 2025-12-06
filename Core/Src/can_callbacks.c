/*
 * can_callbacks.c
 *
 *  Created on: 29-Nov-2025
 *      Author: asus
 */


#include "stm32f4xx_hal.h"
#include "can_queue.h"
#include "timer.h"   // micros()
#include "can_process.h"
extern CAN_HandleTypeDef hcan1;

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rxh;
    can_frame_t f;

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxh, f.data) != HAL_OK)
        return;

    if (rxh.IDE == CAN_ID_EXT)
    {
        f.id = rxh.ExtId & 0x1FFFFFFF;
    }
    else
    {
        f.id = rxh.StdId & 0x7FF;
    }

    f.dlc = rxh.DLC;
    f.timestamp_us = micros();

    if(f.id == 0x522 || f.id == 0x531 || f.id == 0x1E1)
    {
    	process_suspension_msg(f.id,f.data);
    	return;
    }
    // push it into queue (fast, constant time)
    canq_push_isr(&f);
}
