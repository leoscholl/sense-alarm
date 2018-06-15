#include <pebble_worker.h>
#include "datastore.h"

void test1(void) {
  init_datastore(10);
  
  
  for (uint16_t i=0; i<31; i++) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Store %u", i+1);
    store_sample(i+1);
  
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Read contents:");
    uint16_t *contents = ds_contents();
    for (uint32_t j = 0; i < 10 ? j <= (uint32_t)i : j < 10; j++) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "%u", contents[j]);
    }
    free(contents);
  }
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, ds_is_local_max(10) ? "Local max" : "Not local max");
  deinit_datastore();
}
