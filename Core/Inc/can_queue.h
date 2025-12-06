/*
 * can_queue.h
 *
 *  Created on: 29-Nov-2025
 *      Author: asus
 */

#ifndef INC_CAN_QUEUE_H_
#define INC_CAN_QUEUE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
    uint32_t timestamp_us;
} can_frame_t;

#define CAN_QUEUE_SIZE 64   // Must be power of 2: 32/64/128/256 etc.

void canq_init(void);
int  canq_push_isr(const can_frame_t *f);    // called in ISR
int  canq_pop(can_frame_t *out);             // called in main

extern volatile uint32_t canq_drop_count;

#ifdef __cplusplus
}
#endif

#endif /* INC_CAN_QUEUE_H_ */
