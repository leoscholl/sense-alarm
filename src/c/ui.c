#include <pebble.h>
#include "ui.h"
#include "settings.h"

#define TIME_CHANGE_RESOLUTION 5
#define TIME_CHANGE_REPEAT_DURATION 50

Window *s_main_window;
TextLayer *time_layer;
TextLayer *text_layer;
StatusBarLayer *status_bar;

// Change the time displayed on the screen
static void display_time(uint32_t hour, uint32_t minute) {
  
  static char buffer[20];
  if (clock_is_24h_style())
    snprintf(buffer, sizeof(buffer), "Alarm set: %02u:%02u", (unsigned int)hour, (unsigned int)minute);
  else
    snprintf(buffer, sizeof(buffer), "Alarm set: %u:%02u%s", hour % 12 ? (unsigned int)hour % 12 : 12, 
             (unsigned int)minute, hour > 12 ? "pm" : "am");
  text_layer_set_text(time_layer, buffer);
  layer_set_hidden(text_layer_get_layer(time_layer), false);
  layer_set_hidden(text_layer_get_layer(text_layer), true);
}

// Display message instead of time
static void display_msg(char *msg) {
  text_layer_set_text(text_layer, msg);
  layer_set_hidden(text_layer_get_layer(time_layer), true);
  layer_set_hidden(text_layer_get_layer(text_layer), false);
}

// Display either the time or the default message depending on state (alarm ON or OFF)
static void display_default(bool state) {
  if (state) {
    uint32_t hour;
    uint32_t minute;
    get_alarm_time(&hour, &minute);
    display_time(hour, minute);
  } else {
    display_msg("No alarm set");
  }
}

// Up clicks increase the alarm time
static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!get_alarm_state())
    return;
  uint32_t hour;
  uint32_t minute;
  change_alarm_time(&hour, &minute, TIME_CHANGE_RESOLUTION);
  display_time(hour, minute);
}

// Down clicks decrease the time
static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!get_alarm_state())
    return;
  uint32_t hour;
  uint32_t minute;
  change_alarm_time(&hour, &minute, -TIME_CHANGE_RESOLUTION);
  display_time(hour, minute);
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
  GRect text_bounds = GRect(bounds.origin.x+10, bounds.size.h/3, bounds.size.w-20, bounds.size.h/3);
  
  // Create text layer
  text_layer = text_layer_create(text_bounds);
  text_layer_set_background_color(text_layer, GColorClear);
  text_layer_set_text_color(text_layer, GColorWhite);
  text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
  
  // Create time layer
  time_layer = text_layer_create(text_bounds);
  text_layer_set_background_color(time_layer, GColorClear);
  text_layer_set_text_color(time_layer, GColorWhite);
  text_layer_set_font(time_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  text_layer_set_overflow_mode(time_layer, GTextOverflowModeWordWrap);
  text_layer_set_text_alignment(time_layer, GTextAlignmentCenter);
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