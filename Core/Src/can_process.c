/*
 * can_process.c
 *
 *  Created on: 29-Nov-2025
 *      Author: asus
 */

#include "stm32f4xx_hal.h"
#include "can_process.h"
#include "can_queue.h"
#include "bms_data.h"
#include "timer.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

extern CAN_HandleTypeDef hcan1;
battery_status battery_standard ={0};
uint8_t charger_brd[8] ={0};
bool buzzer_on = false;

static uint8_t  fet_retry_count = 0;
static uint8_t  fet_desired_dsg = 0;
static uint8_t  fet_desired_chg = 0;
static uint32_t fet_last_tx_try_ms = 0;
bool adaptive_charging = true;
bool send_heartbeat = false;

static uint64_t parse_unique40(const uint8_t d[8]){
	uint64_t id = 0;
	for (int i=1;i<=5;i++) id = (id<<8) | (uint64_t)d[i];
	return id;
}


static int find_oem_slot(uint8_t manuf){
	if (manuf < MAX_OEMS) return (int)manuf;
	return -1;
}


static int find_entry_by_unique(int oem, uint64_t unique){
	if (!unique) return -1;
	for (int i=0;i<MAX_BMS_PER_OEM;i++){
		if (g_bms_table[oem][i].used && g_bms_table[oem][i].unique_id40 == unique) return i;
	}
	return -1;
}


static int find_entry_by_short(int oem, uint32_t shortid){
	for (int i=0;i<MAX_BMS_PER_OEM;i++){
		if (g_bms_table[oem][i].used && g_bms_table[oem][i].short_id24 == shortid) return i;
	}
	return -1;
}


static int create_entry(int oem, uint8_t manuf, uint32_t shortid, uint64_t unique40){
	for (int i=0;i<MAX_BMS_PER_OEM;i++){
		if (!g_bms_table[oem][i].used){
			// initialize
			g_bms_table[oem][i].used = 1;
			g_bms_table[oem][i].manuf_encoded = manuf;
			g_bms_table[oem][i].short_id24 = shortid;
			g_bms_table[oem][i].unique_id40 = unique40;
			g_bms_table[oem][i].state = BMS_STATE_ACTIVE;
			g_bms_table[oem][i].last_rx_tick = HAL_GetTick();
			memset(g_bms_table[oem][i].last_index_ts, 0, sizeof(g_bms_table[oem][i].last_index_ts));
			return i;
		}
	}
	// evict oldest
	uint32_t oldest = 0xFFFFFFFF; int oldest_idx=-1;
	for (int i=0;i<MAX_BMS_PER_OEM;i++){
		if (g_bms_table[oem][i].used && g_bms_table[oem][i].last_rx_tick < oldest){
			oldest = g_bms_table[oem][i].last_rx_tick; oldest_idx = i;
		}
	}
	if (oldest_idx>=0){
		memset(&g_bms_table[oem][oldest_idx],0,sizeof(bms_entry_t));
		g_bms_table[oem][oldest_idx].used = 1;
		g_bms_table[oem][oldest_idx].manuf_encoded = manuf;
		g_bms_table[oem][oldest_idx].short_id24 = shortid;
		g_bms_table[oem][oldest_idx].unique_id40 = unique40;
		g_bms_table[oem][oldest_idx].state = BMS_STATE_ACTIVE;
		g_bms_table[oem][oldest_idx].last_rx_tick = HAL_GetTick();
		return oldest_idx;
	}
	return -1;
}


static void parse_index1(bms_entry_t *e, const uint8_t d[8])
{
	uint16_t raw_v = ((uint16_t)d[1]<<8) | d[2];
	uint16_t raw_i = ((uint16_t)d[3]<<8) | d[4];
	int8_t tmp = (int8_t)d[5];
	e->pack_voltage = raw_v / 100.0f;
	e->pack_current = ((int16_t)raw_i) / 100.0f;
	e->pack_temp = tmp;
	e->load_status = d[6];
}

static void parse_index2(bms_entry_t *e, const uint8_t d[8])
{
	e->soh_percent = d[1];
	e->soc_percent = d[2];
	e->cycle_count = (d[3] << 8) | d[4];
	e->batt_capacity = (d[5] <<8) | d[6];
}

void parse_index3(bms_entry_t *e, const uint8_t d[8])
{
	e->charging_sw_status  = (d[1] >> 1) & 0x01;
	e->discharging_sw_status = d[1] & 0x01;
	e->cell_thermistor[9] = d[5];
	e->cell_thermistor[8] = d[6];
	e->cell_thermistor[7] = d[7];
}

void parse_index4(bms_entry_t *e, const uint8_t d[8])
{
	e->cell_thermistor[0] = d[1];
	e->cell_thermistor[1] = d[2];
	e->cell_thermistor[2] = d[3];
	e->cell_thermistor[3] = d[4];
	e->cell_thermistor[4] = d[5];
	e->cell_thermistor[5] = d[6];
	e->cell_thermistor[6] = d[7];
}
void parse_index5(bms_entry_t *e, const uint8_t d[8])
{
	const size_t src_len = 6;         // d[1]..d[6]
	size_t dst_max = sizeof(e->protocol_version);

	if (dst_max == 0) return;

	size_t copy = (dst_max - 1 < src_len) ? dst_max - 1 : src_len;

	for (size_t i = 0; i < copy; ++i) {
		uint8_t ch = d[1 + i];                // start from d[1]
		e->protocol_version[i] = (ch >= 32 && ch <= 126) ? (char)ch : '?';
	}
	e->protocol_version[copy] = '\0';
}
void parse_index6(bms_entry_t *e, const uint8_t d[8])
{
	const size_t src_len = 7;         // d[1]..d[7]
	size_t dst_max = sizeof(e->battery_serial);

	if (dst_max == 0) return;

	size_t copy = (dst_max - 1 < src_len) ? dst_max - 1 : src_len;

	for (size_t i = 0; i < copy; ++i) {
		uint8_t ch = d[1 + i]; // start at d[1]
		e->battery_serial[i] = (ch >= 32 && ch <= 126) ? (char)ch : '?';
	}
	e->battery_serial[copy] = '\0';
}

void parse_index7(bms_entry_t *e, const uint8_t d[8])
{
	e->ambient_sensor = d[1];
	e->mosfet_temp = d[2];
}

void parse_index8(bms_entry_t *e, const uint8_t d[8])
{
	e->high_cell_voltage_alarm = (d[1] >> 0) & 0x01;
	e->low_cell_voltage_alarm = (d[1] >> 1) & 0x01;
	e->battery_high_voltage_alarm = (d[1] >> 2) & 0x01;
	e->battery_low_voltage_alarm = (d[1] >> 3) & 0x01;
	e->charge_overcurrent_alarm = (d[1] >> 4) & 0x01;
	e->discharge_current_alarm = (d[1] >> 5) & 0x01;
	e->charging_high_temeprature_alarm = (d[1] >> 6) & 0x01;
	e->cell_charging_low_temperature_alarm = (d[1] >> 7) & 0x01;

	e->discharge_low_temeprature_alarm = (d[2] >> 0) & 0x01;
	e->discharge_mosfet_high_temperature_alarm = (d[2] >> 1) & 0x01;
	e->ambient_low_temperature_alarm = (d[2] >> 2) & 0x01;
	e->ambient_high_temperature_alarm = (d[2] >> 3) & 0x01;
	e->charge_mosfet_high_temperature_alarm = (d[2] >> 4) & 0x01;

	e->cell_over_voltage_protection = (d[3] >> 0) & 0x01;
	e->cell_low_voltage_protection = (d[3] >> 1) & 0x01;
	e->battery_over_voltage_protection = (d[3] >> 2) & 0x01;
	e->battery_low_voltage_protection = (d[3] >> 3) & 0x01;
	e->charge_overcurrent_protection = (d[3] >> 4) & 0x01;
	e->discharge_overcurrent_protection = (d[3] >> 5) & 0x01;
	e->short_circuit_protection = (d[3] >> 6) & 0x01;

	e->charge_high_temperature_protection           = (d[4] >> 0) & 0x01;
	e->charge_low_temperature_protection            = (d[4] >> 1) & 0x01;
	e->discharge_high_temperature_protection        = (d[4] >> 2) & 0x01;
	e->thermal_runaway                              = (d[4] >> 3) & 0x01;
	e->discharge_low_temperature_protection         = (d[4] >> 4) & 0x01;
	e->discharge_mosfet_high_temperature_protection = (d[4] >> 5) & 0x01;
	e->ambient_high_temperature_protection          = (d[4] >> 6) & 0x01;
	e->charge_mosfet_high_temperature_protection    = (d[4] >> 7) & 0x01;

	e->ambient_low_temperature_protection = (d[5] >> 0) & 0x01;
	e->discharge_mosfet_failure           = (d[5] >> 1) & 0x01;
	e->charge_mosfet_failure              = (d[5] >> 2) & 0x01;
	e->afe_communication_failure          = (d[5] >> 3) & 0x01;
}

void parse_index9(bms_entry_t *e, const uint8_t d[8])
{
	e->cell_mV[0] = (((uint16_t)d[1] << 8) | d[2]) / 1000;
	e->cell_mV[1] = (((uint16_t)d[3] << 8) | d[4]) / 1000;
	e->cell_mV[2] = (((uint16_t)d[5] << 8) | d[6]) / 1000;
}

void parse_index10(bms_entry_t *e, const uint8_t d[8])
{
	e->cell_mV[3] = (((uint16_t)d[1] << 8) | d[2]) / 1000;
	e->cell_mV[4] = (((uint16_t)d[3] << 8) | d[4]) / 1000;
	e->cell_mV[5] = (((uint16_t)d[5] << 8) | d[6]) / 1000;
}

void parse_index11(bms_entry_t *e, const uint8_t d[8])
{
	e->cell_mV[6] = (((uint16_t)d[1] << 8) | d[2]) / 1000;
	e->cell_mV[7] = (((uint16_t)d[3] << 8) | d[4]) / 1000;
	e->cell_mV[8] = (((uint16_t)d[5] << 8) | d[6]) / 1000;
}

void parse_index12(bms_entry_t *e, const uint8_t d[8])
{
	e->cell_mV[9]  = (((uint16_t)d[1] << 8) | d[2]) / 1000;
	e->cell_mV[10] = (((uint16_t)d[3] << 8) | d[4]) / 1000;
	e->cell_mV[11] = (((uint16_t)d[5] << 8) | d[6]) / 1000;
}

void parse_index13(bms_entry_t *e, const uint8_t d[8])
{
	e->cell_mV[12] = (((uint16_t)d[1] << 8) | d[2]) / 1000;
	e->cell_mV[13] = (((uint16_t)d[3] << 8) | d[4]) / 1000;
	e->cell_mV[14] = (((uint16_t)d[5] << 8) | d[6]) / 1000;
}

void parse_index14(bms_entry_t *e, const uint8_t d[8])
{
	e->cell_mV[15] = ((uint16_t)d[1] << 8) | d[2] / 1000;
}

// Helper: send a raw 8-byte extended CAN frame. Returns 0 on success, -1 on failure.
static int can_tx_ext(uint32_t ext_id, const uint8_t data[8], uint8_t dlc)
{
	CAN_TxHeaderTypeDef txh;
	uint32_t txmailbox;

	txh.ExtId = ext_id & 0x1FFFFFFF;
	txh.IDE = CAN_ID_EXT;
	txh.RTR = CAN_RTR_DATA;
	txh.DLC = dlc;
	txh.TransmitGlobalTime = DISABLE;

	if (HAL_CAN_AddTxMessage(&hcan1, &txh, (uint8_t *)data, &txmailbox) != HAL_OK) {
		// TX request failed (mailboxes full or HAL error)
		return -1;
	}
	// Optionally you can wait for mailbox to be free or check status
	return 0;
}

static int send_bms_response(uint32_t std_id, const uint8_t data[8], uint8_t dlc)
{
	CAN_TxHeaderTypeDef txh;
	uint32_t txmailbox;

	txh.StdId = std_id;
	txh.IDE = CAN_ID_STD;
	txh.RTR = CAN_RTR_DATA;
	txh.DLC = dlc;
	txh.TransmitGlobalTime = DISABLE;

	if (HAL_CAN_AddTxMessage(&hcan1, &txh, (uint8_t *)data, &txmailbox) != HAL_OK) {
		// TX request failed (mailboxes full or HAL error)
		return -1;
	}
	// Optionally you can wait for mailbox to be free or check status
	return 0;
}

int can_send_fet_control(uint32_t target_id_ext, uint8_t dsg_cmd, uint8_t chg_cmd)
{
	uint8_t data[8] = {0};
	data[0] = 0x0F;
	data[1] = dsg_cmd;   // 0x01 or 0x02
	data[2] = chg_cmd;   // 0x01 or 0x02
	data[3] = 0x02;
	data[4] = 0x00;
	data[5] = 0x00;
	data[6] = 0x00;
	data[7] = 0x00;

	return can_tx_ext(target_id_ext, data, 8);
}

void can_send_buzzer_query(uint32_t target_id_ext)
{
	// Some datasheets use same ID for query/response. The "query" frame often is 8 bytes of zeros.
	uint8_t data[8] = {0};
	(void) can_tx_ext(target_id_ext, data, 8);
}

//handle critical frames directly called from isr
void process_suspension_msg(uint32_t id, const uint8_t d[8])
{
	switch(id)
	{
	case 0x1E1:
	{
		memcpy(charger_brd,d,sizeof(uint8_t)*8);
	}
	break;
	case 0x522:
	{
		battery_standard.batt_over_volt = d[0];
		battery_standard.batt_over_curr = d[1];
		battery_standard.batt_over_temp = d[2];
		battery_standard.batt_under_temp = d[3];
		battery_standard.batt_delta_curr_ch_batt = d[4];
		battery_standard.batt_delta_volt_ch_batt = d[5];
		battery_standard.batt_delta_rise_temp = d[6];
	}
	break;
	case 0x531:
	{
		battery_standard.charger_over_volt = d[0];
		battery_standard.charger_over_curr = d[1];
		battery_standard.charger_over_temp = d[2];
		battery_standard.charger_under_temp = d[3];
		battery_standard.charger_delta_curr_ch_batt = d[4];
		battery_standard.charger_delta_volt_ch_batt = d[5];
		battery_standard.charger_delta_rise_temp = d[6];
	}
	break;
	default:
		break;
	}
}

bool parse_std_ids(uint32_t can_id, const uint8_t d[8])
{
	switch(can_id)
	{
	case 0x520:
	{
		battery_standard.batt_is_not_charging = (d[0] == 0x00);
		battery_standard.batt_is_pseudo_mode = (d[0] == 0x01);
		battery_standard.batt_is_cc_mode = (d[0] == 0x02);
		battery_standard.batt_is_cv_mode = (d[0] == 0x03);
		battery_standard.batt_is_charging_complete = (d[0] == 0x04);

		battery_standard.batt_temperature = d[1];
		battery_standard.soc = d[2];
		battery_standard.bms_status = d[3];
		battery_standard.set_charger_volt = ((uint16_t)d[4] << 8) | d[5];
		battery_standard.set_charger_curr = ((uint16_t)d[6] << 8) | d[7];
	}
	break;
	case 0x521:
	{
		battery_standard.max_charging_volt = ((uint16_t)d[0] << 8) | d[1];
		battery_standard.max_curr = ((uint16_t)d[2] << 8) | d[3];
		battery_standard.actual_volt = ((uint16_t)d[4] << 8) | d[5];
		battery_standard.actual_curr = ((uint16_t)d[6] << 8) | d[7];
	}
	break;
	case 0x523:
	{
		battery_standard.host_high_cell_voltage_alarm     = (d[0] >> 0) & 1;
		battery_standard.host_low_cell_voltage_alarm      = (d[0] >> 1) & 1;
		battery_standard.host_battery_high_voltage_alarm  = (d[0] >> 2) & 1;
		battery_standard.host_battery_low_voltage_alarm   = (d[0] >> 3) & 1;
		battery_standard.host_charge_overcurrent_alarm    = (d[0] >> 4) & 1;
		battery_standard.host_discharge_current_alarm     = (d[0] >> 5) & 1;
		battery_standard.host_charging_high_temp_alarm    = (d[0] >> 6) & 1;
		battery_standard.host_charging_low_temp_alarm     = (d[0] >> 7) & 1;

		battery_standard.host_discharge_low_temp_alarm    = (d[1] >> 0) & 1;
		battery_standard.host_mos_high_temp_alarm         = (d[1] >> 1) & 1;
		battery_standard.host_ambient_low_temp_alarm      = (d[1] >> 2) & 1;
		battery_standard.host_ambient_high_temp_alarm     = (d[1] >> 3) & 1;

		battery_standard.host_cell_over_voltage_prot      = (d[2] >> 0) & 1;
		battery_standard.host_cell_low_voltage_prot       = (d[2] >> 1) & 1;
		battery_standard.host_battery_over_voltage_prot   = (d[2] >> 2) & 1;
		battery_standard.host_battery_low_voltage_prot    = (d[2] >> 3) & 1;
		battery_standard.host_charge_overcurrent_prot     = (d[2] >> 4) & 1;
		battery_standard.host_discharge_overcurrent_prot  = (d[2] >> 5) & 1;
		battery_standard.host_short_circuit_prot          = (d[2] >> 6) & 1;

		battery_standard.host_charge_high_temp_prot       = (d[3] >> 0) & 1;
		battery_standard.host_charge_low_temp_prot        = (d[3] >> 1) & 1;
		battery_standard.host_discharge_high_temp_prot    = (d[3] >> 2) & 1;
		battery_standard.host_thermal_runaway_prot        = (d[3] >> 3) & 1;
		battery_standard.host_discharge_low_temp_prot     = (d[3] >> 4) & 1;
		battery_standard.host_mos_high_temp_prot          = (d[3] >> 5) & 1;
		battery_standard.host_ambient_high_temp_prot      = (d[3] >> 6) & 1;
	}
	break;
	case 0x525:
	{
		battery_standard.slave_high_cell_voltage_alarm     = (d[0] >> 0) & 1;
		battery_standard.slave_low_cell_voltage_alarm      = (d[0] >> 1) & 1;
		battery_standard.slave_battery_high_voltage_alarm  = (d[0] >> 2) & 1;
		battery_standard.slave_battery_low_voltage_alarm   = (d[0] >> 3) & 1;
		battery_standard.slave_charge_overcurrent_alarm    = (d[0] >> 4) & 1;
		battery_standard.slave_discharge_current_alarm     = (d[0] >> 5) & 1;
		battery_standard.slave_charging_high_temp_alarm    = (d[0] >> 6) & 1;
		battery_standard.slave_charging_low_temp_alarm     = (d[0] >> 7) & 1;

		battery_standard.slave_discharge_low_temp_alarm    = (d[1] >> 0) & 1;
		battery_standard.slave_mos_high_temp_alarm         = (d[1] >> 1) & 1;
		battery_standard.slave_ambient_low_temp_alarm      = (d[1] >> 2) & 1;
		battery_standard.slave_ambient_high_temp_alarm     = (d[1] >> 3) & 1;

		battery_standard.slave_cell_over_voltage_prot      = (d[2] >> 0) & 1;
		battery_standard.slave_cell_low_voltage_prot       = (d[2] >> 1) & 1;
		battery_standard.slave_battery_over_voltage_prot   = (d[2] >> 2) & 1;
		battery_standard.slave_battery_low_voltage_prot    = (d[2] >> 3) & 1;
		battery_standard.slave_charge_overcurrent_prot     = (d[2] >> 4) & 1;
		battery_standard.slave_discharge_overcurrent_prot  = (d[2] >> 5) & 1;
		battery_standard.slave_short_circuit_prot          = (d[2] >> 6) & 1;

		battery_standard.slave_charge_high_temp_prot       = (d[3] >> 0) & 1;
		battery_standard.slave_charge_low_temp_prot        = (d[3] >> 1) & 1;
		battery_standard.slave_discharge_high_temp_prot    = (d[3] >> 2) & 1;
		battery_standard.slave_thermal_runaway_prot        = (d[3] >> 3) & 1;
		battery_standard.slave_discharge_low_temp_prot     = (d[3] >> 4) & 1;
		battery_standard.slave_mos_high_temp_prot          = (d[3] >> 5) & 1;
		battery_standard.slave_ambient_high_temp_prot      = (d[3] >> 6) & 1;
	}
	break;
	case 0x530:
	{
		battery_standard.chr_is_not_charging = (d[0] == 0x00);
		battery_standard.chr_is_pseudo_mode = (d[0] == 0x01);
		battery_standard.chr_is_cc_mode = (d[0] == 0x02);
		battery_standard.chr_is_cv_mode = (d[0] == 0x03);
		battery_standard.chr_is_charging_complete = (d[0] == 0x04);

		battery_standard.batt_temperature = d[1];
		battery_standard.charging_status = d[3];
		battery_standard.charging_volt = ((uint16_t)d[4] << 8) | d[5];
		battery_standard.charging_curr = ((uint16_t)d[6] << 8) | d[7];
	}
	break;

	default:
		return false;
		break;
	}
	return true;
}

//decode frame by manufacture and oem data
//and then parse parameters
void process_can_frame_full(uint32_t can_id, const uint8_t data[8], uint32_t t_us){
	uint8_t manuf = (can_id>>24) & 0xFF;
	uint32_t shortid = can_id & 0xFFFFFF;
	uint8_t index = data[0];

	// inside process_can_frame_full:
	if (can_id == 0x1D1 && data != NULL) {
		uint8_t buzzer_status = data[0];
		if (buzzer_status == 0x00) {
			// buzzer off
			buzzer_on = 0;
		} else if (buzzer_status == 0x01) {
			// buzzer on (BMS will auto-off after ~5 minutes)
			buzzer_on = 1;
		} else {
			// unknown value: ignore or log
		}
		// return or continue: depends if this ID is unique and not part of OEM table
		return;
	}

	if(parse_std_ids(can_id,data))
	{
		return;
	}


	int oem = find_oem_slot(manuf);
	if (oem < 0) return;


	uint64_t unique40 = parse_unique40(data);
	int ent = -1;
	if (unique40) ent = find_entry_by_unique(oem, unique40);
	if (ent < 0) ent = find_entry_by_short(oem, shortid);
	if (ent < 0) ent = create_entry(oem, manuf, shortid, unique40);
	if (ent < 0) return; // no space


	bms_entry_t *e = &g_bms_table[oem][ent];
	e->last_rx_tick = HAL_GetTick();
	if (index < MAX_INDEXES) e->last_index_ts[index] = HAL_GetTick();


	switch (index){
	case 0x00:
	{
		if (unique40) e->unique_id40 = unique40;
	}
	break;

	case 0x01:
	{
		parse_index1(e, data);
	}
	break;
	case 0x02:
	{
		parse_index2(e, data);
	}
	break;
	case 0x03:
	{
		parse_index3(e, data);
	}
	break;
	case 0x04:
	{
		parse_index4(e, data);
	}
	break;
	case 0x05:
	{
		parse_index5(e, data);
	}
	break;
	case 0x06:
	{
		parse_index6(e, data);
	}
	break;
	case 0x07:
	{
		parse_index7(e, data);
	}
	break;
	case 0x08:
	{
		parse_index8(e, data);
	}
	break;
	case 0x09:
	{
		parse_index9(e, data);
	}
	break;
	case 0x0A:
	{
		parse_index10(e, data);
	}
	break;
	case 0x0B:
	{
		parse_index11(e, data);
	}
	break;
	case 0x0C:
	{
		parse_index12(e, data);
	}
	break;
	case 0x0D:
	{
		parse_index13(e, data);
	}
	break;
	case 0x0E:
	{
		parse_index14(e, data);
	}
	break;
	default:
		break;
	}
}

//main looplike function for handling frames
void process_can_frames(void){
	can_frame_t f;

	if(charger_brd[0] == 0x01 && adaptive_charging == true)
	{
		send_heartbeat = true;
	}
	if(send_heartbeat)
	{
		static uint8_t count = 0;
		static uint64_t t = 0;
		if(t == 0) t = micros64();
		uint8_t data[8] = {0x01,0x02,0x00,0x00,0x00,0x00,0x00,0x00};
		if(micros64() - t >= 1000000ULL)
		{
			send_bms_response(0x1E3,data,8);
			t = micros64();
			count++;
			if(count == 30)
			{
				count = 0;
				send_heartbeat = false;
			}
		}
	}

	if (canq_pop(&f)==0){
		process_can_frame_full(f.id, f.data, f.timestamp_us);

	}
	if (fet_pending) {
		uint32_t now = HAL_GetTick();

		// enforce a small retry interval if previous try failed
		if (fet_last_tx_try_ms == 0 || (now - fet_last_tx_try_ms) >= FET_RETRY_INTERVAL_MS) {

			int tx_rc = can_send_fet_control(0x00ABCDE, fet_desired_dsg, fet_desired_chg);
			fet_last_tx_try_ms = now;

			if (tx_rc == 0) {
				// queued successfully — treat as done
				fet_pending = 0;
				fet_retry_count = 0;
			} else {
				// mailbox busy / failed to queue
				fet_retry_count++;
				if (fet_retry_count >= FET_MAX_TX_RETRIES) {
					fet_pending = 0;

				}
			}
		}
	}
}
