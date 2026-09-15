#include "epaper_spi_gray4.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.gray4";

// The 5- and 6-bit channels of an RGB565 pixel expanded to eight bits, exactly as ColorUtil::to_color does it, so
// that the batched path in draw_pixels_at() arrives at the same level as the per-pixel one.
template<size_t N> static constexpr std::array<uint8_t, N> make_scale() {
  std::array<uint8_t, N> table{};
  for (size_t i = 0; i != N; i++)
    table[i] = (uint8_t) (255 * i / (N - 1));
  return table;
}
static constexpr std::array<uint8_t, 32> SCALE_5 = make_scale<32>();
static constexpr std::array<uint8_t, 64> SCALE_6 = make_scale<64>();

// For a buffer byte - four 2-bit levels, most significant pixel first - the HIGH bit of each of those four levels
// gathered into bits 7..4, and the LOW bit of each into bits 3..0. A plane byte covers eight pixels, so two lookups
// build one, in place of the eight per-pixel buffer reads it used to cost.
static constexpr std::array<uint8_t, 256> make_plane_nibbles() {
  std::array<uint8_t, 256> table{};
  for (size_t value = 0; value != table.size(); value++) {
    uint8_t high = 0;
    uint8_t low = 0;
    for (unsigned pixel = 0; pixel != 4; pixel++) {
      const uint8_t level = (uint8_t) ((value >> (6 - 2 * pixel)) & 0x03);
      high = (uint8_t) (high | ((level >> 1) << (3 - pixel)));
      low = (uint8_t) (low | ((level & 1) << (3 - pixel)));
    }
    table[value] = (uint8_t) ((high << 4) | low);
  }
  return table;
}
static constexpr std::array<uint8_t, 256> PLANE_NIBBLES = make_plane_nibbles();

void EPaperGray4::setup() {
  EPaperBase::setup();
  this->init_shadow_();
}

// The comparison frame is only wanted where partial refresh is: with
// full_update_every at 1 every push is a full one and nothing ever diffs
// against it. The first push is always full - the counter starts at zero - so
// the frame is seeded before any partial can need it.
void EPaperGray4::init_shadow_() {
  if (!this->is_using_partial_update_())
    return;
  if (!this->shadow_.init((size_t) ((this->width_ + 7) / 8) * this->height_)) {
    ESP_LOGW(TAG, "No memory for a comparison frame; every update will be a full refresh");
  }
}

// Luminance into four even quarters. A renderer that antialiases - LVGL
// composites at 16-bit - delivers glyph edges here as real intermediate
// values, and this is where they survive instead of being thresholded.
uint8_t EPaperGray4::level_from_sum(uint16_t sum) {  // sum of the three channels, 0..765
  if (sum >= 574)
    return 3;  // white
  if (sum >= 383)
    return 2;  // light gray
  if (sum >= 192)
    return 1;  // dark gray
  return 0;    // black
}

uint8_t EPaperGray4::color_to_level_(Color color) const {
  return level_from_sum((uint16_t) color.r + color.g + color.b);
}

void EPaperGray4::fill(Color color) {
  if (this->get_clipping().is_set()) {
    EPaperBase::fill(color);  // clipping active: defers to the per-pixel path
    return;
  }
  this->buffer_.fill(this->color_to_level_(color) * 0x55);  // same value in all four slots
  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->width_;
  this->y_high_ = this->height_;
}

void HOT EPaperGray4::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;
  const size_t byte_position = (size_t) y * this->row_width_ + x / 4;
  const uint8_t shift = 6 - 2 * (x % 4);  // most significant pixel first
  uint8_t value = this->buffer_[byte_position];
  value = (value & ~(0x03 << shift)) | (this->color_to_level_(color) << shift);
  this->buffer_[byte_position] = value;
}

// A renderer hands over a whole rectangle, and the generic implementation spends a virtual call, a colour
// conversion, a clipping test, four bounds clamps and two SplitBuffer lookups on every pixel of it. This does the
// same work per RECTANGLE instead, and walks the PANEL row by row rather than the source: rotate_coordinates_() is
// affine - panel x comes from one logical axis and panel y from the other, each monotonically - so on a display
// mounted at 90 degrees a source-major walk lands every consecutive pixel row_width_ bytes apart in the buffer,
// which is a cache miss each. Panel-major, four pixels share a byte and the writes run in order.
void HOT EPaperGray4::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, ColorOrder order,
                                     ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) {
  // Worth a fast path only for what a renderer actually sends: 16-bit pixels, and no clipping window to honour. The
  // colour ORDER needs no case of its own - a luminance is the sum of the three channels, in whatever order they sit.
  if (bitness != COLOR_BITNESS_565 || this->get_clipping().is_set()) {
    Display::draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset, y_offset, x_pad);
    return;
  }

  const bool swap_xy = (this->effective_transform_ & SWAP_XY) != 0;
  const bool mirror_x = (this->effective_transform_ & MIRROR_X) != 0;
  const bool mirror_y = (this->effective_transform_ & MIRROR_Y) != 0;

  // `a` walks the logical axis that becomes panel x, `b` the one that becomes panel y.
  const int a_start = swap_xy ? y_start : x_start;
  const int b_start = swap_xy ? x_start : y_start;
  // Clip once, to the same bounds rotate_coordinates_() tests per pixel.
  const int a_lo = std::max(0, -a_start);
  const int a_hi = std::min(swap_xy ? h : w, (int) this->width_ - a_start);
  const int b_lo = std::max(0, -b_start);
  const int b_hi = std::min(swap_xy ? w : h, (int) this->height_ - b_start);
  if (a_lo >= a_hi || b_lo >= b_hi)
    return;

  // Within one panel row only one logical axis moves, so the source index advances by a fixed stride.
  const size_t line_stride = (size_t) (x_offset + w + x_pad);
  const size_t src_step = swap_xy ? line_stride : 1;
  const size_t src_row_step = swap_xy ? 1 : line_stride;
  const size_t src_base = (size_t) y_offset * line_stride + (size_t) x_offset;

  const int px_step = mirror_x ? -1 : 1;
  for (int b = b_lo; b != b_hi; b++) {
    const int py = mirror_y ? this->height_ - 1 - (b_start + b) : b_start + b;
    const size_t row_start = (size_t) py * this->row_width_;

    const uint8_t *src = ptr + 2 * (src_base + (size_t) b * src_row_step + (size_t) a_lo * src_step);
    int px = mirror_x ? this->width_ - 1 - (a_start + a_lo) : a_start + a_lo;
    for (int a = a_lo; a != a_hi; a++, px += px_step, src += 2 * src_step) {
      const uint16_t value = big_endian ? (uint16_t) ((src[0] << 8) | src[1]) : (uint16_t) (src[0] | (src[1] << 8));
      const uint8_t level =
          level_from_sum(SCALE_5[(value >> 11) & 0x1F] + SCALE_6[(value >> 5) & 0x3F] + SCALE_5[value & 0x1F]);
      const uint8_t shift = 6 - 2 * (px & 3);  // most significant pixel first
      uint8_t &cell = this->buffer_[row_start + (px >> 2)];
      cell = (uint8_t) ((cell & ~(0x03 << shift)) | (level << shift));
    }
  }

  // The bounds of what was touched, once for the rectangle rather than once per pixel.
  const int px_lo = mirror_x ? this->width_ - (a_start + a_hi) : a_start + a_lo;
  const int py_lo = mirror_y ? this->height_ - (b_start + b_hi) : b_start + b_lo;
  this->x_low_ = clamp_at_most(this->x_low_, px_lo);
  this->x_high_ = clamp_at_least(this->x_high_, px_lo + (a_hi - a_lo));
  this->y_low_ = clamp_at_most(this->y_low_, py_lo);
  this->y_high_ = clamp_at_least(this->y_high_, py_lo + (b_hi - b_lo));
}

bool EPaperGray4::reset() {
  if (EPaperBase::reset()) {
    // A software reset drops controller RAM, which a partial needs to keep.
    // A full push rewrites both planes anyway, so it can afford one.
    if (this->update_count_ == 0)
      this->command(0x12);
    return true;
  }
  return false;
}

void EPaperGray4::set_window_() {
  // Every push covers the whole panel: a four-level refresh drives all of
  // it from both planes, and a partial is panel-wide too - master
  // activation reads all of 0x24, a RAM window only scopes the write.
  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->width_;
  this->y_high_ = this->height_;
  this->cmd_data(0x44, {0x00, 0x00, (uint8_t) ((this->width_ - 1) & 0xFF), (uint8_t) ((this->width_ - 1) >> 8)});
  this->cmd_data(0x4E, {0x00, 0x00});
  this->cmd_data(0x45, {0x00, 0x00, (uint8_t) ((this->height_ - 1) & 0xFF), (uint8_t) ((this->height_ - 1) >> 8)});
  this->cmd_data(0x4F, {0x00, 0x00});
}

// Both kinds of push write two planes; what differs is what goes in them.
//
//   FULL     pass 0 -> new RAM = high bit of the level
//            pass 1 -> old RAM = low bit, both inverted where the panel
//                      reads 1 as white
//   PARTIAL  pass 0 -> old RAM = the frame on the glass (the comparison)
//            pass 1 -> new RAM = the frame we want
//
// In both, the SECOND pass records what the push is putting on the glass.
// Doing it in the first would overwrite the shadow the second still has to
// read, leaving both planes identical - and then every cleared pixel is
// undriven and the old image stays underneath the new one.
bool HOT EPaperGray4::transfer_data() {
  auto start_time = millis();
  const size_t row_length = this->width_ / 8;
  const bool second_pass = this->plane_ == 1;
  if (this->current_data_index_ == 0) {
    if (!second_pass) {
      // Latch the kind of push for its whole duration: the two planes must
      // agree, and refresh_screen() must match what was written.
      this->partial_push_ = this->update_count_ != 0 && this->shadow_.is_valid();
      this->set_window_();
    }
    this->command(this->plane_command(this->partial_push_ == second_pass));
  }
  // Which bit of each level this plane carries. The low one belongs to the second pass of a FULL push and nowhere
  // else: a partial's new frame is "level >= 2", which is the high bit again.
  const bool low_bits = !this->partial_push_ && second_pass;
  const bool invert = !this->partial_push_ && this->gray_planes_inverted();
  const bool reads_buffer = !this->partial_push_ || second_pass;
  uint8_t row[128];
  this->start_data_();
  while (this->current_data_index_ != this->height_) {
    const int y = (int) this->current_data_index_;
    // Two buffer bytes hold the eight pixels of one plane byte, so a plane row spans 2 * row_length of them.
    const size_t src_row = (size_t) y * this->row_width_;
    for (size_t b = 0; b != row_length; b++) {
      const size_t shadow_index = (size_t) y * row_length + b;
      uint8_t out, mono = 0;
      if (!reads_buffer) {
        out = this->shadow_[shadow_index];  // what is on the glass
      } else {
        const uint8_t first = PLANE_NIBBLES[this->buffer_[src_row + 2 * b]];
        const uint8_t second = PLANE_NIBBLES[this->buffer_[src_row + 2 * b + 1]];
        mono = (uint8_t) ((first & 0xF0) | (second >> 4));  // the high bit of every level: "level >= 2"
        out = low_bits ? (uint8_t) (((first & 0x0F) << 4) | (second & 0x0F)) : mono;
        if (invert)
          out = (uint8_t) ~out;
      }
      row[b] = out;
      if (second_pass && this->shadow_.is_valid())
        this->shadow_[shadow_index] = mono;
    }
    ++this->current_data_index_;
    this->write_array(row, row_length);
    if (millis() - start_time > MAX_TRANSFER_TIME) {
      this->disable();
      return false;  // come back next loop
    }
  }
  this->disable();
  this->current_data_index_ = 0;
  if (!second_pass) {
    this->plane_ = 1;
    return false;  // second plane next time round
  }
  this->plane_ = 0;
  return true;
}

// The base class offers its own full/partial opinion from a counter, but the
// planes were written against partial_push_, latched when the push began -
// it can differ, because a partial is downgraded when there is no comparison
// frame yet. The waveform has to match what was actually written.
void EPaperGray4::refresh_screen(bool partial) {
  if (this->partial_push_) {
    ESP_LOGV(TAG, "Partial refresh");
    this->refresh_partial();
  } else {
    ESP_LOGV(TAG, "Four-level refresh");
    this->refresh_gray();
  }
}

void EPaperGray4::refresh_partial() {
  this->cmd_data(0x22, {0xFF});  // OTP display mode 2, with temperature
  this->command(0x20);           // master activation
}

// The panel sleeps after every push, as it does for a monochrome display -
// it wakes on the reset the next push performs anyway, so this costs nothing
// and saves its idle draw for however long the host stays up.
//
// Mode 1 keeps RAM across the sleep and that reset. This class writes both
// planes every time and so does not depend on it, but the deeper mode buys
// nothing measurable while the host is awake, and mode 1 is the one with
// hardware evidence behind it on this family.
//
// Mode 2 is for the host going to sleep: the rail is usually cut behind it,
// so there is nothing to preserve and no wake to pay for.
void EPaperGray4::deep_sleep() {
  if (this->sleep_panel_deep_) {
    this->sleep_panel_deep_ = false;
    ESP_LOGV(TAG, "Panel deep sleep, mode 2");
    this->cmd_data(0x10, {0x03});
    return;
  }
  ESP_LOGV(TAG, "Panel deep sleep, mode 1");
  this->cmd_data(0x10, {0x01});
}

// Vendor waveform selection: 0xD7 is not a named row of the datasheet's
// Table 7-1. Seeed's dashboard driver and the stock firmware both send it,
// preceded by the forced OTP temperature.
void EPaperStickyGray4::refresh_gray() {
  this->cmd_data(0x1A, {0x67, 0x00});  // force temperature by OTP
  this->cmd_data(0x22, {0xD7});        // four-level update sequence
  this->command(0x20);                 // master activation
}

}  // namespace esphome::epaper_spi
