#include <pebble.h>
#include "alarm.h"

#define ALARM_REPEAT              6
#define ALARM_LIMIT               (ARRAY_LENGTH(alarm_pattern) * ALARM_REPEAT)
#define SNOOZE_DURATION_SECONDS   9 * SECONDS_PER_MINUTE
#define VIBE_DUR_MS               PBL_IF_ROUND_ELSE(100, 50)

// Times (in seconds) between each vibe (gives a progressive alarm and gaps between phases)
static uint8_t alarm_pattern[] = { 5, 4, 4, 3, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 1, 30 };

// Pulse widths (in milliseconds) for each single vibe
static uint32_t const single_segments[] = { VIBE_DUR_MS };
static uint32_t const double_segments[] = { VIBE_DUR_MS, 100, VIBE_DUR_MS };
static uint32_t const triple_segments[] = { VIBE_DUR_MS*2, 100, VIBE_DUR_MS*2, 100, VIBE_DUR_MS*2};
VibePattern single_pat = {
  .durations = single_segments,
  .num_segments = ARRAY_LENGTH(single_segments),
};
VibePattern double_pat = {
  .durations = double_segments,
  .num_segments = ARRAY_LENGTH(double_segments),
};
VibePattern triple_pat = {
  .durations = triple_segments,
  .num_segments = ARRAY_LENGTH(triple_segments),
};
VibePattern *vibe_pattern[3] = {&single_pat, &double_pat, &triple_pat};

static AppTimer *alarm_timer = NULL;
uint8_t alarm_count = ALARM_LIMIT;

Window *s_window;
static StatusBarLayer *s_status_bar;
static ActionBarLayer *s_action_bar;
static ActionMenuLevel *s_root_level;
static TextLayer *s_label_layer;
static GBitmap *s_ellipsis_bitmap;

// Alarm timer loop
void do_alarm(void *data) {
  
  if (data == NULL)
    alarm_count = 0;
  else
    alarm_count = *(uint8_t *)data;
  
  if (alarm_count == 0)
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Alarm fired, starting vibes");
  else
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Alarm count = %u", (unsigned int)alarm_count);

  // Already hit the limit, give up
  if (alarm_count >= ALARM_LIMIT)
    return;
  
  // Vibrate
  VibePattern pat = *vibe_pattern[(alarm_count / ARRAY_LENGTH(alarm_pattern)) % ARRAY_LENGTH(vibe_pattern)];
  vibes_enqueue_custom_pattern(pat);
  
  // Prepare a timer for the next buzz
  uint8_t delay = alarm_pattern[alarm_count % (ARRAY_LENGTH(alarm_pattern))];
  alarm_count++;
  alarm_timer = app_timer_register(((uint16_t)delay) * 1000, do_alarm, &alarm_count);
}

static void do_snooze(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Alarm snoozed");
  WakeupId id = wakeup_schedule(time(NULL) + SNOOZE_DURATION_SECONDS, 0, true);
  if (id < 0)
    APP_LOG(APP_LOG_LEVEL_ERROR, "Snooze failed");
}

// Callback for button presses on the ActionMenu popup
static void action_performed_callback(ActionMenu *action_menu, const ActionMenuItem *action, void *context) {
    
  // Cancel the current alarm, then create a wakeup event if snooze was selected
  app_timer_cancel(alarm_timer);
  alarm_timer = NULL;
  bool snooze = (bool)action_menu_item_get_action_data(action);
  if (snooze) {
    do_snooze();
  } else {
    vibes_double_pulse();
  }
  
  // We won't need the alarm ui any more
  action_menu_close(action_menu, true);
  window_stack_remove(s_window, false);
}

// Callback for select button presses in the main window
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Configure the ActionMenu Window about to be shown
  ActionMenuConfig config = (ActionMenuConfig) {
    .root_level = s_root_level,
    .colors = {
      .background = GColorBlack,
      .foreground = GColorWhite,
    },
    .align = ActionMenuAlignCenter
  };
  
  // Open the action menu window
  action_menu_open(&config);
}

// Callback for up or down presses
static void other_click_handler(ClickRecognizerRef recognizer, void *context) {
  do_snooze();
  window_stack_remove(s_window, true);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, other_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, other_click_handler);
}

// Unload the window
void alarm_window_unload(Window *window) {
  text_layer_destroy(s_label_layer);
  action_bar_layer_destroy(s_action_bar);
  status_bar_layer_destroy(s_status_bar);
  gbitmap_destroy(s_ellipsis_bitmap);
  action_menu_hierarchy_destroy(s_root_level, NULL, NULL);
  
  // If there is an ongoing alarm and the window is closed, snooze the alarm
  if (alarm_timer != NULL)
    do_snooze();
}

// Load the window
void alarm_window_load(Window *window) {

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  s_ellipsis_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ELLIPSIS);

  // Action bar for selecting whether to stop or snooze
  s_action_bar = action_bar_layer_create();
  action_bar_layer_set_click_config_provider(s_action_bar, click_config_provider);
  action_bar_layer_set_icon(s_action_bar, BUTTON_ID_SELECT, s_ellipsis_bitmap);
  action_bar_layer_add_to_window(s_action_bar, window);
  
  int16_t width = bounds.size.w - ACTION_BAR_WIDTH;

  // Status bar shows the time at the top of the main alarm window
  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, GColorClear, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  status_bar_layer_set_separator_mode(s_status_bar, 
                                      StatusBarLayerSeparatorModeDotted);
  GRect frame = GRect(0, 0, width, STATUS_BAR_LAYER_HEIGHT);
  layer_set_frame(status_bar_layer_get_layer(s_status_bar), frame);  
  layer_add_child(window_layer, status_bar_layer_get_layer(s_status_bar));


  // Text layer to display a message about the alarm
  s_label_layer = text_layer_create(GRect(bounds.origin.x, bounds.size.h/4, 
                                          width, bounds.size.h/2));
  text_layer_set_text(s_label_layer, "Alarm is going off! Time to wake up!!");
  text_layer_set_font(s_label_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_color(s_label_layer, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  text_layer_set_background_color(s_label_layer, GColorClear);
  text_layer_set_text_alignment(s_label_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_label_layer));

#if defined(PBL_ROUND)
  text_layer_enable_screen_text_flow_and_paging(s_label_layer, 3);
#endif

  // Set up the actions menu
  s_root_level = action_menu_level_create(2);
  action_menu_level_add_action(s_root_level, "Stop", action_performed_callback, (bool *)false);
  action_menu_level_add_action(s_root_level, "Snooze", action_performed_callback, (bool *)true);
  
}

void alarm_init(void) {
  s_window = window_create();
  window_set_background_color(s_window, PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorWhite));
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = alarm_window_load,
    .unload = alarm_window_unload,
  });
  window_stack_push(s_window, true);
}

void alarm_deinit(void) {    
  window_destroy(s_window);
}


