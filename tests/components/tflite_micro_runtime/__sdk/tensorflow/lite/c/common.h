#pragma once

#include <cstddef>
#include <cstdint>

// Other suites double these types differently, and every suite is linked into one test program, so this
// suite's types get names of their own.
#define TfLiteStatus RuntimeTestTfLiteStatus
#define TfLitePtrUnion RuntimeTestTfLitePtrUnion
#define TfLiteTensor RuntimeTestTfLiteTensor

enum TfLiteStatus { kTfLiteOk, kTfLiteError };
enum TfLiteType { kTfLiteFloat32 = 1, kTfLiteInt32 = 2, kTfLiteUInt8 = 3, kTfLiteInt8 = 9 };
union TfLitePtrUnion {
  char *raw;
  float *f;
  uint8_t *uint8;
  int8_t *int8;
};
struct TfLiteTensor {
  TfLiteType type;
  TfLitePtrUnion data;
  size_t bytes;
};
