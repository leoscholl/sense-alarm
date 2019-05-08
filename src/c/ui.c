#include <pebble.h>
#include "ui.h"
#include "settings.h"

#define TIME_CHANGE_RESOLUTION        5
#define TIME_CHANGE_REPEAT_DURATION   50

Window *s_main_window;
TextLayer *time_layer;
TextLayer *text_layer;
StatusBarLayer *status_bar;

// Change the time displayed on the screen
static void update_time(void) {
  uint32_t hour;
  uint32_t minute;
  get_alarm_time(&hour, &minute);

  static char buffer[11];
  if (clock_is_24h_style())
    snprintf(buffer, sizeof(buffer), "%02u :%02u", (unsigned int)hour, (unsigned int)minute);
  else
    snprintf(buffer, sizeof(buffer), "%3u :%02u", hour % 12 ? (unsigned int)hour % 12 : 12, (unsigned int)minute);
  text_layer_set_text(time_layer, buffer);
}

// Display either the time or the default message depending on state (alarm ON or OFF)
static void display_default(bool state) {
  if (state) {
    update_time();
    text_layer_set_text(text_layer, "Alarm set:");
    layer_set_hidden(text_layer_get_layer(time_layer), false);
  } else {
    text_layer_set_text(text_layer, "No alarm set");
    layer_set_hidden(text_layer_get_layer(time_layer), true);
  }
}

// Up clicks increase the alarm time
static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!get_alarm_state())
    return;
  uint32_t hour;
  uint32_t minute;
  change_alarm_time(&hour, &minute, TIME_CHANGE_RESOLUTION);
  update_time();
}

// Down clicks decrease the time
static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!get_alarm_state())
    return;
  uint32_t hour;
  uint32_t minute;
  change_alarm_time(&hour, &minute, -TIME_CHANGE_RESOLUTION);
  update_time();
}

// Select clicks turns the alarm on or off
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  bool state = !get_alarm_state();
  set_alarm_state(state);
  display_default(state);
}

// Subcribe to button click events
static void click_config_provider(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, TIME_CHANGE_REPEAT_DURATION,
                                          up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, TIME_CHANGE_REPEAT_DURATION,
                                          down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

// Load the main display
static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);

  // Create status bar
  status_bar = status_bar_layer_create();
  status_bar_layer_set_separator_mode(status_bar, StatusBarLayerSeparatorModeDotted);
  status_bar_layer_set_colors(status_bar, GColorClear, GColorWhite);
  layer_add_child(window_layer, status_bar_layer_get_layer(status_bar));
  
  // Calculate remaining space
  GRect bounds = layer_get_unobstructed_bounds(window_layer);
  
  // Create text layer
  GRect text_bounds = GRect(bounds.origin.x+10, bounds.origin.y+10, bounds.size.w-20, bounds.size.h-20);
  text_layer = text_layer_create(text_bounds);
  text_layer_set_background_color(text_layer, GColorClear);
  text_layer_set_text_color(text_layer, GColorWhite);
  text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_DROID_SERIF_28_BOLD));
  text_layer_set_overflow_mode(text_layer, GTextOverflowModeWordWrap);
  text_layer_set_text_alignment(text_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
  
  // Create time layer
  GRect time_bounds = GRect(bounds.origin.x+10, bounds.size.h/2-20, bounds.size.w-20, bounds.size.h/2+20);
  time_layer = text_layer_create(time_bounds);
  text_layer_set_background_color(time_layer, GColorClear);
  text_layer_set_text_color(time_layer, GColorWhite);
  text_layer_set_font(time_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  text_layer_set_overflow_mode(time_layer, GTextOverflowModeWordWrap);
  text_layer_set_text_alignment(time_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(time_layer));
}

// Unload the main display
static void main_window_unload(Window *window) {
  text_layer_destroy(text_layer);
  text_layer_destroy(time_layer);
  status_bar_layer_destroy(status_bar);
}

void ui_init(void) {
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Opening UI");
  
  // Create main Window element and click provider
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
  window_set_click_config_provider(s_main_window, click_config_provider);
  
  // Initialize display
  bool state = get_alarm_state();
  display_default(state);
}

void ui_deinit(void) {
  window_destroy(s_main_window);
}
