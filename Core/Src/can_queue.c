/*
 * can_queue.c
 *
 *  Created on: 29-Nov-2025
 *      Author: asus
 */

#include "can_queue.h"
#include <string.h>

static volatile can_frame_t canq[CAN_QUEUE_SIZE];
static volatile uint16_t head = 0;
static volatile uint16_t tail = 0;

volatile uint32_t canq_drop_count = 0;

void canq_init(void)
{
    head = 0;
    tail = 0;
    canq_drop_count = 0;
}

// ISR-safe push
int canq_push_isr(const can_frame_t *f)
{
    uint16_t next = (head + 1) & (CAN_QUEUE_SIZE - 1);

    if (next == tail) {
        // queue full → drop frame
        canq_drop_count++;
        return -1;
    }

    // shallow copy of frame (fast)
    canq[head] = *f;
    head = next;
    return 0;
}

// main-loop pop (non-ISR)
int canq_pop(can_frame_t *out)
{
    if (tail == head) {
        return -1; // empty
    }

    *out = canq[tail];
    tail = (tail + 1) & (CAN_QUEUE_SIZE - 1);
    return 0;
}

