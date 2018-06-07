#include <pebble.h>
#include "alarm.h"
#include "settings.h"
#include "ui.h"

#define WAKEUP_HOUR 8
#define WAKEUP_MINUTE 15

#define SENDER_WORKER 0
#define SENDER_APP 1

bool did_alarm_init = false;

// Close the worker and start the alarm sequence
static void alarm_trigger(void) {
  if (!did_alarm_init) {
    alarm_init();
    did_alarm_init = true;
  }
  do_alarm(0);
}

// Handle when the worker sends a message (alarm trigger)
static void worker_message_handler(uint16_t type, AppWorkerMessage *message) {
  if (type == SENDER_WORKER)
    alarm_trigger();
}

// Handle when a wakeup is received (alarm trigger)
static void wakeup_handler(WakeupId id, int32_t reason) {
  alarm_trigger();
}

static void handle_init(void) {
  
  // Trigger the alarm immediately if the worker or wakeup woke us up
  if (launch_reason() == APP_LAUNCH_WORKER || 
     launch_reason() == APP_LAUNCH_WAKEUP) {
    alarm_trigger();
  } else {
  
    // Save the default alarm time
    if (!alarm_time_exists())
      set_alarm_time(WAKEUP_HOUR, WAKEUP_MINUTE);
    
    // Open the UI
    ui_init();
  
    // Subscribe to alarm trigger messages
    app_worker_message_subscribe(worker_message_handler);
    wakeup_service_subscribe(wakeup_handler);
  
    // Make sure worker is turned on if alarm is on
    if (get_alarm_state() && !app_worker_is_running())
      app_worker_launch();
  }

}

static void handle_deinit(void) {
  if (did_alarm_init)
    alarm_deinit();
  if (launch_reason() != APP_LAUNCH_WORKER &&
     launch_reason() != APP_LAUNCH_WAKEUP)
    ui_deinit();
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
