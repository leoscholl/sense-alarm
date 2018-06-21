#include <pebble_worker.h>
#include "background.h"
#include "accel.h"

#define ALARM_HOUR_KEY 0
#define ALARM_MINUTE_KEY 1

#define RECORDING_BEFORE_WAKEUP_WINDOW_SECONDS 6 * SECONDS_PER_HOUR
#define WAKEUP_WINDOW_SECONDS 30 * SECONDS_PER_MINUTE

#define SENDER_WORKER 0
#define SENDER_APP 1

time_t alarm_time;
bool accel_is_on = false;

void load_alarm_time(void) {
  
  // Lookup the alarm time
  int hour;
  int minute;
  if (persist_exists(ALARM_HOUR_KEY) && persist_exists(ALARM_MINUTE_KEY)) {
    hour = persist_read_int(ALARM_HOUR_KEY);
    minute = persist_read_int(ALARM_MINUTE_KEY);
  } else {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "No alarm set, error");
    return;
  }
  alarm_time = time_start_of_today() + (hour*SECONDS_PER_HOUR) + (minute*SECONDS_PER_MINUTE);
  
  // Next occuring alarm time
  while (alarm_time < time(NULL))
    alarm_time = alarm_time + SECONDS_PER_DAY;
}

static void trigger_alarm(void) {
  AppWorkerMessage message = {
    .data0 = true
  };
  app_worker_send_message(SENDER_WORKER, &message); // if app is open already
  worker_launch_app(); // if app is closed
  alarm_time += SECONDS_PER_DAY;
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  
  // Check alarm time for trigger regardless of recording status
  if (mktime(tick_time) >= alarm_time) { 
    trigger_alarm();
    if (accel_is_on) {
      deinit_accel();
      accel_is_on = false;
      return;
    }
  }
  
  // Trigger the alarm if we're in the wakeup window and the datastore 
  // is currently in a local maxmimum
  if (accel_is_on && 
      mktime(tick_time) >= alarm_time - WAKEUP_WINDOW_SECONDS && 
      is_local_max()) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Alarm triggered");
    deinit_accel();
    accel_is_on = false;
    trigger_alarm();
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting a new alarm for %u hours from now", 
            (unsigned int)(alarm_time - time(NULL))/SECONDS_PER_HOUR);
  }
  
  // If the alarm time was changed recently, may need to turn off accelerometer
  if (accel_is_on && 
      mktime(tick_time) < alarm_time - WAKEUP_WINDOW_SECONDS -
                          RECORDING_BEFORE_WAKEUP_WINDOW_SECONDS) {
    deinit_accel();
    accel_is_on = false;
    return;
  }
  
  // Turn on acceleration logging if needed
  if (!accel_is_on && 
      mktime(tick_time) >= alarm_time - WAKEUP_WINDOW_SECONDS -
                           RECORDING_BEFORE_WAKEUP_WINDOW_SECONDS) {
    init_accel();
    accel_is_on = true;
  }
}

// Handle when the app sends a message (update the alarm time)
static void worker_message_handler(uint16_t type, AppWorkerMessage *message) {
  if (type == SENDER_APP)
    load_alarm_time();
}

void background_init(void) {
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Background process started");
  load_alarm_time();
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  // Subscribe to worker messages
  app_worker_message_subscribe(worker_message_handler);
}

void background_deinit(void) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Background process stopped");
  if (accel_is_on) {
    deinit_accel();
    accel_is_on = false;
  }
}

int main(void) {
  background_init();
  worker_event_loop();
  background_deinit();
}