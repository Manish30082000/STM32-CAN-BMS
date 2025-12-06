/*
 * bms_data.c
 *
 *  Created on: 29-Nov-2025
 *      Author: asus
 */


#include "bms_data.h"
#include <string.h>


bms_entry_t g_bms_table[MAX_OEMS][MAX_BMS_PER_OEM];


// initialize table (call once during startup)
void bms_table_init(void) {
for (int o=0;o<MAX_OEMS;o++){
for (int i=0;i<MAX_BMS_PER_OEM;i++){
g_bms_table[o][i].used = 0;
}
}
}
