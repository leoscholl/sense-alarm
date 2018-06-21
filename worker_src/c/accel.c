#include <pebble_worker.h>
#include "math.h"
#include "accel.h"
#include "datastore.h"

#define SAMPLE_RATE 10
#define SAMPLES_PER_BATCH 25
#define LOCAL_AVG_SIZE 30 * SECONDS_PER_MINUTE * SAMPLE_RATE / SAMPLES_PER_BATCH
#define BUF_SIZE 3 * LOCAL_AVG_SIZE

#define DEBUG 1

typedef struct Samples {
  uint8_t data;
} Sample;

static DataLoggingSessionRef s_session_ref;
static circular_buffer buf;


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
  
  uint16_t diff = (uint16_t)abs(sum2 - sum1);
  
  // Store the sample to the datastore and log to the datalog
  Sample s = {
    .data = diff > 255 ? 255 : (uint8_t)diff
  };
  cb_push_back(&buf, &s);
#if DEBUG  
  data_logging_log(s_session_ref, &diff, 1);
#endif
}

uint16_t mean(void) {
  Sample s1;
  uint32_t sum = 0;
  uint32_t n = cb_size(&buf);
  for (uint32_t i=0; i<n; i++) {
    s1 = *(Sample*)cb_peek(&buf, i);
    sum += s1.data;
  }
  return (uint16_t)(sum / n);
}

uint16_t std_dev(void) {
  uint32_t n = cb_size(&buf);
  if (n == 0) return 0;
  Sample s;
  uint32_t sum = 0;
  uint32_t sq_sum = 0;
  for(uint32_t i=0; i<n; i++) {
    s = *(Sample*)cb_peek(&buf, i);
    uint32_t d = s.data;
    sum += d;
    sq_sum += d * d;
  }
  return (uint16_t)sm_sqrt((n * sq_sum - sum * sum) / (n * n));
}

// // Check whether accel data is at local maximum
bool is_local_max(void) {

  unsigned int num_buffer = cb_size(&buf);
  unsigned int num_bins = num_buffer/LOCAL_AVG_SIZE;
    
  if (LOCAL_AVG_SIZE*2 >= num_buffer) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Number of samples requested is too high");
    return false;
  }
  if (num_bins < 3) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Number of samples requested is too low");
    return false;
  } 
  
  // Mean and standard deviation over whole datastore
  uint16_t avg = mean();
  uint16_t std = std_dev();
  uint16_t thr = avg + std;
  
  // Calculate spike rates
  Sample s;
  uint16_t spikes_global = 0;
  uint16_t spikes_bins[3] = {0};
  for (unsigned int i=0; i<num_buffer; i++) {
    s = *(Sample*)cb_peek(&buf, i);
    if (s.data > thr) {
      spikes_global++;
      if (i*num_bins/num_buffer < 3) spikes_bins[i*num_bins/num_buffer]++;
    }
  }
  for (unsigned int i=0; i<3; i++) {
    spikes_bins[i] *= num_bins;
  }

  // No need to continue if local is lower global
  if (spikes_bins[0] <= spikes_global)
    return false;
  
  // Slope must be negative
  if (spikes_bins[0] < spikes_bins[1])
    return false;
  
  // Previous slope must be positive
  if (spikes_bins[1] > spikes_bins[2])
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
  // Set up the datalog
  s_session_ref = data_logging_create(1, DATA_LOGGING_UINT, sizeof(uint16_t), false);
  uint64_t flag = 0;
  data_logging_log(s_session_ref, &flag, 4);
#endif
  
  // Set up the circular buffer datastore
  cb_init(&buf, BUF_SIZE, sizeof(Sample));
}

// De-initialize if needed
void deinit_accel(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Acceleration logging OFF");
  accel_data_service_unsubscribe();
  cb_free(&buf);
#if DEBUG
  uint64_t flag = 0;
  data_logging_log(s_session_ref, &flag, 4);
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  data_logging_log(s_session_ref, &t->tm_hour, 1);
  data_logging_log(s_session_ref, &t->tm_min, 1);
  data_logging_finish(s_session_ref);
#endif
}