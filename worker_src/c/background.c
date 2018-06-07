#include <pebble_worker.h>
#include "background.h"
#include "accel.h"

#define ALARM_HOUR_KEY 0
#define ALARM_MINUTE_KEY 1

#define WAKEUP_WINDOW_SECONDS 30 * SECONDS_PER_MINUTE

#define SENDER_WORKER 0
#define SENDER_APP 1

time_t alarm_time;
bool listening = false;
AppWorkerMessage message = {
  .data0 = true
};

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
  
  // Next occuring alarm time excluding the current wakeup window
  while (alarm_time < time(NULL))
    alarm_time = alarm_time + SECONDS_PER_DAY;
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  if (listening) { // Accelerometer data is being logged
          
    // If the alarm time was changed recently, may need to turn off accelerometer
    if (mktime(tick_time) < alarm_time - WAKEUP_WINDOW_SECONDS) {
      deinit_accel();
      listening = false;
    }
    
    // Trigger the alarm if either wakeup condition is met
    if (is_local_max() || mktime(tick_time) >= alarm_time) {
      APP_LOG(APP_LOG_LEVEL_INFO, "Alarm triggered");
      deinit_accel();
      listening = false;
      app_worker_send_message(SENDER_WORKER, &message); // if app is open already
      worker_launch_app(); // if app is closed
      alarm_time += SECONDS_PER_DAY;
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting a new alarm for %u hours from now", 
              (unsigned int)(alarm_time - time(NULL))/SECONDS_PER_HOUR);
    }
  } else { // Accelerometer data is not being logged
    if (mktime(tick_time) >= alarm_time - WAKEUP_WINDOW_SECONDS) {
      
      // Start listening to accelerometer data
      init_accel();
      listening = true;
    }
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
  if (listening) {
    deinit_accel();
    listening = false;
  }
}

int main(void) {
  background_init();
  worker_event_loop();
  background_deinit();
}