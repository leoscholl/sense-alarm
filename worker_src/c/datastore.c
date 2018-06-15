#include <pebble_worker.h>
#include "datastore.h"
#include "math.h"

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
  
  void *item = cb->head - cb->sz - index * cb->sz;
  if (item < cb->buffer)
    item = cb->buffer_end - (cb->buffer - item);
  return item;
}

// Store a data sample into the buffer
void store_sample(uint16_t data) {
  Sample s;
  s.data = data;
  cb_push_back(&buf, &s);
}

// Return contents of datastore
uint16_t *ds_contents(void) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Buffer holds %u items, copying to new array", (unsigned int)cb_size(&buf));
  uint16_t *contents = malloc(cb_size(&buf) * sizeof(uint16_t));
  if(contents == NULL)
    APP_LOG(APP_LOG_LEVEL_ERROR, "Memory allocation failed");
  Sample s;
  for (int i=0; i<(int)cb_size(&buf); i++) {
    s = *(Sample*)cb_peek(&buf, i);
    contents[i] = s.data;
  }
  return contents;
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

// Calculate instantaneous derivative of num_samples
bool ds_is_local_max(unsigned int num_samples) {

  unsigned int num_buffer = cb_size(&buf);
  unsigned int num_bins = num_buffer/num_samples;
    
  if (num_samples*2 >= num_buffer) {
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
  uint16_t thr = avg + 2*std;
  
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

void init_datastore(size_t capacity) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Initialize datastore");
  size_t sz = sizeof(Sample);
  cb_init(&buf, capacity, sz);
}

void deinit_datastore(void) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Deinitialize datastore");
  cb_free(&buf);
}