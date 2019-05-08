#include <pebble_worker.h>
#include "math.h"
#include "accel.h"
#include "datastore.h"

#define SAMPLE_RATE           10
#define SAMPLES_PER_BATCH     25
#define SECONDS_PER_EPOCH     SECONDS_PER_MINUTE
#define SECONDS_PER_BIN       10 * SECONDS_PER_MINUTE
#define SECONDS_IN_BUFFER     2 * SECONDS_PER_HOUR

#define SAMPLES_PER_EPOCH     SECONDS_PER_EPOCH * SAMPLE_RATE
#define EPOCHS_PER_BIN        SECONDS_PER_BIN / SECONDS_PER_EPOCH
#define EPOCHS_IN_BUFFER      SECONDS_IN_BUFFER / SECONDS_PER_EPOCH

#define DEBUG 0

uint16_t count = 0;
uint16_t samples_counted = 0;
static circular_buffer buf;
#if DEBUG
static DataLoggingSessionRef s_session_ref;
static DataLoggingSessionRef l_session_ref;
#endif

// Count zero-crossings in accelerometer data batches
static void accel_data_handler(AccelData *data, uint32_t num_samples) {

  // Average the data
  int16_t x, y, z, x_prev = 0, y_prev = 0, z_prev = 0;
  float l;
  AccelData *dx = data;
  for (uint32_t i = 0; i < num_samples; i++, dx++) {
    // If vibe went off then discount everything
    if (dx->did_vibrate) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Vibrate invalidated datapoint");
      continue;
    }
    samples_counted++;
    x = dx->x;
    y = dx->y;
    z = dx->z;
    l = sm_sqrt(x*x + y*y + z*z);
    x -= (x/l)*1000;
    y -= (y/l)*1000;
    z -= (z/l)*1000;
    if ((x > 0 && x_prev < 0) || (x < 0 && x_prev > 0) ||
        (y > 0 && y_prev < 0) || (y < 0 && y_prev > 0) ||
        (z > 0 && z_prev < 0) || (z < 0 && z_prev > 0)) {
      count++;
    }
    x_prev = x;
    y_prev = y;
    z_prev = z;
  }
  
  // Append the datastore if necessary
  if (samples_counted >= SAMPLES_PER_EPOCH) {
    count = count > 255 ? 255 : count;
    cb_push_back(&buf, &count);
#if DEBUG  
    data_logging_log(s_session_ref, &count, 1);
#endif
    count = 0;
    samples_counted = 0;
  }
}

uint16_t mean(void) {
  uint32_t sum = 0;
  uint32_t n = cb_size(&buf);
  for (uint32_t i=0; i<n; i++) {
    sum += *(uint8_t*)cb_peek(&buf, i);
  }
  return (uint16_t)(sum / n);
}

// // Check whether accel data is at local maximum
bool is_local_max(void) {

  unsigned int num_buffer = cb_size(&buf);
  unsigned int num_bins = EPOCHS_IN_BUFFER / EPOCHS_PER_BIN;
    
  if (num_buffer < EPOCHS_IN_BUFFER) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Number of samples requested is too high");
    return false;
  }
  if (num_bins < 3) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Number of samples requested is too low");
    return false;
  } 
  
  // Mean over whole datastore
  uint16_t avg = mean() * EPOCHS_PER_BIN;
  
  // Calculate spike rates
  uint16_t spikes_bins[3] = {0};
  for (unsigned int i=0; i<3*EPOCHS_PER_BIN; i++) {
    spikes_bins[i/EPOCHS_PER_BIN] += *(uint8_t*)cb_peek(&buf, i);
  }

#if DEBUG
  data_logging_log(l_session_ref, &spikes_bins, 1);
  data_logging_log(l_session_ref, &avg, 1);
  data_logging_log(l_session_ref, &num_buffer, 1);
#endif

  // Middle bin should be above threshold
  if (spikes_bins[1] <= avg)
    return false;
  
  // Slope must be negative
  if (spikes_bins[0] > spikes_bins[1])
    return false;
  
  // Previous slope must be positive
  if (spikes_bins[1] < spikes_bins[2])
    return false;

  return true;
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

#if DEBUG  
  // Set up the datalogs
  l_session_ref = data_logging_create(2, DATA_LOGGING_UINT, sizeof(uint16_t), false);
  s_session_ref = data_logging_create(1, DATA_LOGGING_UINT, sizeof(uint8_t), false);
  uint32_t flag = 0;
  data_logging_log(s_session_ref, &flag, 4);
#endif
  
  // Set up the circular buffer datastore
  cb_init(&buf, EPOCHS_IN_BUFFER, sizeof(uint8_t));
}

// De-initialize if needed
void deinit_accel(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Acceleration logging OFF");
  accel_data_service_unsubscribe();
  cb_free(&buf);
#if DEBUG
  data_logging_finish(l_session_ref);
  uint32_t flag = 0;
  data_logging_log(s_session_ref, &flag, 4);
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  data_logging_log(s_session_ref, &t->tm_hour, 1);
  data_logging_log(s_session_ref, &t->tm_min, 1);
  data_logging_finish(s_session_ref);
#endif
}
