#include <pebble_worker.h>
#include "math.h"

#define SQRT_MAGIC_F 0x5f3759df 

const float PI     = 3.14159265;
const float TWOPI  = 6.28318531;
const float HALFPI = 1.57079633;
const float LN2    = 0.693147181;

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

float sm_exp(float x) {
  int LIMIT = 100;
  int INVERSE_LIMIT = 100000; // Inverse fractional limit 100000 = 0.00001 == 0.001%
  double x1 = 1;// running x, x^2, x^3 etc
  double y = 1; // result
  double yl = 1; // last result
  double yd = 1; // delta result
  double d = 1; // denominator
  for (int i=1; i<=LIMIT; i++) {
    yl = y;
    x1 *= x;
    d *= i;
    y += x1 / d;
    if (y == yl) {
//      printf("sm_exp iterations: %i", i);
      break;
    }
    yd = y / (y - yl);
    if (yd > INVERSE_LIMIT || yd < -INVERSE_LIMIT) {
//      printf("sm_exp iterations: %i", i);
      break;
    }
  }
  return y;
}

float sm_agm(float x1, float x2) {
// Arithmetic-Geometric Mean  
  int LIMIT = 100;
  int MIN_LIMIT = 6;
  int INVERSE_LIMIT = 100000; // Inverse fractional limit 100000 = 0.00001 == 0.001%
  float a1 = 1;
  float g1 = 1000;
  float a2 = x1;
  float g2 = x2;
  
  for (int n=1; n<=LIMIT; n++) {
    a1 = a2;
    g1 = g2;
    a2 = (a1 + g1)/2.0;
    g2 = sm_sqrt(a1 * g1);
    float yd = 1.0f;
   
    if (n >= MIN_LIMIT) {
      if (a2 == g2) {
//        printf("sm_agm iterations: %i", n);
        break;
      }
      yd = g2 / (g2 - g1);
      if (yd > INVERSE_LIMIT || yd < -INVERSE_LIMIT) {
//        printf("sm_agm iterations: %i", n);
        break;
      }
    }
  }
  
  return a2;
}

float sm_powint(float x, int y) {
  float powint = 1.0f;
  if (y > 0) {
    for (int i=0; i<y; i++) {
      powint *= x;
    }
  } else {
    for (int i=0; i<y; i++) {
      powint /= x;    
    }
  }
  return powint;
}

float sm_ln(float x) {
  const int p = 18; // bits of precision
  int m = 9;
  int twoPowerM = sm_powint(2.0, m);  
  float s = x * twoPowerM;
  
  while (s < sm_powint(2, p/2)) {
    m+=1;
    twoPowerM = sm_powint(2, m);
    s = x * twoPowerM;
  }
  
  float part1 = 4.0f / s;
  float part2 = sm_agm(1.0f, part1) * 2;
  float part3 = PI / part2;
  float part4 = LN2 * m;
  float y = part3 - part4;
  return y;
}

float sm_pow(float x, float y) {
  // relies on exp and ln, so you're only going to get 6 sig figs out of this.
  //printf("sm_pow: sm_ln(x)=%i/1000", (int)(1000*sm_ln(x)));
  //printf("sm_pow: y * sm_ln(x)=%i/1000", (int)(1000*(y * sm_ln(x))));
  if (x <= 0.0) {
    return 0.0;
  }
  if (y == 0.0) {
    return 1.0;
  }
  
  if (y == (int)y) {
    return sm_powint(x, (int)y);
  }
  
  float res = sm_exp(y * sm_ln(x));
  return res;
}

