#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>
#include "esphome/components/lzss/lzss.h"

namespace esphome::lzss::testing {

using Bytes = std::vector<uint8_t>;

static Bytes text(const std::string &value) { return {value.begin(), value.end()}; }

static const std::string JSON_DICTIONARY =
    R"({"name":"","value":,"state":"ON","OFF","unit":"","id":"","type":"temperature","humidity",)"
    R"("time":"","status":"ok","error":null,"enabled":true,"false},{"":[{""}]})";

static const std::string WEATHER =
    R"({"station":"backyard","temperature":{"value":18.2,"min":12.1,"max":23.9},"humidity":{"value":66,"min":51,)"
    R"("max":90},"wind":{"speed":3.4,"gust":7.9,"direction":"SSW"},"rain":{"rate":0.0,"today":2.4},"uv":3})";

// A codec with its dictionary and working space
struct Codec {
  explicit Codec(const std::string &dictionary)
      : dictionary(dictionary.begin(), dictionary.end()),
        lzss(this->dictionary.data(), this->dictionary.size()),
        chains(Lzss::CHAIN_WORDS) {}
  Bytes compress(const Bytes &data, size_t capacity) {
    Bytes out(capacity);
    out.resize(this->lzss.compress(data.data(), data.size(), out.data(), capacity, this->chains.data()));
    return out;
  }
  // Empty with ok false when the data is malformed
  Bytes expand(const Bytes &data, size_t capacity, bool &ok) const {
    Bytes out(capacity);
    size_t length = this->lzss.expand(data.data(), data.size(), out.data(), capacity);
    ok = length != SIZE_MAX;
    out.resize(ok ? length : 0);
    return out;
  }
  Bytes round_trip(const Bytes &data) {
    Bytes compressed = this->compress(data, data.size() * 2 + 8);
    bool ok;
    Bytes expanded = this->expand(compressed, data.size(), ok);
    EXPECT_TRUE(ok);
    return expanded;
  }

  Bytes dictionary;
  Lzss lzss;
  std::vector<uint16_t> chains;
};

TEST(Lzss, DictionaryMakesJsonSmaller) {
  std::string reading = WEATHER;
  std::replace(reading.begin(), reading.end(), '2', '5');
  Codec with(JSON_DICTIONARY), without("");
  size_t with_size = with.compress(text(reading), reading.size()).size();
  size_t without_size = without.compress(text(reading), reading.size()).size();
  EXPECT_NE(with_size, 0u);
  EXPECT_LT(with_size, without_size);
  EXPECT_EQ(with.round_trip(text(reading)), text(reading));
}

TEST(Lzss, OutputTooSmallReturnsZero) {
  Codec codec(JSON_DICTIONARY);
  Bytes noise(100);
  std::mt19937 random(1);  // NOLINT(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) reproducible
  for (auto &byte : noise)
    byte = random();
  EXPECT_TRUE(codec.compress(noise, noise.size()).empty());
}

TEST(Lzss, LongMatchesAndOverlap) {
  Codec codec("");
  Bytes run(240, 'x');
  // A literal and one overlapping match
  EXPECT_LE(codec.compress(run, run.size()).size(), 4u);
  EXPECT_EQ(codec.round_trip(run), run);
}

// The length code allows at most MAX_MATCH, so a longer run is split
TEST(Lzss, LongestMatchIsCapped) {
  Codec codec("");
  Bytes run(Lzss::MAX_MATCH * 2 + 10, 'x');
  Bytes compressed = codec.compress(run, run.size());
  ASSERT_FALSE(compressed.empty());
  bool ok;
  EXPECT_EQ(codec.expand(compressed, run.size(), ok), run);
  EXPECT_TRUE(ok);
}

// Any input either expands into the buffer or is refused; nothing is written past it
TEST(Lzss, MalformedDataNeverOverruns) {
  Codec codec(JSON_DICTIONARY);
  std::mt19937 random(3);  // NOLINT(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) reproducible
  Bytes guard(64, 0xA5);
  for (int round = 0; round < 20000; round++) {
    Bytes data(random() % 40);
    for (auto &byte : data)
      byte = random();
    Bytes out(32 + guard.size());
    std::memcpy(out.data() + 32, guard.data(), guard.size());
    size_t length = codec.lzss.expand(data.data(), data.size(), out.data(), 32);
    ASSERT_TRUE(length == SIZE_MAX || length <= 32);
    ASSERT_EQ(std::memcmp(out.data() + 32, guard.data(), guard.size()), 0);
  }
}

TEST(Lzss, RandomRoundTrips) {
  std::mt19937 random(4);  // NOLINT(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) reproducible
  for (const auto *dictionary : {&JSON_DICTIONARY, &WEATHER}) {
    Codec codec(*dictionary);
    for (int round = 0; round < 500; round++) {
      // Mostly pieces of JSON, so there are matches of every length and distance
      Bytes data;
      size_t size = random() % 1000;
      while (data.size() < size) {
        if (random() % 3 == 0) {
          data.push_back(random());
        } else {
          size_t start = random() % WEATHER.size();
          size_t count = std::min<size_t>(1 + random() % 20, WEATHER.size() - start);
          data.insert(data.end(), WEATHER.begin() + start, WEATHER.begin() + start + count);
        }
      }
      data.resize(size);
      ASSERT_EQ(codec.round_trip(data), data);
    }
  }
}

TEST(Lzss, Speed) {
  Codec codec(JSON_DICTIONARY);
  std::string json;
  while (json.size() + WEATHER.size() < 1000)
    json += WEATHER;
  Bytes data = text(json);
  constexpr int rounds = 200;
  Bytes compressed;
  auto start = std::chrono::steady_clock::now();
  for (int round = 0; round < rounds; round++)
    compressed = codec.compress(data, data.size());
  auto middle = std::chrono::steady_clock::now();
  Bytes expanded;
  bool ok = false;
  for (int round = 0; round < rounds; round++)
    expanded = codec.expand(compressed, data.size(), ok);
  auto end = std::chrono::steady_clock::now();
  ASSERT_TRUE(ok);
  ASSERT_EQ(expanded, data);
  auto us = [](auto d) { return std::chrono::duration_cast<std::chrono::microseconds>(d).count() / rounds; };
  std::printf("%zu bytes compressed to %zu: %lld us to compress, %lld us to expand, on this host\n", data.size(),
              compressed.size(), static_cast<long long>(us(middle - start)), static_cast<long long>(us(end - middle)));
  // The slowest case: every position is searched to the end of the window
  Bytes noise(1000);
  std::mt19937 random(5);  // NOLINT(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) reproducible
  for (auto &byte : noise)
    byte = random();
  start = std::chrono::steady_clock::now();
  for (int round = 0; round < rounds; round++)
    codec.compress(noise, noise.size());
  end = std::chrono::steady_clock::now();
  std::printf("%zu bytes of noise: %lld us to find that it does not compress, on this host\n", noise.size(),
              static_cast<long long>(us(end - start)));
}

}  // namespace esphome::lzss::testing
