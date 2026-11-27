#pragma once

#include <cstddef>
#include <cstdint>

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
