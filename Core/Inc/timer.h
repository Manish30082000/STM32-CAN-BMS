/*
 * timer.h
 *
 *  Created on: 29-Nov-2025
 *      Author: asus
 */

#ifndef INC_TIMER_H_
#define INC_TIMER_H_

#include <stdint.h>


void timers_start(void);
uint32_t micros(void);
uint64_t micros64(void);
int timers_poll_housekeeping(uint32_t interval_ms);
void timers_ms_tick_handler(void);

#endif /* INC_TIMER_H_ */
