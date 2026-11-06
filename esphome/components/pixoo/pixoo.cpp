#include "pixoo.h"
#include "esphome/components/display/display_color_utils.h"

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include <cmath>
#include <cstring>
#include <utility>

namespace esphome::pixoo {

ESPHOME_LOG_TAG(TAG, "pixoo");

// Divoom LED-board packet protocol.
static constexpr uint8_t PACKET_HEAD = 0xAA;
static constexpr uint8_t PACKET_TAIL = 0xBB;
static constexpr uint8_t CMD_DATA = 0x00;
static constexpr uint8_t CMD_LIGHT = 0x01;
static constexpr uint8_t CMD_UNUSED = 0x21;
static constexpr uint8_t CMD_SET_RGB_IOUT = 0x22;
static constexpr size_t PACKET_HEADER_LEN = 4;  // head + len(2) + cmd
static constexpr uint8_t DEFAULT_IOUT = 75;     // per-channel LED current / white balance default

// Pack a `0xAA len cmd data 0xBB` packet into buf; returns the packet length.
static inline size_t build_packet(uint8_t *buf, uint8_t cmd, const uint8_t *data, uint16_t len) {
  buf[0] = PACKET_HEAD;
  put_le16(buf + 1, len);
  buf[3] = cmd;
  if (data != nullptr && len > 0)
    std::memcpy(buf + PACKET_HEADER_LEN, data, len);
  buf[PACKET_HEADER_LEN + len] = PACKET_TAIL;
  return len + PACKET_STATIC_LEN;
}

// Fill `total` bytes at buf with a single UNUSED padding packet.
static inline void pad_unused(uint8_t *buf, size_t total) {
  buf[0] = PACKET_HEAD;
  put_le16(buf + 1, total - PACKET_STATIC_LEN);
  buf[3] = CMD_UNUSED;
  buf[total - 1] = PACKET_TAIL;
}

float Pixoo::get_setup_priority() const { return setup_priority::PROCESSOR; }

void Pixoo::setup() {
  const size_t image_bytes = data_size(this->model_);
  if (!this->setup_check(this->buffer_.init(image_bytes), LOG_STR(ESP_LOG_MSG_ALLOC_FAIL)))
    return;

  // Pre-build the constant DATA-packet framing; only the RGB888 payload changes per frame.
  this->frame_buffer_[0] = PACKET_HEAD;
  put_le16(this->frame_buffer_ + 1, image_bytes);
  this->frame_buffer_[3] = CMD_DATA;
  this->frame_buffer_[PACKET_HEADER_LEN + image_bytes] = PACKET_TAIL;
  pad_unused(this->frame_buffer_ + image_bytes + PACKET_STATIC_LEN, DMA_CHUNK);

  this->spi_setup();

  this->buffer_.fill(0x00);

  // Set the per-channel LED current. Brightness is controlled separately via the light platform.
  const uint8_t iout[3] = {DEFAULT_IOUT, DEFAULT_IOUT, DEFAULT_IOUT};
  this->send_command_(CMD_SET_RGB_IOUT, iout, 3);

  // Frames are pushed synchronously inside update(), so there is no loop() work to do and the
  // component is idle between updates. Marking it done (LOOP_DONE) lets LVGL's
  // update_when_display_idle option treat the panel as idle and drive frames on demand.
  this->disable_loop();
}

void Pixoo::send_command_(uint8_t cmd, const uint8_t *data, uint16_t len) {
  std::memset(this->cmd_buffer_, 0, DMA_CHUNK);
  const size_t used = build_packet(this->cmd_buffer_, cmd, data, len);
  if (DMA_CHUNK - used >= PACKET_STATIC_LEN)
    pad_unused(this->cmd_buffer_ + used, DMA_CHUNK - used);
  this->enable();
  this->write_array(this->cmd_buffer_, DMA_CHUNK);
  this->disable();
}

void Pixoo::set_panel_brightness(float brightness) {
  const uint8_t pct = static_cast<uint8_t>(lroundf(clamp(brightness, 0.0f, 1.0f) * 100.0f));
  this->send_command_(CMD_LIGHT, &pct, 1);
}

void Pixoo::update() {
  this->do_update_();
  const size_t image_bytes = data_size(this->model_);
  for (size_t i = 0; i < image_bytes; i++)
    this->frame_buffer_[PACKET_HEADER_LEN + i] = this->buffer_[i];
  this->enable();
  this->write_array(this->frame_buffer_, frame_size(this->model_));
  this->disable();
}

void Pixoo::set_pixel_(uint32_t index, Color color) {
  const size_t off = static_cast<size_t>(index) * 3;
  this->buffer_[off] = color.r;
  this->buffer_[off + 1] = color.g;
  this->buffer_[off + 2] = color.b;
}

void HOT Pixoo::draw_pixel_at(int x, int y, Color color) {
  if (this->is_point_clipped(x, y))
    return;
  const int side = static_cast<int>(this->model_);
  switch (this->rotation_) {
    case display::DISPLAY_ROTATION_0_DEGREES:
      break;
    case display::DISPLAY_ROTATION_90_DEGREES:
      std::swap(x, y);
      x = side - x - 1;
      break;
    case display::DISPLAY_ROTATION_180_DEGREES:
      x = side - x - 1;
      y = side - y - 1;
      break;
    case display::DISPLAY_ROTATION_270_DEGREES:
      std::swap(x, y);
      y = side - y - 1;
      break;
  }
  if (x < 0 || x >= side || y < 0 || y >= side)
    return;
  this->set_pixel_(static_cast<uint32_t>(y) * side + x, color);
}

// flatten keeps the RGB565 decode inlined in the pixel loop, as in Display's own per pixel loop.
void __attribute__((flatten)) Pixoo::draw_pixels_internal(int x_start, int y_start, int w, int h, const uint8_t *ptr,
                                                          display::ColorBitness bitness, bool big_endian, int x_offset,
                                                          int y_offset, int x_pad) {
  // Fast path for the common LVGL/image blit: RGB565, no rotation, no active clipping.
  // Anything else defers to the base implementation, which decodes per pixel and routes through
  // draw_pixel_at() so rotation, clipping and other color formats stay correct.
  if (bitness != display::COLOR_BITNESS_565 || this->rotation_ != display::DISPLAY_ROTATION_0_DEGREES ||
      this->is_clipping()) {
    display::Display::draw_pixels_internal(x_start, y_start, w, h, ptr, bitness, big_endian, x_offset, y_offset, x_pad);
    return;
  }
  const int side = static_cast<int>(this->model_);
  const size_t line_stride = static_cast<size_t>(x_offset) + w + x_pad;
  for (int y = 0; y != h; y++) {
    const int dst_y = y_start + y;
    if (dst_y < 0 || dst_y >= side)
      continue;
    size_t source_idx = (static_cast<size_t>(y_offset) + y) * line_stride + x_offset;
    for (int x = 0; x != w; x++, source_idx++) {
      const int dst_x = x_start + x;
      if (dst_x < 0 || dst_x >= side)
        continue;
      this->set_pixel_(static_cast<uint32_t>(dst_y) * side + dst_x,
                       display::ColorUtil::to_color(
                           display::ColorUtil::read_packed<display::COLOR_BITNESS_565>(ptr, source_idx, big_endian),
                           display::COLOR_BITNESS_565));
    }
  }
}

void Pixoo::fill(Color color) {
  if (this->is_clipping()) {
    display::Display::fill(color);
    return;
  }
  const size_t image_bytes = data_size(this->model_);
  for (size_t i = 0; i < image_bytes; i += 3) {
    this->buffer_[i] = color.r;
    this->buffer_[i + 1] = color.g;
    this->buffer_[i + 2] = color.b;
  }
}

void Pixoo::dump_config() {
  LOG_DISPLAY("", "Divoom Pixoo", this);
  ESP_LOGCONFIG(TAG, "  Model: %ux%u", (unsigned) this->model_, (unsigned) this->model_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::pixoo
