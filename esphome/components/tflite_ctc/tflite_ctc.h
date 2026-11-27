#pragma once

#include "esphome/core/defines.h"

#include <cstddef>
#include <cstdint>

#ifdef USE_TFLITE_MICRO_HELPER
#include <string>

#include "esphome/components/tflite_output_processor/output_processor.h"
#include "esphome/core/helpers.h"
#endif

namespace esphome::tflite_ctc {

/// @brief Greedy CTC decoding of a steps x classes score matrix.
///
/// Takes the best class of each step, collapses repeats and drops the blank, so a
/// class that appears twice in a row in the result had a blank between its steps.
///
/// @param ids    Receives up to @p steps class ids.
/// @param score  `score(index)` returns the value at step * classes + class.
/// @return the number of ids written.
template<typename Score>
size_t ctc_greedy_decode(uint16_t *ids, size_t steps, size_t classes, size_t blank, Score &&score) {
  size_t count = 0;
  size_t previous = blank;
  for (size_t step = 0; step < steps; step++) {
    size_t best = 0;
    float best_score = score(step * classes);
    for (size_t index = 1; index < classes; index++) {
      const float value = score(step * classes + index);
      if (value > best_score) {
        best = index;
        best_score = value;
      }
    }
    if (best != blank && best != previous)
      ids[count++] = static_cast<uint16_t>(best);
    previous = best;
  }
  return count;
}

/// The position of a class in a label list, which leaves out the blank.
inline size_t ctc_label_index(size_t class_id, size_t blank) { return class_id > blank ? class_id - 1 : class_id; }

#ifdef USE_TFLITE_MICRO_HELPER
/// @brief Decodes a [1, steps, classes] output into a sequence of class ids.
class CtcDecoder final : public tflite_output_processor::OutputDecoder {
 public:
  explicit CtcDecoder(size_t blank) : blank_(blank) {}
  /// One label per class other than the blank; code generation passes a static array.
  void set_labels(const char *const *labels) { this->labels_ = labels; }

  bool decode(TfLiteTensor *const *outputs, size_t count) override;

  const uint16_t *ids() const { return this->ids_.get(); }
  size_t id_count() const { return this->id_count_; }
  /// The labels of the decoded ids, joined; empty without labels.
  std::string text() const;

 protected:
  size_t blank_;
  const char *const *labels_{nullptr};
  RAMUniquePtr<uint16_t[]> ids_;
  size_t capacity_{0};
  size_t id_count_{0};
};
#endif

}  // namespace esphome::tflite_ctc
