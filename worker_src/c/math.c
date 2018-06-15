#include <pebble_worker.h>
#include "math.h"

#define SQRT_MAGIC_F 0x5f3759df 

float sm_sqrt(const float x)
{
  const float xhalf = 0.5f*x;
 
  union // get bits for floating value
  {
    float x;
    int i;
  } u;
  u.x = x;
  u.i = SQRT_MAGIC_F - (u.i >> 1);  // gives initial guess y0
  
  u.x = u.x*(1.5f - xhalf*u.x*u.x);   // This can be removed for increasing speed
  u.x = u.x*(1.5f - xhalf*u.x*u.x);   // This can be removed for increasing speed
  return x*u.x*(1.5f - xhalf*u.x*u.x);// Newton step, repeating increases accuracy 
}