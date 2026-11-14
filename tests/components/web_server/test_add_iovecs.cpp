#ifdef USE_HOST
#include <gtest/gtest.h>

#include <sys/uio.h>

#include <cstdint>
#include <string>
#include <vector>

#include "esphome/components/web_server_base/stream_head.h"

namespace esphome::web_server::testing {

using namespace web_server_base;  // NOLINT(google-build-using-namespace)

// Records each add() and send() like an AsyncClient whose send buffer holds space bytes
struct FakeClient {
  size_t space{SIZE_MAX};
  std::string bytes;
  std::vector<uint8_t> flags;
  int sends{0};

  size_t add(const char *data, size_t len, uint8_t apiflags) {
    this->flags.push_back(apiflags);
    const size_t taken = std::min(len, this->space);
    this->bytes.append(data, taken);
    this->space -= taken;
    return taken;
  }
  bool send() {
    this->sends++;
    return true;
  }
};

static std::vector<struct iovec> pieces(std::vector<std::string> &parts) {
  std::vector<struct iovec> iov;
  for (auto &part : parts) {
    iov.push_back({part.data(), part.size()});
  }
  return iov;
}

TEST(AddIovecs, CoalescesPiecesIntoOneAdd) {
  std::vector<std::string> parts{"abc", "de", "fghij"};
  auto iov = pieces(parts);
  FakeClient client;
  char scratch[64];
  EXPECT_EQ(add_iovecs(client, iov.data(), static_cast<int>(iov.size()), scratch, sizeof(scratch)), 10u);
  EXPECT_EQ(client.bytes, "abcdefghij");
  ASSERT_EQ(client.flags.size(), 1u);
  EXPECT_EQ(client.flags[0], STREAM_ADD_FLAG_COPY);
  EXPECT_EQ(client.sends, 1);
}

TEST(AddIovecs, SplitsAtScratchSizeWithMoreOnAllButLast) {
  std::vector<std::string> parts{"0123456789", "abcdefghij", "XYZ"};
  auto iov = pieces(parts);
  FakeClient client;
  char scratch[8];
  EXPECT_EQ(add_iovecs(client, iov.data(), static_cast<int>(iov.size()), scratch, sizeof(scratch)), 23u);
  EXPECT_EQ(client.bytes, "0123456789abcdefghijXYZ");
  ASSERT_EQ(client.flags.size(), 3u);
  EXPECT_EQ(client.flags[0], STREAM_ADD_FLAG_COPY | STREAM_ADD_FLAG_MORE);
  EXPECT_EQ(client.flags[1], STREAM_ADD_FLAG_COPY | STREAM_ADD_FLAG_MORE);
  EXPECT_EQ(client.flags[2], STREAM_ADD_FLAG_COPY);
  EXPECT_EQ(client.sends, 1);
}

TEST(AddIovecs, StopsAtPartialAccept) {
  std::vector<std::string> parts{"0123456789", "abcdefghij"};
  auto iov = pieces(parts);
  FakeClient client;
  client.space = 12;
  char scratch[8];
  EXPECT_EQ(add_iovecs(client, iov.data(), static_cast<int>(iov.size()), scratch, sizeof(scratch)), 12u);
  EXPECT_EQ(client.bytes, "0123456789ab");
  EXPECT_EQ(client.flags.size(), 2u);  // nothing offered after the short add()
  EXPECT_EQ(client.sends, 1);
}

TEST(AddIovecs, ZeroSpaceSendsNothing) {
  std::vector<std::string> parts{"abc"};
  auto iov = pieces(parts);
  FakeClient client;
  client.space = 0;
  char scratch[8];
  EXPECT_EQ(add_iovecs(client, iov.data(), static_cast<int>(iov.size()), scratch, sizeof(scratch)), 0u);
  EXPECT_EQ(client.flags.size(), 1u);
  EXPECT_EQ(client.sends, 0);
}

TEST(AddIovecs, EmptyPiecesAreSkipped) {
  std::vector<std::string> parts{"", "ab", "", "c"};
  auto iov = pieces(parts);
  FakeClient client;
  char scratch[8];
  EXPECT_EQ(add_iovecs(client, iov.data(), static_cast<int>(iov.size()), scratch, sizeof(scratch)), 3u);
  EXPECT_EQ(client.bytes, "abc");
  ASSERT_EQ(client.flags.size(), 1u);
  EXPECT_EQ(client.flags[0], STREAM_ADD_FLAG_COPY);
}

}  // namespace esphome::web_server::testing
#endif  // USE_HOST
