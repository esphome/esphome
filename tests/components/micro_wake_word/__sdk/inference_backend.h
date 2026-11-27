#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Only the external inference engine is doubled. StreamingModel and WakeWordModel
// are production code, driven through their ordinary enable/disable/inference APIs.
using TfLiteStatus = int;
static constexpr int kTfLiteOk = 0, kTfLiteInt8 = 9, kTfLiteUInt8 = 3, TFLITE_SCHEMA_VERSION = 3;
struct TfLiteIntArray {
  int size;
  int data[3];
};
union TfLitePtrUnion {
  int8_t *int8;
  uint8_t *uint8;
};
struct TfLiteTensor {
  TfLiteIntArray *dims;
  int type;
  TfLitePtrUnion data;
};

namespace esphome::micro_wake_word::testing {
inline std::vector<std::array<int8_t, 120>> invocations;
}
namespace tflite {
struct Model {
  int version() const { return TFLITE_SCHEMA_VERSION; }
};
inline const Model *GetModel(const uint8_t *) {
  static Model model;
  return &model;
}
class MicroAllocator {
 public:
  static MicroAllocator *Create(uint8_t *, size_t) {
    static MicroAllocator allocator;
    return &allocator;
  }
};
class MicroResourceVariables {
 public:
  static MicroResourceVariables *Create(MicroAllocator *, size_t) {
    static MicroResourceVariables variables;
    return &variables;
  }
};
template<int N> class MicroMutableOpResolver {
 public:
  int AddCallOnce() { return kTfLiteOk; }
  int AddVarHandle() { return kTfLiteOk; }
  int AddReshape() { return kTfLiteOk; }
  int AddReadVariable() { return kTfLiteOk; }
  int AddStridedSlice() { return kTfLiteOk; }
  int AddConcatenation() { return kTfLiteOk; }
  int AddAssignVariable() { return kTfLiteOk; }
  int AddConv2D() { return kTfLiteOk; }
  int AddMul() { return kTfLiteOk; }
  int AddAdd() { return kTfLiteOk; }
  int AddMean() { return kTfLiteOk; }
  int AddFullyConnected() { return kTfLiteOk; }
  int AddLogistic() { return kTfLiteOk; }
  int AddQuantize() { return kTfLiteOk; }
  int AddDepthwiseConv2D() { return kTfLiteOk; }
  int AddAveragePool2D() { return kTfLiteOk; }
  int AddMaxPool2D() { return kTfLiteOk; }
  int AddPad() { return kTfLiteOk; }
  int AddPack() { return kTfLiteOk; }
  int AddSplitV() { return kTfLiteOk; }
};
class MicroInterpreter {
 public:
  template<int N>
  MicroInterpreter(const Model *, MicroMutableOpResolver<N> &, uint8_t *, size_t, MicroResourceVariables *) {
    // Fresh tensor rows are observable poison, so missing input is not mistaken for new feature slices.
    this->input_bytes_.fill(85);
    this->input_.data.int8 = this->input_bytes_.data();
    this->output_.data.uint8 = &this->probability_;
  }
  int AllocateTensors() { return kTfLiteOk; }
  int Invoke() {
    esphome::micro_wake_word::testing::invocations.push_back(this->input_bytes_);
    return kTfLiteOk;
  }
  size_t arena_used_bytes() const { return 128; }
  TfLiteTensor *input(int) { return &this->input_; }
  TfLiteTensor *output(int) { return &this->output_; }

 protected:
  std::array<int8_t, 120> input_bytes_{};
  TfLiteIntArray input_dimensions_{3, {1, 3, 40}}, output_dimensions_{2, {1, 1, 0}};
  TfLiteTensor input_{&this->input_dimensions_, kTfLiteInt8, {}}, output_{&this->output_dimensions_, kTfLiteUInt8, {}};
  uint8_t probability_{0};
};
template<typename T> T *GetTensorData(TfLiteTensor *tensor) { return reinterpret_cast<T *>(tensor->data.int8); }
}  // namespace tflite
