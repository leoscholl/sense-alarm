#include <pebble_worker.h>
#include "math.h"
#include "accel.h"
#include "datastore.h"

#define SAMPLE_RATE 10
#define SAMPLES_PER_BATCH 25
#define BUF_SIZE 500
#define LOCAL_AVG_SIZE 5.0 * SECONDS_PER_MINUTE / ((double)SAMPLES_PER_BATCH / (double)SAMPLE_RATE)

static DataLoggingSessionRef s_session_ref;

// Process accelerometer data
static void accel_data_handler(AccelData *data, uint32_t num_samples) {

  // Average the data
  int16_t x, y, z;
  float l;
  int32_t sum1 = 0;
  int32_t sum2 = 0;
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
    l = sm_sqrt(x*x + y*y + z*z);
    x -= (x/l)*1000;
    y -= (y/l)*1000;
    z -= (z/l)*1000;
    if (i < num_samples/2)
      sum1 += (int32_t)sm_sqrt(x*x + y*y + z*z);
    else
      sum2 += (int32_t)sm_sqrt(x*x + y*y + z*z);
  }
  
  uint32_t diff = abs(sum2 - sum1);
  
  // Store the sample to the datastore and log to the datalog
  store_sample((uint16_t)diff);
  uint32_t t = (uint32_t)(time(NULL));
  data_logging_log(s_session_ref, &t, 1);
  data_logging_log(s_session_ref, &diff, 1);
}

// Check whether accel data is at local maximum
bool is_local_max(void) {
  return ds_is_local_max(LOCAL_AVG_SIZE);
}

// Initialize
void init_accel(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Acceleration logging ON");
  uint32_t num_samples = SAMPLES_PER_BATCH;  // Number of samples per batch/callback
  accel_data_service_subscribe(num_samples, accel_data_handler);
  switch (SAMPLE_RATE) {
    case 10:
      accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
      break;
    case 25:
      accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);
      break;
    case 50:
      accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);
      break;
    case 100:
      accel_service_set_sampling_rate(ACCEL_SAMPLING_100HZ);
      break;
    default:
      APP_LOG(APP_LOG_LEVEL_ERROR, "Unsupported sampling rate");
      accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);
      break;
  }

  // Set up the datalog
  s_session_ref = data_logging_create(1, DATA_LOGGING_UINT, sizeof(uint32_t), false);
  
  // Set up the circular buffer datastore
  init_datastore(BUF_SIZE);
}

// De-initialize if needed
void deinit_accel(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Acceleration logging OFF");
  accel_data_service_unsubscribe();
  deinit_datastore();
  data_logging_finish(s_session_ref);
}