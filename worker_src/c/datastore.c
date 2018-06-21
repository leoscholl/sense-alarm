#include <pebble_worker.h>
#include "datastore.h"


// Create a new circular buffer
void cb_init(circular_buffer *cb, size_t capacity, size_t sz)
{
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Allocating %u B buffer", 
          (unsigned int)(capacity * sz));
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