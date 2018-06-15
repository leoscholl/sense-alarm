#pragma once

void store_sample(uint16_t data);
uint16_t *ds_contents(void);
bool ds_is_local_max(unsigned int num_samples);
void init_datastore(size_t capacity);
void deinit_datastore(void);