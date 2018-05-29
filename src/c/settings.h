#pragma once

void set_alarm_state(bool state);
bool get_alarm_state(void);
void set_alarm_time(uint32_t hour, uint32_t minute);
bool alarm_time_exists(void);
bool get_alarm_time(uint32_t *hour, uint32_t *minute);
void change_alarm_time(uint32_t *hour, uint32_t *minute, int32_t delta_minutes);
void delete_alarm_time(void);