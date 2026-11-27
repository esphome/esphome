#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::tflite_text {

/// @brief BERT-style WordPiece tokenizer over a vocabulary stored in flash.
///
/// Text is split at ASCII whitespace and punctuation, each punctuation character being
/// a word of its own, and every word is cut into the longest vocabulary pieces from
/// the left; pieces after the first carry the "##" prefix. A word that cannot be cut
/// completely, or is longer than MAX_WORD_BYTES, becomes the unknown token. Only ASCII
/// letters are lower-cased, and other characters are matched as they are.
class WordPieceTokenizer {
 public:
  static constexpr size_t MAX_WORD_BYTES = 100;

  /// The arrays come from code generation and must outlive the tokenizer.
  ///
  /// @param text     The tokens, each NUL-terminated.
  /// @param offsets  The start of each token in @p text, ordered by the token's bytes.
  /// @param ids      The id of the token at the same position in @p offsets.
  WordPieceTokenizer(const uint8_t *text, const uint32_t *offsets, const uint16_t *ids, size_t size,
                     uint16_t unknown_id, uint16_t start_id, uint16_t end_id, uint16_t pad_id, bool lowercase)
      : text_(text),
        offsets_(offsets),
        ids_(ids),
        size_(size),
        unknown_id_(unknown_id),
        start_id_(start_id),
        end_id_(end_id),
        pad_id_(pad_id),
        lowercase_(lowercase) {}

  /// Write the start token, the pieces of @p input and the end token, truncating the
  /// pieces to fit, then pad the rest of @p out.
  ///
  /// @return the number of tokens before the padding; 0 when @p capacity is below 2.
  size_t encode(const char *input, size_t length, int32_t *out, size_t capacity) const;

  /// The id of the token of @p length bytes, none of them NUL, or -1 when the vocabulary does not have it.
  int32_t find(const char *token, size_t length) const;

 protected:
  /// Append the pieces of one word at @p count, writing no further than @p limit.
  void append_word_(const char *word, size_t length, int32_t *out, size_t &count, size_t limit) const;

  const uint8_t *text_;
  const uint32_t *offsets_;
  const uint16_t *ids_;
  size_t size_;
  uint16_t unknown_id_;
  uint16_t start_id_;
  uint16_t end_id_;
  uint16_t pad_id_;
  bool lowercase_;
};

}  // namespace esphome::tflite_text
