#include <gtest/gtest.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "esphome/components/runtime_image/qoi_decoder.h"
#include "esphome/components/runtime_image/runtime_image.h"

namespace esphome::runtime_image::testing {

// Big enough that decoding it takes well over DECODE_TIME_LIMIT_MS, little enough to build quickly
static constexpr uint32_t WIDTH = 4096;
static constexpr uint32_t HEIGHT = 2048;

static void put_le32(std::vector<uint8_t> &data, size_t offset, uint32_t value) {
  for (size_t i = 0; i < 4; i++)
    data[offset + i] = (value >> (8 * i)) & 0xFF;
}

static void put_be32(std::vector<uint8_t> &data, size_t offset, uint32_t value) {
  for (size_t i = 0; i < 4; i++)
    data[offset + i] = (value >> (24 - 8 * i)) & 0xFF;
}

// A 1-bit BMP with the left half of every row set: eight pixels for each byte read
static std::vector<uint8_t> make_mono_bmp() {
  constexpr size_t data_offset = 14 + 40 + 2 * 4;
  constexpr size_t row_bytes = WIDTH / 8;
  std::vector<uint8_t> bmp(data_offset + row_bytes * HEIGHT, 0);
  bmp[0] = 'B';
  bmp[1] = 'M';
  put_le32(bmp, 2, bmp.size());
  put_le32(bmp, 10, data_offset);
  put_le32(bmp, 14, 40);  // DIB header size
  put_le32(bmp, 18, WIDTH);
  put_le32(bmp, 22, HEIGHT);
  bmp[26] = 1;  // color planes
  bmp[28] = 1;  // bits per pixel
  for (size_t row = 0; row < HEIGHT; row++) {
    for (size_t i = 0; i < row_bytes / 2; i++)
      bmp[data_offset + row * row_bytes + i] = 0xFF;
  }
  return bmp;
}

// A QOI image of one color: an RGB chunk, then runs of 62 pixels, each run a single byte
static std::vector<uint8_t> make_solid_qoi() {
  std::vector<uint8_t> qoi(14, 0);
  qoi[0] = 'q';
  qoi[1] = 'o';
  qoi[2] = 'i';
  qoi[3] = 'f';
  put_be32(qoi, 4, WIDTH);
  put_be32(qoi, 8, HEIGHT);
  qoi[12] = 3;  // channels
  qoi.insert(qoi.end(), {0xFE, 30, 120, 200});
  for (size_t left = WIDTH * HEIGHT - 1; left > 0;) {
    const size_t run = std::min<size_t>(left, 62);
    qoi.push_back(0xC0 | (run - 1));
    left -= run;
  }
  qoi.insert(qoi.end(), {0, 0, 0, 0, 0, 0, 0, 1});
  return qoi;
}

// Feed the whole image as often as it takes, and return how many calls that was
static int decode_in_calls(RuntimeImage &img, std::vector<uint8_t> &data) {
  EXPECT_TRUE(img.begin_decode(data.size()));
  size_t offset = 0;
  int calls = 0;
  while (offset < data.size() && calls < 10000) {
    const int consumed = img.feed_data(data.data() + offset, data.size() - offset);
    EXPECT_GT(consumed, 0);
    if (consumed <= 0)
      break;
    offset += consumed;
    calls++;
  }
  EXPECT_EQ(offset, data.size());
  EXPECT_TRUE(img.end_decode());
  return calls;
}

TEST(DecodeTimeLimit, BmpIsDecodedOverSeveralCalls) {
  RuntimeImage img(BMP, image::IMAGE_TYPE_GRAYSCALE, image::TRANSPARENCY_OPAQUE);
  auto data = make_mono_bmp();
  EXPECT_GT(decode_in_calls(img, data), 1) << "the whole image was decoded in one call";
  ASSERT_EQ(img.get_width(), static_cast<int>(WIDTH));
  ASSERT_EQ(img.get_height(), static_cast<int>(HEIGHT));
  for (auto [col, row] : {std::pair{0, 0}, std::pair{1000, 1000}, std::pair{4095, 2047}}) {
    const uint8_t expected = col < static_cast<int>(WIDTH / 2) ? 255 : 0;
    EXPECT_EQ(img.get_pixel(col, row).r, expected) << col << "," << row;
  }
}

TEST(DecodeTimeLimit, QoiIsDecodedOverSeveralCalls) {
  RuntimeImage img(QOI, image::IMAGE_TYPE_RGB, image::TRANSPARENCY_OPAQUE);
  auto data = make_solid_qoi();
  EXPECT_GT(decode_in_calls(img, data), 1) << "the whole image was decoded in one call";
  for (auto [col, row] : {std::pair{0, 0}, std::pair{2000, 1000}, std::pair{4095, 2047}}) {
    const Color pixel = img.get_pixel(col, row);
    EXPECT_EQ(pixel.r, 30);
    EXPECT_EQ(pixel.g, 120);
    EXPECT_EQ(pixel.b, 200);
  }
}

// A chunk split across two calls: the bytes used before the split count towards the decoded total
TEST(DecodeTimeLimit, QoiCountsTheBytesBeforeASplitChunk) {
  RuntimeImage img(QOI, image::IMAGE_TYPE_RGB, image::TRANSPARENCY_OPAQUE);
  std::vector<uint8_t> data = {'q', 'o', 'i', 'f', 0, 0, 0, 2, 0, 0, 0, 1, 3, 0};
  data.insert(data.end(), {0xFE, 10, 20, 30, 0xFE, 40, 50, 60, 0, 0, 0, 0, 0, 0, 0, 1});
  QoiDecoder decoder(&img);
  ASSERT_EQ(decoder.prepare(data.size()), 0);

  // The header and the first chunk, and half of the second
  EXPECT_EQ(decoder.decode(data.data(), 20), 18);
  EXPECT_EQ(decoder.get_decoded_bytes(), 18u);
  EXPECT_EQ(decoder.decode(data.data() + 18, data.size() - 18), static_cast<int>(data.size() - 18));
  EXPECT_EQ(decoder.get_decoded_bytes(), data.size());
  EXPECT_TRUE(decoder.is_finished());
}

}  // namespace esphome::runtime_image::testing
