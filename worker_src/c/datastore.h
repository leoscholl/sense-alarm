#pragma once
#include <pebble_worker.h>

void store_sample(uint16_t data);
bool ds_is_local_max(size_t num_samples);
void init_datastore(size_t capacity);
void deinit_datastore(void);