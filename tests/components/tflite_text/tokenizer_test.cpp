#include <gtest/gtest.h>

#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

#include "esphome/components/tflite_text/tokenizer.h"

namespace esphome::tflite_text::testing {

namespace {
constexpr int32_t PAD = 0, UNK = 1, CLS = 2, SEP = 3, HELLO = 4, WORLD = 5, S = 6, BANG = 7, UN = 8, ABLE = 9,
                  CAFE = 10;

/// Builds the arrays code generation would emit for these tokens.
class Vocabulary {
 public:
  explicit Vocabulary(const std::vector<std::string> &tokens) {
    std::vector<uint16_t> order(tokens.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](uint16_t a, uint16_t b) { return tokens[a] < tokens[b]; });
    for (uint16_t id : order) {
      this->offsets.push_back(this->text.size());
      this->ids.push_back(id);
      this->text.insert(this->text.end(), tokens[id].begin(), tokens[id].end());
      this->text.push_back(0);
    }
  }
  WordPieceTokenizer tokenizer(bool lowercase = true) const {
    return {this->text.data(), this->offsets.data(), this->ids.data(), this->ids.size(), UNK, CLS, SEP, PAD, lowercase};
  }
  std::vector<uint8_t> text;
  std::vector<uint32_t> offsets;
  std::vector<uint16_t> ids;
};

const Vocabulary VOCABULARY({"[PAD]", "[UNK]", "[CLS]", "[SEP]", "hello", "world", "##s", "!", "un", "##able",
                             "caf\xC3\xA9"});

std::vector<int32_t> encode(const std::string &text, size_t capacity = 8, bool lowercase = true) {
  std::vector<int32_t> out(capacity, -1);
  out.resize(VOCABULARY.tokenizer(lowercase).encode(text.data(), text.size(), out.data(), capacity));
  return out;
}
}  // namespace

TEST(WordPieceTokenizerTest, WordsAndPunctuationAreFramedByTheSpecialTokens) {
  EXPECT_EQ(encode("Hello  world!\n"), (std::vector<int32_t>{CLS, HELLO, WORLD, BANG, SEP}));
}

TEST(WordPieceTokenizerTest, AWordIsCutIntoTheLongestPieces) {
  EXPECT_EQ(encode("worlds unables"), (std::vector<int32_t>{CLS, WORLD, S, UN, ABLE, S, SEP}));
}

TEST(WordPieceTokenizerTest, AWordThatCannotBeCutCompletelyIsUnknown) {
  EXPECT_EQ(encode("worldx hello"), (std::vector<int32_t>{CLS, UNK, HELLO, SEP}));
}

TEST(WordPieceTokenizerTest, AnOverlongWordIsUnknown) {
  EXPECT_EQ(encode(std::string(WordPieceTokenizer::MAX_WORD_BYTES + 1, 's')), (std::vector<int32_t>{CLS, UNK, SEP}));
}

TEST(WordPieceTokenizerTest, LowercasingCanBeTurnedOff) {
  EXPECT_EQ(encode("Hello", 8, false), (std::vector<int32_t>{CLS, UNK, SEP}));
}

TEST(WordPieceTokenizerTest, MultiByteCharactersMatchAsStored) {
  EXPECT_EQ(encode("caf\xC3\xA9s caf\xC3"), (std::vector<int32_t>{CLS, CAFE, S, UNK, SEP}));
}

TEST(WordPieceTokenizerTest, TheEndTokenSurvivesTruncation) {
  EXPECT_EQ(encode("hello world hello", 4), (std::vector<int32_t>{CLS, HELLO, WORLD, SEP}));
  EXPECT_EQ(encode("worlds", 3), (std::vector<int32_t>{CLS, WORLD, SEP}));
  // A word that is unknown stays unknown even when only its first piece would fit.
  EXPECT_EQ(encode("worldx", 3), (std::vector<int32_t>{CLS, UNK, SEP}));
}

TEST(WordPieceTokenizerTest, TheRestIsPadded) {
  std::vector<int32_t> out(6, -1);
  EXPECT_EQ(VOCABULARY.tokenizer().encode("hello", 5, out.data(), out.size()), 3u);
  EXPECT_EQ(out, (std::vector<int32_t>{CLS, HELLO, SEP, PAD, PAD, PAD}));
}

TEST(WordPieceTokenizerTest, EmptyTextAndNoRoom) {
  EXPECT_EQ(encode(""), (std::vector<int32_t>{CLS, SEP}));
  EXPECT_TRUE(encode("hello", 1).empty());
}

}  // namespace esphome::tflite_text::testing
