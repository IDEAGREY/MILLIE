#pragma once
#include <math.h>
namespace millie {
struct Point {
  float x, y;
};
struct Calibration {
  uint32_t magic = 0;
  float a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
};
inline Point project(const Calibration &k, Point p) {
  return {k.a * p.x + k.b * p.y + k.c, k.d * p.x + k.e * p.y + k.f};
}
inline bool solveCalibration(const Point *r, const Point *s, Calibration &k) {
  float den = r[0].x * (r[1].y - r[2].y) + r[1].x * (r[2].y - r[0].y) + r[2].x * (r[0].y - r[1].y);
  if (fabsf(den) < 40000)
    return false;
  float x[3] = {s[0].x, s[1].x, s[2].x}, y[3] = {s[0].y, s[1].y, s[2].y};
  float coeff[6];
  for (int dim = 0; dim < 2; dim++) {
    float *v = dim ? y : x;
    coeff[dim * 3] =
        (v[0] * (r[1].y - r[2].y) + v[1] * (r[2].y - r[0].y) + v[2] * (r[0].y - r[1].y)) / den;
    coeff[dim * 3 + 1] =
        (v[0] * (r[2].x - r[1].x) + v[1] * (r[0].x - r[2].x) + v[2] * (r[1].x - r[0].x)) / den;
    coeff[dim * 3 + 2] =
        (v[0] * (r[1].x * r[2].y - r[2].x * r[1].y) + v[1] * (r[2].x * r[0].y - r[0].x * r[2].y) +
         v[2] * (r[0].x * r[1].y - r[1].x * r[0].y)) /
        den;
  }
  for (float f : coeff)
    if (!isfinite(f))
      return false;
  k.magic = 0x54434832;
  k.a = coeff[0];
  k.b = coeff[1];
  k.c = coeff[2];
  k.d = coeff[3];
  k.e = coeff[4];
  k.f = coeff[5];
  return fabsf(k.a * k.e - k.b * k.d) > 0.00001f;
}
inline bool calibrationValid(const Calibration &k) {
  return k.magic == 0x54434832 && isfinite(k.a) && isfinite(k.b) && isfinite(k.c) &&
         isfinite(k.d) && isfinite(k.e) && isfinite(k.f) && fabsf(k.a) < 1 && fabsf(k.b) < 1 &&
         fabsf(k.d) < 1 && fabsf(k.e) < 1 && fabsf(k.c) < 1000 && fabsf(k.f) < 1000 &&
         fabsf(k.a * k.e - k.b * k.d) > 0.00001f;
}
inline int navAt(int x, int y) { return x >= 0 && x < 240 && y >= 288 && y < 320 ? x / 48 : -1; }
} // namespace millie
