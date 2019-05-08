#include <pebble.h>
#include "settings.h"

#define ALARM_HOUR_KEY     0
#define ALARM_MINUTE_KEY   1
#define ALARM_ON_KEY       2

#define SENDER_WORKER      0
#define SENDER_APP         1

AppWorkerMessage message = {
  .data0 = true
};

// Save the state (ON or OFF) to persistent storage
void set_alarm_state(bool state) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Alarm turned %s", state ? "ON" : "OFF");
  persist_write_bool(ALARM_ON_KEY, state);
  if (state && !alarm_time_exists()) {
    
    // Set an hour and minute in case there isn't one set already
    uint32_t hour = 0;
    uint32_t minute = 0;
    set_alarm_time(hour, minute);
  }
  
  // Turn on or off the worker
  if (state && !app_worker_is_running()) {
    app_worker_launch();
  } else if (!state) {
    app_worker_kill();
    wakeup_cancel_all();
  }
}

// Read the state (ON or OFF)
bool get_alarm_state(void) {
  if (persist_exists(ALARM_ON_KEY))
    return persist_read_bool(ALARM_ON_KEY);
  else
    return false;
}

// Save the hour and minute to persistent storage
void set_alarm_time(uint32_t hour, uint32_t minute) {
  persist_write_int(ALARM_HOUR_KEY, hour);
  persist_write_int(ALARM_MINUTE_KEY, minute);
  
  // Send message to background worker to update time
  app_worker_send_message(SENDER_APP, &message);
}

// Check whether alarm time exists
bool alarm_time_exists(void) {
  return persist_exists(ALARM_HOUR_KEY) && persist_exists(ALARM_MINUTE_KEY);
}

// Read the hour and minute from storage
bool get_alarm_time(uint32_t *hour, uint32_t *minute) {
  if (!alarm_time_exists())
    return false;
  *hour = persist_read_int(ALARM_HOUR_KEY);
  *minute = persist_read_int(ALARM_MINUTE_KEY);
  return true;
}

// Read the hour and minute but change it by delta_minutes and store the result
void change_alarm_time(uint32_t *hour, uint32_t *minute, int32_t delta_minutes) {
  get_alarm_time(hour, minute);
  int32_t seconds = ((int32_t)(*hour)*SECONDS_PER_HOUR) + 
    ((int32_t)(*minute)*SECONDS_PER_MINUTE) + 
    delta_minutes*SECONDS_PER_MINUTE;
  time_t time = time_start_of_today() + (time_t)seconds;
  struct tm *time_s = localtime(&time);
  *hour = time_s->tm_hour;
  *minute = time_s->tm_min;
  set_alarm_time(*hour, *minute);
}

// Delete the alarm time
void delete_alarm_time(void) {
  persist_delete(ALARM_HOUR_KEY);
  persist_delete(ALARM_MINUTE_KEY);
}
