#include <pebble_worker.h>
#include "math.h"
#include "accel.h"
#include "datastore.h"

#define BUF_SIZE 300
#define LOCAL_AVG_SIZE 50

bool is_running = false;

// Process accelerometer data
static void accel_data_handler(AccelData *data, uint32_t num_samples) {

  // Average the data
  int16_t x, y, z;
  float l;
  uint32_t avg = 0;
  AccelData *dx = data;
  for (uint32_t i = 0; i < num_samples; i++, dx++) {
    // If vibe went off then discount everything
    if (dx->did_vibrate) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Vibrate invalidated datapoint");
      return;
    }
    x = dx->x;
    y = dx->y;
    z = dx->z;
    l = sm_sqrt(sm_pow(abs(x),2) + sm_pow(abs(y),2) + sm_pow(abs(z),2));
    x -= (x/l)*1000;
    y -= (y/l)*1000;
    z -= (z/l)*1000;
    avg += (uint32_t)sm_sqrt(sm_pow(abs(x),2) + sm_pow(abs(y),2) + sm_pow(abs(z),2));
  }
  
  avg /= num_samples;
  
  // Store the sample at the current time
  store_sample(time(NULL), avg);
}

// Check whether accel data is at local maximum
bool is_local_max(void) {
  return ds_is_local_max(LOCAL_AVG_SIZE);
}

// Initialize
void init_accel(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Acceleration logging ON");
  uint32_t num_samples = 25;  // Number of samples per batch/callback
  accel_data_service_subscribe(num_samples, accel_data_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
  
  // Set up the circular buffer datastore
  init_datastore(BUF_SIZE);
  is_running = true;
}

// De-initialize if needed
void deinit_accel(void) {
  if (is_running) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Acceleration logging OFF");
    accel_data_service_unsubscribe();
    deinit_datastore();
    is_running = false;
  }
}