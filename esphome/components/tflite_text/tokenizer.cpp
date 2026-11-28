#include "tokenizer.h"

#include <algorithm>
#include <cstring>

namespace esphome::tflite_text {

namespace {

/// ASCII whitespace and control characters separate words.
bool is_separator(uint8_t c) { return c <= ' ' || c == 0x7F; }

bool is_punctuation(uint8_t c) {
  return (c >= '!' && c <= '/') || (c >= ':' && c <= '@') || (c >= '[' && c <= '`') || (c >= '{' && c <= '~');
}

/// Whether a UTF-8 character starts at this byte.
bool is_character_start(char c) { return (static_cast<uint8_t>(c) & 0xC0) != 0x80; }

}  // namespace

int32_t WordPieceTokenizer::find(const char *token, size_t length) const {
  size_t low = 0;
  size_t high = this->size_;
  while (low < high) {
    const size_t middle = low + (high - low) / 2;
    const char *entry = reinterpret_cast<const char *>(this->text_) + this->offsets_[middle];
    // strncmp() orders bytes as unsigned, as code generation sorted them, and stops at a shorter entry's NUL.
    int order = strncmp(entry, token, length);
    if (order == 0 && entry[length] != '\0')
      order = 1;  // the token is a prefix of this entry
    if (order == 0)
      return this->ids_[middle];
    if (order < 0) {
      low = middle + 1;
    } else {
      high = middle;
    }
  }
  return -1;
}

void WordPieceTokenizer::append_word_(const char *word, size_t length, int32_t *out, size_t &count,
                                      size_t limit) const {
  const size_t first = count;
  auto unknown = [&]() {
    count = first;
    if (count < limit)
      out[count++] = this->unknown_id_;
  };
  if (length > MAX_WORD_BYTES) {
    unknown();
    return;
  }
  char piece[MAX_WORD_BYTES + 2];
  size_t start = 0;
  while (start < length) {
    const size_t prefix = start == 0 ? 0 : 2;
    memcpy(piece, "##", prefix);
    for (size_t i = start; i < length; i++) {
      const char c = word[i];
      piece[prefix + i - start] = this->lowercase_ && c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    }
    int32_t id = -1;
    size_t end = length;
    // Longest match first, ending only between characters.
    for (; end > start; end--) {
      if (end < length && !is_character_start(word[end]))
        continue;
      id = this->find(piece, prefix + end - start);
      if (id >= 0)
        break;
    }
    if (id < 0) {
      unknown();
      return;
    }
    if (count < limit)
      out[count++] = id;
    start = end;
  }
}

size_t WordPieceTokenizer::encode(const char *input, size_t length, int32_t *out, size_t capacity) const {
  if (capacity < 2)
    return 0;
  size_t count = 0;
  const size_t limit = capacity - 1;  // the end token always fits
  out[count++] = this->start_id_;
  size_t i = 0;
  while (i < length && count < limit) {
    const uint8_t c = static_cast<uint8_t>(input[i]);
    if (is_separator(c)) {
      i++;
      continue;
    }
    size_t end = i + 1;
    if (!is_punctuation(c)) {
      while (end < length && !is_separator(static_cast<uint8_t>(input[end])) &&
             !is_punctuation(static_cast<uint8_t>(input[end])))
        end++;
    }
    this->append_word_(input + i, end - i, out, count, limit);
    i = end;
  }
  out[count++] = this->end_id_;
  std::fill(out + count, out + capacity, this->pad_id_);
  return count;
}

}  // namespace esphome::tflite_text
