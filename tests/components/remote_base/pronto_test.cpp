#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/remote_base/pronto_protocol.h"

namespace esphome::remote_base::testing {

// Output of the string-building decode this replaced, for the same timings
static constexpr const char *SHORT_TEXT = "0000 006D 0005 0000 0159 00AE 0015 0016 0015 0042 0015 0603 0000 0001 ";

static const std::vector<int32_t> SHORT_TIMINGS{9000, -4500, 560, -560, 560, -1690, 560, -40000, 10, -5};

TEST(ProntoProtocol, DecodeText) {
  RemoteReceiveData src(SHORT_TIMINGS, 25, TOLERANCE_MODE_PERCENTAGE);
  auto decoded = ProntoProtocol().decode(src);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->data, SHORT_TEXT);
  EXPECT_EQ(decoded->delta, -1);
  EXPECT_EQ(ProntoProtocol::text_length(SHORT_TIMINGS), std::string(SHORT_TEXT).size());
}

TEST(ProntoProtocol, WriteTextInPiecesMatchesDecode) {
  std::vector<int32_t> timings;
  for (int i = 0; i < 120; i++)
    timings.push_back(i % 2 == 0 ? 300 + i * 7 : -(200 + i * 11));
  RemoteReceiveData src(timings, 25, TOLERANCE_MODE_PERCENTAGE);
  const std::string whole = ProntoProtocol().decode(src)->data;
  // Piece sizes that split numbers at every position, like the dumper's log chunks
  for (size_t piece : {1u, 3u, 7u, 230u}) {
    std::string joined;
    char buf[230];
    for (size_t offset = 0; offset < whole.size(); offset += piece) {
      const size_t len = std::min(piece, whole.size() - offset);
      ProntoProtocol::write_text(timings, offset, buf, len);
      joined.append(buf, len);
    }
    EXPECT_EQ(joined, whole) << "piece " << piece;
  }
}

}  // namespace esphome::remote_base::testing
