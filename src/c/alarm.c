#include <pebble.h>
#include "alarm.h"

#define ALARM_REPEAT 5
#define ALARM_LIMIT (ARRAY_LENGTH(alarm_pattern) * ALARM_REPEAT)

// Times (in seconds) between each vibe (gives a progressive alarm and gaps between phases)
static uint8_t alarm_pattern[] = { 5, 4, 4, 3, 3, 3, 2, 2, 2, 2, 30 };

// Pulse widths (in milliseconds) for each single vibe
static uint32_t const single_segments[] = { 50 };
static uint32_t const double_segments[] = { 50, 100, 50 };
VibePattern single_pat = {
  .durations = single_segments,
  .num_segments = ARRAY_LENGTH(single_segments),
};
VibePattern double_pat = {
  .durations = double_segments,
  .num_segments = ARRAY_LENGTH(double_segments),
};

static uint8_t alarm_count = ALARM_LIMIT;
static AppTimer *alarm_timer = NULL;

static ActionMenu *s_action_menu;
static ActionMenuLevel *s_root_level;

// Alarm timer loop
void do_alarm(void *data) {
  
  alarm_timer = NULL;
  
  // New alarm, open the action menu popup and reset the counter
  if ((bool)data) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Alarm received by app, triggering action menu and vibes");
    alarm_count = 0;
    alarm_window_load();
    ActionMenuConfig config = (ActionMenuConfig) {
      .root_level = s_root_level,
      .colors = {
        .background = GColorClear,
        .foreground = GColorWhite,
      },
      .align = ActionMenuAlignCenter
    };
    s_action_menu = action_menu_open(&config);
  }

  // Already hit the limit, give up
  if (alarm_count >= ALARM_LIMIT)
    return;
  
  // Vibrate
  if (alarm_count < ARRAY_LENGTH(alarm_pattern))
    vibes_enqueue_custom_pattern(single_pat);
  else 
    vibes_enqueue_custom_pattern(double_pat);
  
  // Prepare a timer for the next buzz
  alarm_timer = app_timer_register(((uint16_t) alarm_pattern[alarm_count % (ARRAY_LENGTH(alarm_pattern))]) * 1000, 
                                   do_alarm, (bool *)false);
  alarm_count++;
}

// Callback for button presses on the ActionMenu popup
static void action_performed_callback(ActionMenu *action_menu, const ActionMenuItem *action, void *context) {
    
  // An action was selected, determine which one
  bool snooze = (bool)action_menu_item_get_action_data(action);
  
  if (alarm_timer != NULL) {
    app_timer_cancel(alarm_timer);
    alarm_timer = NULL;
  }
  
  if (snooze) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Alarm snoozed");
    alarm_timer = app_timer_register(9 * SECONDS_PER_MINUTE * 1000, do_alarm, (bool *)true);
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "Alarm canceled");
    alarm_count = ALARM_LIMIT;
  }
  
  // We won't need the ActionMenu again so unload it
  alarm_window_unload();
}

// Initialize the ActionMenu popup
void alarm_window_load(void) {
  
  // Create the root level
  s_root_level = action_menu_level_create(2);

  // Set up the actions for this level, using action context to pass colors
  action_menu_level_add_action(s_root_level, "Stop", action_performed_callback, (bool *)false);
  action_menu_level_add_action(s_root_level, "Snooze", action_performed_callback, (bool *)true);
}

// Destroy the popup
void alarm_window_unload(void) {
  action_menu_hierarchy_destroy(s_root_level, NULL, NULL);
}

