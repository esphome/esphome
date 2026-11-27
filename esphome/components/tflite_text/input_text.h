#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstddef>
#include <cstdint>

#include "esphome/components/tflite_micro_runtime/tflite_micro_runtime.h"
#include "tokenizer.h"

namespace esphome::tflite_text {

/// @brief Fills a text model's int32 inputs from text or from token ids.
///
/// Input 0 receives the tokens, input 1 the attention mask (one for each token, zero for
/// the padding after them) and input 2 an all-zero segment mask, the layout the exported
/// BERT-style models expect. With a tokenizer the payload is UTF-8 text; without one it
/// is the int32 token ids.
class TextInputPipeline final : public tflite_micro_runtime::InputPipeline {
 public:
  void set_tokenizer(const WordPieceTokenizer *tokenizer) { this->tokenizer_ = tokenizer; }
  bool fill(TfLiteTensor *input, const uint8_t *src, size_t size) override { return false; }
  bool fill_all(TfLiteTensor *const *inputs, size_t count, const uint8_t *src, size_t size) override;
  const char *name() const override { return "text"; }

 protected:
  const WordPieceTokenizer *tokenizer_{nullptr};
};

}  // namespace esphome::tflite_text

#endif  // USE_TFLITE_MICRO_HELPER
