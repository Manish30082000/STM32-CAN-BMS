/*
 * bms_data.h
 *
 *  Created on: 29-Nov-2025
 *      Author: asus
 */

#ifndef INC_BMS_DATA_H_
#define INC_BMS_DATA_H_

#include <stdint.h>
#include <stdbool.h>

#define MAX_CELLS 16
#define MAX_INDEXES 15
#define MAX_OEMS 4
#define MAX_BMS_PER_OEM 8


typedef enum { BMS_STATE_UNKNOWN=0, BMS_STATE_ACTIVE, BMS_STATE_STALE } bms_state_t;


typedef struct {
uint8_t used; // 0 = free, 1 = used
uint8_t manuf_encoded; // OEM encoded byte
uint32_t short_id24; // short id from CAN id
uint64_t unique_id40; // canonical id from data[1..5]

// Measurements
float pack_voltage; // V
float pack_current; // A
int8_t pack_temp; // degC

uint16_t cell_mV[MAX_CELLS];
uint8_t num_cells;
char protocol_version[8];
char battery_serial[12]; // space for 7 chars + NUL + spare

uint16_t cycle_count;
uint8_t soc_percent;
uint8_t soh_percent;
uint32_t protection_flags;
uint8_t load_status;
uint16_t batt_capacity;
bool charging_sw_status;
bool discharging_sw_status;
int8_t cell_thermistor[10];

uint8_t ambient_sensor;
int8_t mosfet_temp;

// timestamps
uint32_t last_rx_tick; // HAL_GetTick() ms
uint32_t last_index_ts[MAX_INDEXES]; // ms per index

bool high_cell_voltage_alarm;
bool low_cell_voltage_alarm;
bool battery_high_voltage_alarm;
bool battery_low_voltage_alarm;
bool charge_overcurrent_alarm;
bool discharge_current_alarm;
bool charging_high_temeprature_alarm;
bool cell_charging_low_temperature_alarm;

bool discharge_low_temeprature_alarm;
bool discharge_mosfet_high_temperature_alarm;
bool ambient_low_temperature_alarm;
bool ambient_high_temperature_alarm;
bool charge_mosfet_high_temperature_alarm;

bool cell_over_voltage_protection;
bool cell_low_voltage_protection;
bool battery_over_voltage_protection;
bool battery_low_voltage_protection;
bool charge_overcurrent_protection;
bool discharge_overcurrent_protection;
bool short_circuit_protection;

bool charge_high_temperature_protection;
bool charge_low_temperature_protection;
bool discharge_high_temperature_protection;
bool thermal_runaway;
bool discharge_low_temperature_protection;
bool discharge_mosfet_high_temperature_protection;
bool ambient_high_temperature_protection;
bool charge_mosfet_high_temperature_protection;

bool ambient_low_temperature_protection;
bool discharge_mosfet_failure;
bool charge_mosfet_failure;
bool afe_communication_failure;


bms_state_t state;
} bms_entry_t;

// OEM grouped table
extern bms_entry_t g_bms_table[MAX_OEMS][MAX_BMS_PER_OEM];
void bms_table_init(void);

#endif /* INC_BMS_DATA_H_ */
