#pragma once

void alarm_window_load(void);
void alarm_window_unload(void);

void do_alarm(void *data);
void cancel_alarm(ClickRecognizerRef recognizer, void *context);