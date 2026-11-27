#pragma once

#include "esphome/core/defines.h"

#include <cstddef>
#include <cstdint>

#include <cstring>

#ifdef USE_TFLITE_MICRO_HELPER
#include "esphome/components/tflite_output_processor/output_processor.h"
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

/// @brief Joins the labels of decoded ids into @p out, always NUL-terminated.
///
/// Stops before the first label that does not fit whole.
///
/// @return the length written, without the NUL; 0 when @p size is 0.
inline size_t ctc_join_labels(char *out, size_t size, const uint16_t *ids, size_t count, const char *const *labels,
                              size_t blank) {
  if (size == 0)
    return 0;
  size_t length = 0;
  for (size_t i = 0; i < count; i++) {
    const char *label = labels[ctc_label_index(ids[i], blank)];
    const size_t label_length = strlen(label);
    if (label_length >= size - length)
      break;
    memcpy(out + length, label, label_length);
    length += label_length;
  }
  out[length] = '\0';
  return length;
}

#ifdef USE_TFLITE_MICRO_HELPER
/// @brief Decodes a [1, steps, classes] output into a sequence of class ids.
class CtcDecoder final : public tflite_output_processor::OutputDecoder {
 public:
  /// @param ids  Storage for @p capacity ids, one per step, from code generation.
  CtcDecoder(size_t blank, uint16_t *ids, size_t capacity) : blank_(blank), ids_(ids), capacity_(capacity) {}
  /// One label per class other than the blank; code generation passes a static array.
  void set_labels(const char *const *labels) { this->labels_ = labels; }

  bool decode(TfLiteTensor *const *outputs, size_t count) override;

  const uint16_t *ids() const { return this->ids_; }
  size_t id_count() const { return this->id_count_; }
  /// Write the labels of the decoded ids into @p out; empty without labels.
  /// @return the length written, as ctc_join_labels().
  size_t text(char *out, size_t size) const;

 protected:
  size_t blank_;
  const char *const *labels_{nullptr};
  uint16_t *ids_;
  size_t capacity_;
  size_t id_count_{0};
};
#endif

}  // namespace esphome::tflite_ctc
