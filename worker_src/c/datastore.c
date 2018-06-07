#include <pebble_worker.h>
#include "datastore.h"

#define PEAK_DERIV 0.1
#define MOVING_AVG_SAMPLES 10

typedef struct Samples {
  uint16_t data;
} Sample;

typedef struct circular_buffer
{
    void *buffer;     // data buffer
    void *buffer_end; // end of data buffer
    size_t capacity;  // maximum number of items in the buffer
    size_t count;     // number of items in the buffer
    size_t sz;        // size of each item in the buffer
    void *head;       // pointer to head
} circular_buffer;

static circular_buffer buf;

// Create a new circular buffer
void cb_init(circular_buffer *cb, size_t capacity, size_t sz)
{
  cb->buffer = malloc(capacity * sz);
  if(cb->buffer == NULL)
    APP_LOG(APP_LOG_LEVEL_ERROR, "Memory allocation failed");
  cb->buffer_end = (char *)cb->buffer + capacity * sz;
  cb->capacity = capacity;
  cb->count = 0;
  cb->sz = sz;
  cb->head = cb->buffer;
}

// Release memory for the buffer array
void cb_free(circular_buffer *cb)
{
  free(cb->buffer);
  cb->buffer = NULL;
  cb->buffer_end = NULL;
  cb->head = NULL;
  cb->count = 0;
  cb->capacity = 0;
}

// Add a new item to the buffer
void cb_push_back(circular_buffer *cb, const void *item)
{
  memcpy(cb->head, item, cb->sz);
  cb->head = (char*)cb->head + cb->sz;
  if(cb->head == cb->buffer_end)
    cb->head = cb->buffer;
  if (cb->count < cb->capacity)
    cb->count++;
}

// Check how many items are present
size_t cb_size(circular_buffer *cb)
{
  return cb->count;
}

// Copy an item from the buffer
void* cb_peek(circular_buffer *cb, size_t index)
{
  if (cb->count <= index) {
    // handle error
    APP_LOG(APP_LOG_LEVEL_ERROR, "Peek into invalid index");
    return NULL;
  }
  
  size_t count = 0;
  char *loc = cb->head - cb->sz;
  if (loc < (char *)cb->buffer)
    loc = cb->buffer_end - cb->sz;
  while (count < index) {
    loc = loc - cb->sz;
    if (loc < (char *)cb->buffer)
      loc = cb->buffer_end - cb->sz;
    count++;
  }
  return loc;
}

// Store a data sample into the buffer
void store_sample(uint16_t data) {

  Sample s;

  // Moving average
  if (MOVING_AVG_SAMPLES > 0 && cb_size(&buf) > 0) {
    Sample s_prev;
    s_prev = *(Sample*)cb_peek(&buf, 0);
    s.data = (data + (MOVING_AVG_SAMPLES-1)*s_prev.data)/MOVING_AVG_SAMPLES;
  }
  else {
    s.data = data;
  }
  cb_push_back(&buf, &s);
}

// Calculate instantaneous derivative of num_samples
bool ds_is_local_max(size_t num_samples) {

  
  if (num_samples*2 >= cb_size(&buf)) {
    return false;
  }
  if (num_samples < 4) {
    return false;
  } 
  
  // Average over whole datastore
  Sample s1;
  uint16_t avg = 0;
  for (int i=0; i<(int)cb_size(&buf); i++) {
    s1 = *(Sample*)cb_peek(&buf, i);
    avg += s1.data;
  }
  avg /= (uint16_t)cb_size(&buf);
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Data average: %u", (unsigned int)avg);

  // Check for absolute threshold and local min/max
  float deriv[num_samples-1];
  float deriv_avg = 0;
  Sample s2;
  s1 = *(Sample*)cb_peek(&buf, 0);
  uint16_t current_avg = s1.data;
  for (int i=1; i<(int)num_samples; i++) {
    s2 = *(Sample*)cb_peek(&buf, i);
    current_avg += s2.data;
    deriv[i-1] = (float)((uint16_t)s1.data - (uint16_t)s2.data);
    deriv_avg += deriv[i-1];

    s1 = *(Sample*)cb_peek(&buf, i);
  }
  current_avg /= (uint16_t)num_samples;
  deriv_avg /= (float)num_samples - 1.0;
  
  
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Local average: %u", (unsigned int)current_avg);
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Deriv average: %d.%03d", (int)deriv_avg, (int)(deriv_avg*1000)%1000);
  
  // No need to continue if absolute accel is low
  if (current_avg <= avg) {
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "Not local max; current_avg (%u) smaller than avg (%u)", (unsigned int)current_avg, (unsigned int)avg);
    return false;
  }
  
  // No need to continue if slope is high
  if (abs(deriv_avg) > PEAK_DERIV) {
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "Not local min/max; deriv too high at %d.%03d", (int)deriv_avg, (int)(deriv_avg*1000)%1000);
    return false;
  }
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Local min/max; deriv %d.%03d", (int)deriv_avg, (int)(deriv_avg*1000)%1000);
  
  // Check double derivative
  float dderiv_avg = 0;
  for (int i=1; i<(int)num_samples-1; i++) {
    dderiv_avg += (deriv[i] - deriv[i-1])/2.0;
  }
  if (dderiv_avg >= 0) {
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "Local max; dderiv %f", dderiv_avg);
    return true;
  }
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Local min; dderiv %f", dderiv_avg);
  return false;
}

void init_datastore(size_t capacity) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Initialize datastore");
  size_t sz = sizeof(Sample);
  cb_init(&buf, capacity, sz);
}

void deinit_datastore(void) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Deinitialize datastore");
  
  // Log the current buffer state into the datalog
  static DataLoggingSessionRef s_session_ref;
  s_session_ref = data_logging_create(1, DATA_LOGGING_UINT, sizeof(uint16_t), false);
  Sample s;
  for (int i=0; i<(int)cb_size(&buf); i++) {
    s = *(Sample*)cb_peek(&buf, i);
    data_logging_log(s_session_ref, &(s.data), 1);
  }
  data_logging_finish(s_session_ref);

  cb_free(&buf);
}