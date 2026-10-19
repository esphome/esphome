#include <gtest/gtest.h>

#include "../common.h"

namespace esphome::st7305::testing {

class ST7305Test : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(this->display_.init_buffer()); }

  void reset_screen() {
    this->display_.fill(Color::BLACK);
    this->display_.clear_dirty();
  }

  TestableST7305 display_;
};

TEST_F(ST7305Test, FillSetsAllBytes) {
  this->display_.fill(Color::WHITE);
  for (size_t i = 0; i < this->display_.buffer_length(); i++)
    EXPECT_EQ(this->display_.buffer()[i], 0xFF) << "byte " << i;

  this->display_.fill(Color::BLACK);
  for (size_t i = 0; i < this->display_.buffer_length(); i++)
    EXPECT_EQ(this->display_.buffer()[i], 0x00) << "byte " << i;
}

TEST_F(ST7305Test, PixelMapping) {
  struct PixelCase {
    int x;
    int y;
    size_t index;
    uint8_t mask;
  };
  const PixelCase cases[] = {
      {0, 299, 0, 0x80},  {1, 299, 0, 0x40}, {0, 298, 0, 0x20}, {0, 297, 0, 0x08},      {0, 296, 0, 0x02},
      {2, 299, 75, 0x80}, {0, 0, 74, 0x02},  {1, 0, 74, 0x01},  {200, 150, 7537, 0x20}, {399, 0, 14999, 0x01},
  };

  for (const auto &pixel : cases) {
    this->display_.draw_pixel_at(pixel.x, pixel.y, Color::WHITE);
    EXPECT_EQ(this->display_.buffer()[pixel.index], pixel.mask) << "pixel " << pixel.x << "," << pixel.y;
    this->display_.draw_pixel_at(pixel.x, pixel.y, Color::BLACK);
    EXPECT_EQ(this->display_.buffer()[pixel.index], 0x00) << "pixel " << pixel.x << "," << pixel.y;
  }
}

TEST_F(ST7305Test, ClearingOnePixelLeavesNeighbours) {
  this->display_.draw_pixel_at(0, 299, Color::WHITE);
  this->display_.draw_pixel_at(1, 299, Color::WHITE);
  EXPECT_EQ(this->display_.buffer()[0], 0xC0);

  this->display_.draw_pixel_at(0, 299, Color::BLACK);
  EXPECT_EQ(this->display_.buffer()[0], 0x40);
}

TEST_F(ST7305Test, DirtyBoundsFollowChanges) {
  EXPECT_FALSE(this->display_.is_dirty());

  this->display_.draw_pixel_at(10, 20, Color::WHITE);
  EXPECT_TRUE(this->display_.is_dirty());
  EXPECT_EQ(this->display_.x_low(), 10);
  EXPECT_EQ(this->display_.x_high(), 10);
  EXPECT_EQ(this->display_.y_low(), 20);
  EXPECT_EQ(this->display_.y_high(), 20);

  this->display_.clear_dirty();
  this->display_.draw_pixel_at(10, 20, Color::WHITE);
  EXPECT_FALSE(this->display_.is_dirty());

  this->display_.draw_pixel_at(10, 20, Color::BLACK);
  EXPECT_TRUE(this->display_.is_dirty());

  this->display_.clear_dirty();
  this->display_.draw_pixel_at(10, 20, Color::WHITE);
  this->display_.draw_pixel_at(100, 200, Color::WHITE);
  EXPECT_EQ(this->display_.x_low(), 10);
  EXPECT_EQ(this->display_.x_high(), 100);
  EXPECT_EQ(this->display_.y_low(), 20);
  EXPECT_EQ(this->display_.y_high(), 200);
}

TEST_F(ST7305Test, FillOnlyDirtiesChangedBytes) {
  this->display_.clear_dirty();
  this->display_.fill(Color::BLACK);
  EXPECT_FALSE(this->display_.is_dirty());

  this->display_.fill(Color::WHITE);
  EXPECT_TRUE(this->display_.is_dirty());
  EXPECT_EQ(this->display_.x_low(), 0);
  EXPECT_EQ(this->display_.x_high(), 399);
  EXPECT_EQ(this->display_.y_low(), 0);
  EXPECT_EQ(this->display_.y_high(), 299);

  this->display_.clear_dirty();
  this->display_.fill(Color::WHITE);
  EXPECT_FALSE(this->display_.is_dirty());
}

TEST_F(ST7305Test, WindowAlignsToAddressBlocks) {
  this->reset_screen();
  this->display_.draw_pixel_at(0, 299, Color::WHITE);
  this->display_.draw_pixel_at(1, 299, Color::WHITE);

  const auto window = this->display_.compute_window();
  EXPECT_EQ(window.outer_start, 0);
  EXPECT_EQ(window.outer_end, 0);
  EXPECT_EQ(window.block_start, 0);
  EXPECT_EQ(window.block_end, 2);
  EXPECT_EQ(window.caset_start, 0x12);
  EXPECT_EQ(window.caset_end, 0x12);
  EXPECT_EQ(window.data_length(), 3u);

  this->reset_screen();
  this->display_.draw_pixel_at(200, 150, Color::WHITE);

  const auto middle = this->display_.compute_window();
  EXPECT_EQ(middle.outer_start, 100);
  EXPECT_EQ(middle.outer_end, 100);
  EXPECT_EQ(middle.block_start, 36);
  EXPECT_EQ(middle.block_end, 38);
  EXPECT_EQ(middle.caset_start, 0x1E);
  EXPECT_EQ(middle.caset_end, 0x1E);
  EXPECT_EQ(middle.data_length(), 3u);

  this->reset_screen();
  this->display_.draw_pixel_at(0, 0, Color::WHITE);
  this->display_.draw_pixel_at(0, 299, Color::WHITE);

  const auto full_height = this->display_.compute_window();
  EXPECT_EQ(full_height.outer_start, 0);
  EXPECT_EQ(full_height.outer_end, 0);
  EXPECT_EQ(full_height.block_start, 0);
  EXPECT_EQ(full_height.block_end, 74);
  EXPECT_EQ(full_height.caset_start, 0x12);
  EXPECT_EQ(full_height.caset_end, 0x2A);
  EXPECT_EQ(full_height.data_length(), 75u);
}

TEST_F(ST7305Test, WriteWindowEmitsWindowCommands) {
  for (size_t i = 0; i < this->display_.buffer_length(); i++)
    this->display_.buffer()[i] = static_cast<uint8_t>(i & 0xFF);

  RecordingPin dc_pin;
  RecordingDelegate delegate(&dc_pin);
  this->display_.attach_spi(&delegate, &dc_pin);

  this->display_.draw_pixel_at(200, 151, Color::WHITE);
  this->display_.write_window();

  const uint8_t expected[] = {
      0x2A,
      0x1E,
      0x1E,
      0x2B,
      100,
      100,
      0x2C,
      this->display_.buffer()[100 * 75 + 36],
      this->display_.buffer()[100 * 75 + 37],
      this->display_.buffer()[100 * 75 + 38],
  };
  ASSERT_EQ(delegate.bytes.size(), sizeof(expected));
  for (size_t i = 0; i < sizeof(expected); i++)
    EXPECT_EQ(delegate.bytes[i], expected[i]) << "byte " << i;

  ASSERT_EQ(delegate.chunks.size(), 6u);
  EXPECT_EQ(delegate.chunks[0], std::make_pair(false, size_t{1}));
  EXPECT_EQ(delegate.chunks[1], std::make_pair(true, size_t{2}));
  EXPECT_EQ(delegate.chunks[2], std::make_pair(false, size_t{1}));
  EXPECT_EQ(delegate.chunks[3], std::make_pair(true, size_t{2}));
  EXPECT_EQ(delegate.chunks[4], std::make_pair(false, size_t{1}));
  EXPECT_EQ(delegate.chunks[5], std::make_pair(true, size_t{3}));
}

TEST_F(ST7305Test, PortraitPixelMapping) {
  TestableST7305 portrait(200, 200, Orientation::ORIENTATION_PORTRAIT, 0x13, 0x25, 0x00, 0x63);
  ASSERT_TRUE(portrait.init_buffer());
  ASSERT_EQ(portrait.get_width(), 200);
  ASSERT_EQ(portrait.get_height(), 200);

  struct PixelCase {
    int x;
    int y;
    size_t index;
    uint8_t mask;
  };
  const PixelCase cases[] = {
      {0, 0, 0, 0x80}, {1, 0, 0, 0x20}, {2, 0, 0, 0x08},  {3, 0, 0, 0x02},
      {0, 1, 0, 0x40}, {4, 0, 1, 0x80}, {0, 2, 50, 0x80}, {199, 199, 4999, 0x01},
  };

  for (const auto &pixel : cases) {
    portrait.draw_pixel_at(pixel.x, pixel.y, Color::WHITE);
    EXPECT_EQ(portrait.buffer()[pixel.index], pixel.mask) << "pixel " << pixel.x << "," << pixel.y;
    portrait.draw_pixel_at(pixel.x, pixel.y, Color::BLACK);
    EXPECT_EQ(portrait.buffer()[pixel.index], 0x00) << "pixel " << pixel.x << "," << pixel.y;
  }
}

TEST_F(ST7305Test, PortraitWindowAlignsToAddressBlocks) {
  TestableST7305 portrait(200, 200, Orientation::ORIENTATION_PORTRAIT, 0x13, 0x25, 0x00, 0x63);
  ASSERT_TRUE(portrait.init_buffer());

  portrait.draw_pixel_at(100, 100, Color::WHITE);
  const auto window = portrait.compute_window();
  EXPECT_EQ(window.outer_start, 50);
  EXPECT_EQ(window.outer_end, 50);
  EXPECT_EQ(window.stride, 50);
  EXPECT_EQ(window.block_start, 24);
  EXPECT_EQ(window.block_end, 26);
  EXPECT_EQ(window.caset_start, 0x1B);
  EXPECT_EQ(window.caset_end, 0x1B);
  EXPECT_EQ(window.data_length(), 3u);

  portrait.fill(Color::BLACK);
  portrait.clear_dirty();
  portrait.draw_pixel_at(0, 0, Color::WHITE);
  portrait.draw_pixel_at(3, 1, Color::WHITE);
  const auto corner = portrait.compute_window();
  EXPECT_EQ(corner.outer_start, 0);
  EXPECT_EQ(corner.outer_end, 0);
  EXPECT_EQ(corner.block_start, 0);
  EXPECT_EQ(corner.block_end, 2);
  EXPECT_EQ(corner.caset_start, 0x13);
  EXPECT_EQ(corner.caset_end, 0x13);
  EXPECT_EQ(corner.data_length(), 3u);
}

}  // namespace esphome::st7305::testing
