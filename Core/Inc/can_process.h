/*
 * can_process.h
 *
 *  Created on: 29-Nov-2025
 *      Author: asus
 */

#ifndef INC_CAN_PROCESS_H_
#define INC_CAN_PROCESS_H_

#include <stdint.h>
#include "bms_data.h"


void process_can_frames(void); // call from main loop
void process_can_frame_full(uint32_t can_id, const uint8_t data[8], uint32_t t_us);
int can_send_fet_control(uint32_t target_id_ext, uint8_t dsg_cmd, uint8_t chg_cmd);
void can_send_buzzer_query(uint32_t target_id_ext);
bool parse_std_ids(uint32_t can_id, const uint8_t data[8]);
void process_suspension_msg(uint32_t id,const uint8_t d[8]);

extern uint8_t charger_brd[8];
static volatile uint8_t fet_pending = 0;    // 1 = pending send
extern bool adaptive_charging;
extern bool send_heartbeat;
#define FET_MAX_TX_RETRIES  3
#define FET_RETRY_INTERVAL_MS  50   // retry quickly if mailbox busy


extern bool buzzer_on;
typedef struct
{
	bool batt_is_not_charging;
	bool batt_is_pseudo_mode;
	bool batt_is_cc_mode;
	bool batt_is_cv_mode;
	bool batt_is_charging_complete;
	int8_t batt_temperature;
	bool chr_is_not_charging;
	bool chr_is_pseudo_mode;
	bool chr_is_cc_mode;
	bool chr_is_cv_mode;
	bool chr_is_charging_complete;
	int8_t chr_temperature;
	uint8_t soc;
	uint8_t bms_status;
	uint8_t charging_status;
	uint16_t set_charger_volt;
	uint16_t set_charger_curr;
	uint16_t max_charging_volt;
	uint16_t max_curr;
	uint16_t actual_volt;
	uint16_t actual_curr;
	uint8_t batt_over_volt;
	uint8_t batt_over_curr;
	uint8_t batt_over_temp;
	uint8_t batt_under_temp;
	uint8_t batt_delta_curr_ch_batt;
	uint8_t batt_delta_volt_ch_batt;
	uint8_t batt_delta_rise_temp;
	uint8_t charger_over_volt;
	uint8_t charger_over_curr;
	uint8_t charger_over_temp;
	uint8_t charger_under_temp;
	uint8_t charger_delta_curr_ch_batt;
	uint8_t charger_delta_volt_ch_batt;
	uint8_t charger_delta_rise_temp;
	uint16_t charging_volt;
	uint16_t charging_curr;

	bool host_high_cell_voltage_alarm;         //  0
		bool host_low_cell_voltage_alarm;          //  1
		bool host_battery_high_voltage_alarm;      //  2
		bool host_battery_low_voltage_alarm;       //  3
		bool host_charge_overcurrent_alarm;        //  4
		bool host_discharge_current_alarm;         //  5
		bool host_charging_high_temp_alarm;        //  6
		bool host_charging_low_temp_alarm;         //  7

		// ---- Alarm Byte 2 ----
		bool host_discharge_low_temp_alarm;        //  0
		bool host_mos_high_temp_alarm;             //  1
		bool host_ambient_low_temp_alarm;          //  2
		bool host_ambient_high_temp_alarm;         //  3

		// ---- Protection Byte 1 ----
		bool host_cell_over_voltage_prot;          //  0
		bool host_cell_low_voltage_prot;           //  1
		bool host_battery_over_voltage_prot;       //  2
		bool host_battery_low_voltage_prot;        //  3
		bool host_charge_overcurrent_prot;         //  4
		bool host_discharge_overcurrent_prot;      //  5
		bool host_short_circuit_prot;              //  6

		// ---- Protection Byte 2 ----
		bool host_charge_high_temp_prot;           //  0
		bool host_charge_low_temp_prot;            //  1
		bool host_discharge_high_temp_prot;        //  2
		bool host_thermal_runaway_prot;            //  3
		bool host_discharge_low_temp_prot;         //  4
		bool host_mos_high_temp_prot;              //  5
		bool host_ambient_high_temp_prot;          //  6
		bool slave_high_cell_voltage_alarm;         //  0
			bool slave_low_cell_voltage_alarm;          //  1
			bool slave_battery_high_voltage_alarm;      //  2
			bool slave_battery_low_voltage_alarm;       //  3
			bool slave_charge_overcurrent_alarm;        //  4
			bool slave_discharge_current_alarm;         //  5
			bool slave_charging_high_temp_alarm;        //  6
			bool slave_charging_low_temp_alarm;         //  7

			// ---- Alarm Byte 2 ----
			bool slave_discharge_low_temp_alarm;        //  0
			bool slave_mos_high_temp_alarm;             //  1
			bool slave_ambient_low_temp_alarm;          //  2
			bool slave_ambient_high_temp_alarm;         //  3

			// ---- Protection Byte 1 ----
			bool slave_cell_over_voltage_prot;          //  0
			bool slave_cell_low_voltage_prot;           //  1
			bool slave_battery_over_voltage_prot;       //  2
			bool slave_battery_low_voltage_prot;        //  3
			bool slave_charge_overcurrent_prot;         //  4
			bool slave_discharge_overcurrent_prot;      //  5
			bool slave_short_circuit_prot;              //  6

			// ---- Protection Byte 2 ----
			bool slave_charge_high_temp_prot;           //  0
			bool slave_charge_low_temp_prot;            //  1
			bool slave_discharge_high_temp_prot;        //  2
			bool slave_thermal_runaway_prot;            //  3
			bool slave_discharge_low_temp_prot;         //  4
			bool slave_mos_high_temp_prot;              //  5
			bool slave_ambient_high_temp_prot;          //  6

}battery_status;

extern battery_status battery_standard;

#endif /* INC_CAN_PROCESS_H_ */
