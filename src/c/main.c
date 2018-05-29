#include <pebble.h>
#include "alarm.h"
#include "settings.h"
#include "ui.h"

#define WAKEUP_HOUR 8
#define WAKEUP_MINUTE 15

#define SENDER_WORKER 0
#define SENDER_APP 1

// Close the worker and start the alarm sequence
void alarm_trigger(void) {
  do_alarm((void *)true);
}

// Handle when the worker sends a message (alarm trigger)
static void worker_message_handler(uint16_t type, AppWorkerMessage *message) {
  if (type == SENDER_WORKER)
    alarm_trigger();
}

void handle_init(void) {
  
  // Trigger the alarm immediately if the worker woke us up
  if (launch_reason() == APP_LAUNCH_WORKER) {
    alarm_trigger();
  } else {
  
    // Save the default alarm time
    if (!alarm_time_exists())
      set_alarm_time(WAKEUP_HOUR, WAKEUP_MINUTE);
    
    // Open the UI
    ui_init();
  
    // Subscribe to worker messages
    app_worker_message_subscribe(worker_message_handler);
  
    // Make sure worker is turned on if alarm is on
    if (get_alarm_state() && !app_worker_is_running())
      app_worker_launch();
  }

}

void handle_deinit(void) {
  if (launch_reason() != APP_LAUNCH_WORKER)
    ui_deinit();
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
