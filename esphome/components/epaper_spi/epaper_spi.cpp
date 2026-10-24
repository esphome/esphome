#include "epaper_spi.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

ESPHOME_LOG_TAG(TAG, "epaper_spi");
static constexpr size_t EPAPER_MAX_CMD_LOG_BYTES = 128;

static constexpr const char *const EPAPER_STATE_STRINGS[] = {
    "IDLE",          "UPDATE",   "RESET",          "RESET_END", "SHOULD_WAIT", "INITIALISE",
    "TRANSFER_DATA", "POWER_ON", "REFRESH_SCREEN", "POWER_OFF", "DEEP_SLEEP",
};

const char *EPaperBase::epaper_state_to_string_() {
  if (auto idx = static_cast<unsigned>(this->state_); idx < std::size(EPAPER_STATE_STRINGS))
    return EPAPER_STATE_STRINGS[idx];
  return "Unknown";
}

void EPaperBase::setup() {
  if (!this->init_buffer_(this->buffer_length_)) {
    this->mark_failed(LOG_STR("Failed to initialise buffer"));
    return;
  }
  this->setup_pins_();
  this->spi_setup();
}

bool EPaperBase::init_buffer_(size_t buffer_length) {
  if (!this->buffer_.init(buffer_length)) {
    return false;
  }
  this->clear();
  return true;
}

bool EPaperBase::init_sent_frame_(size_t length) {
  if (!this->is_using_partial_update_())
    return false;
  if (this->sent_.init(length))
    return true;
  ESP_LOGW(TAG, "No memory for the previous frame; partial updates will send the whole drawn area");
  return false;
}

uint32_t EPaperBase::buffer_hash_() const {
  uint32_t hash = FNV1_OFFSET_BASIS;
  size_t index = 0;
  while (index != this->buffer_.size()) {
    size_t length;
    const uint8_t *data = this->buffer_.get_span(index, length);
    for (size_t i = 0; i != length; i++)
      hash = fnv1a_hash_extend(hash, data[i]);
    index += length;
  }
  return hash;
}

void EPaperBase::set_full_bounds_() {
  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->width_;
  this->y_high_ = this->height_;
}

void EPaperBase::reset_bounds_() {
  this->x_low_ = this->width_;
  this->x_high_ = 0;
  this->y_low_ = this->height_;
  this->y_high_ = 0;
}

void EPaperBase::setup_pins_() const {
  for (auto *pin : this->enable_pins_) {
    pin->setup();
    pin->digital_write(true);
  }
  this->dc_pin_->setup();  // OUTPUT
  this->dc_pin_->digital_write(false);

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();  // OUTPUT
    this->reset_pin_->digital_write(true);
  }

  if (this->busy_pin_ != nullptr) {
    this->busy_pin_->setup();  // INPUT
  }
}

float EPaperBase::get_setup_priority() const { return setup_priority::PROCESSOR; }

void EPaperBase::command(uint8_t value) {
  ESP_LOGV(TAG, "Command: 0x%02X", value);
  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(value);
  this->disable();
}

// write a command followed by zero or more bytes of data.
void EPaperBase::cmd_data(uint8_t command, const uint8_t *ptr, size_t length) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex_buf[format_hex_pretty_size(EPAPER_MAX_CMD_LOG_BYTES)];
  ESP_LOGV(TAG, "Command: 0x%02X, Length: %d, Data: %s", command, length,
           format_hex_pretty_to(hex_buf, ptr, length, '.'));
#endif

  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(command);
  if (length > 0) {
    this->dc_pin_->digital_write(true);
    this->write_array(ptr, length);
  }
  this->disable();
}

bool EPaperBase::is_idle_() const {
  if (this->busy_pin_ == nullptr) {
    return true;
  }
  return !this->busy_pin_->digital_read();
}

bool EPaperBase::reset() { return this->hardware_reset_(); }

bool EPaperBase::hardware_reset_() {
  if (this->reset_pin_ != nullptr) {
    if (this->state_ == EPaperState::RESET) {
      this->reset_pin_->digital_write(false);
      return false;
    }
    this->reset_pin_->digital_write(true);
  }
  return true;
}

void EPaperBase::update_effective_transform_() {
  switch (this->rotation_) {
    case DISPLAY_ROTATION_90_DEGREES:
      this->effective_transform_ = this->transform_ ^ (SWAP_XY | MIRROR_X);
      break;
    case DISPLAY_ROTATION_180_DEGREES:
      this->effective_transform_ = this->transform_ ^ (MIRROR_Y | MIRROR_X);
      break;
    case DISPLAY_ROTATION_270_DEGREES:
      this->effective_transform_ = this->transform_ ^ (SWAP_XY | MIRROR_Y);
      break;
    default:
      this->effective_transform_ = this->transform_;
      break;
  }
}

void EPaperBase::update() {
  if (this->state_ != EPaperState::IDLE) {
    ESP_LOGE(TAG, "Display already in state %s", epaper_state_to_string_());
    return;
  }
  this->set_state_(EPaperState::UPDATE);
  this->enable_loop();
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
  this->update_start_time_ = millis();
#endif
}

void EPaperBase::wait_for_idle_(bool should_wait) {
  // The wait starts when the step's delay ends, or now for a wait in the middle of a step
  const uint32_t now = millis();
  this->waiting_for_idle_start_ = static_cast<int32_t>(this->delay_until_ - now) > 0 ? this->delay_until_ : now;
  this->waiting_for_idle_ = should_wait;
}

/**
 * Called during the loop task.
 * First defer for any pending delays, then check if we are waiting for the display to become idle.
 * If not waiting for idle, process the state machine.
 */

void EPaperBase::loop() {
  auto now = millis();
  // using modulus arithmetic to handle wrap-around
  int diff = now - this->delay_until_;
  if (diff < 0) {
    // Running the loop continuously cannot shorten a delay longer than one loop interval
    if (-diff > static_cast<int>(App.get_loop_interval())) {
      this->high_frequency_loop_.stop();
    } else {
      this->high_frequency_loop_.start();
    }
    return;
  }
  if (this->waiting_for_idle_) {
    if (this->is_idle_()) {
      this->waiting_for_idle_ = false;
      this->high_frequency_loop_.start();
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
      ESP_LOGV(TAG, "Screen was busy for %u ms", (unsigned) (millis() - this->waiting_for_idle_start_));
#endif
    } else {
      const uint32_t busy_for = now - this->waiting_for_idle_start_;
      if (busy_for > this->busy_timeout_seconds_ * 1000u) {
        this->abandon_update_();
        return;
      }
      if (busy_for > FAST_BUSY_POLL_MS)
        this->high_frequency_loop_.stop();
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
      if (now - this->waiting_for_idle_last_print_ >= 1000) {
        ESP_LOGV(TAG, "Waiting for idle in state %s", this->epaper_state_to_string_());
        this->waiting_for_idle_last_print_ = millis();
      }
#endif
      return;
    }
  }
  this->process_state_();
  if (this->is_failed())
    this->high_frequency_loop_.stop();
}

// The busy pin never cleared: give up on this update so later ones are not refused, and make the next one full
void EPaperBase::abandon_update_() {
  ESP_LOGE(TAG, "Display stayed busy for %u s in state %s; check the busy pin", (unsigned) this->busy_timeout_seconds_,
           this->epaper_state_to_string_());
  this->status_set_warning(LOG_STR("Busy pin timeout"));
  if (this->state_ == EPaperState::TRANSFER_DATA)
    this->abort_transfer();
  // Leave the panel powered down rather than with its high-voltage drivers enabled
  this->power_off();
  this->full_update_requested_ = true;
  this->current_data_index_ = 0;
  this->set_state_(EPaperState::IDLE);
}

/**
 * Process the state machine.
 * Typical state sequence:
 * IDLE -> RESET -> RESET_END -> UPDATE -> INITIALISE -> TRANSFER_DATA -> POWER_ON -> REFRESH_SCREEN -> POWER_OFF ->
 * DEEP_SLEEP -> IDLE
 *
 * Should a subclassed class need to override this, the method will need to be made virtual.
 */
void EPaperBase::process_state_() {
  ESP_LOGV(TAG, "Process state entered in state %s", epaper_state_to_string_());
  switch (this->state_) {
    default:
      ESP_LOGE(TAG, "Display is in unhandled state %s", epaper_state_to_string_());
      this->set_state_(EPaperState::IDLE);
      break;
    case EPaperState::IDLE:
      this->disable_loop();
      break;
    case EPaperState::RESET:
    case EPaperState::RESET_END:
      if (this->reset()) {
        this->set_state_(EPaperState::INITIALISE);
      } else {
        this->set_state_(EPaperState::RESET_END, this->reset_duration_);
      }
      break;
    case EPaperState::UPDATE:
      this->do_update_();  // Calls ESPHome (current page) lambda
      if (this->full_update_requested_) {
        // Refresh the whole panel even if nothing was drawn
        this->full_update_requested_ = false;
        this->update_count_ = 0;
        this->set_full_bounds_();
      }
      if (this->x_high_ < this->x_low_ || this->y_high_ < this->y_low_) {
        this->set_state_(EPaperState::IDLE);
        return;
      }
      if (this->is_using_partial_update_()) {
        const uint32_t frame_hash = this->buffer_hash_();
        if (this->update_count_ != 0 && this->shown_hash_valid_ && frame_hash == this->shown_hash_) {
          ESP_LOGD(TAG, "Frame unchanged, refresh skipped");
          this->reset_bounds_();
          this->set_state_(EPaperState::IDLE);
          return;
        }
        this->shown_hash_ = frame_hash;
      }
      this->drawn_since_hash_ = false;
      // Valid again only once this frame is on the panel, so an abandoned update leaves nothing to match
      this->shown_hash_valid_ = false;
      // A full update resets update_count_ to zero, so a non-zero count marks a partial one
      this->partial_ = this->update_count_ != 0;
      this->sent_count_ = 0;
      this->set_state_(EPaperState::RESET);
      break;
    case EPaperState::INITIALISE:
      if (!this->initialise(this->partial_)) {
        return;  // Not done yet, come back next loop
      }
      this->set_state_(EPaperState::TRANSFER_DATA);
      break;
    case EPaperState::TRANSFER_DATA:
      if (!this->transfer_data()) {
        return;  // Not done yet, come back next loop
      }
      if (this->sent_.is_valid() && this->sent_count_ == this->sent_.size())
        this->sent_valid_ = true;
      // A draw during the chunked push may not have reached the panel, so the next update sends the whole frame
      this->sent_hash_intact_ = this->is_using_partial_update_() && !this->drawn_since_hash_;
      if (this->drawn_since_hash_) {
        this->set_full_bounds_();
      } else {
        this->reset_bounds_();
      }
      this->set_state_(EPaperState::POWER_ON);
      break;
    case EPaperState::POWER_ON:
      this->power_on();
      this->set_state_(EPaperState::REFRESH_SCREEN);
      break;
    case EPaperState::REFRESH_SCREEN:
      this->refresh_screen(this->partial_);
      this->update_count_ = (this->update_count_ + 1) % this->full_update_every_;
      this->set_state_(EPaperState::POWER_OFF);
      break;
    case EPaperState::POWER_OFF:
      if (!this->after_refresh()) {
        return;  // Not done yet, come back next loop
      }
      this->power_off();
      this->set_state_(EPaperState::DEEP_SLEEP);
      break;
    case EPaperState::DEEP_SLEEP:
      this->deep_sleep();
      this->shown_hash_valid_ = this->sent_hash_intact_;
      this->status_clear_warning();
      this->set_state_(EPaperState::IDLE);
      ESP_LOGD(TAG, "Display update took %" PRIu32 " ms", millis() - this->update_start_time_);
      break;
  }
}

void EPaperBase::set_state_(EPaperState state, uint16_t delay) {
  ESP_LOGV(TAG, "Exit state %s", this->epaper_state_to_string_());
  this->state_ = state;
  // allow subclasses to nominate delays
  if (delay == 0)
    delay = this->next_delay_;
  this->next_delay_ = 0;
  this->delay_until_ = millis() + delay;
  this->wait_for_idle_(state > EPaperState::SHOULD_WAIT);
  ESP_LOGV(TAG, "Enter state %s, delay %u, wait_for_idle=%s", this->epaper_state_to_string_(), delay,
           TRUEFALSE(this->waiting_for_idle_));
  // Each step of an update otherwise waits for the next loop iteration; loop() slows down again for long waits
  if (state == EPaperState::IDLE) {
    this->high_frequency_loop_.stop();
    this->disable_loop();
  } else {
    this->high_frequency_loop_.start();
  }
}

void EPaperBase::start_data_() {
  this->dc_pin_->digital_write(true);
  this->enable();
}

void EPaperBase::on_safe_shutdown() { this->deep_sleep(); }

void EPaperBase::send_init_sequence_(const uint8_t *sequence, size_t length) {
  size_t index = 0;

  while (index != length) {
    if (length - index < 2) {
      this->mark_failed(LOG_STR("Malformed init sequence"));
      return;
    }
    const uint8_t cmd = sequence[index++];
    if (const uint8_t x = sequence[index++]; x == DELAY_FLAG) {
      ESP_LOGV(TAG, "Delay %dms", cmd);
      delay(cmd);
    } else {
      const uint8_t num_args = x & 0x7F;
      if (length - index < num_args) {
        ESP_LOGE(TAG, "Malformed init sequence, cmd = %X, num_args = %u", cmd, num_args);
        this->mark_failed();
        return;
      }
      this->cmd_data(cmd, sequence + index, num_args);
      index += num_args;
    }
  }
}

bool EPaperBase::initialise(bool partial) {
  this->send_init_sequence_(this->init_sequence_, this->init_sequence_length_);
  return true;
}

/**
 * Check and rotate coordinates based on the transform flags.
 * @param x
 * @param y
 * @return false if the coordinates are out of bounds
 */
bool EPaperBase::rotate_coordinates_(int &x, int &y) {
  if (this->is_point_clipped(x, y))
    return false;
  if (this->effective_transform_ & SWAP_XY)
    std::swap(x, y);
  if (this->effective_transform_ & MIRROR_X)
    x = this->width_ - x - 1;
  if (this->effective_transform_ & MIRROR_Y)
    y = this->height_ - y - 1;
  if (x >= this->width_ || y >= this->height_ || x < 0 || y < 0)
    return false;
  this->x_low_ = clamp_at_most(this->x_low_, x);
  this->x_high_ = clamp_at_least(this->x_high_, x + 1);
  this->y_low_ = clamp_at_most(this->y_low_, y);
  this->y_high_ = clamp_at_least(this->y_high_, y + 1);
  this->drawn_since_hash_ = true;
  return true;
}

/**
 *  Default implementation for monochrome displays where 8 pixels are packed to a byte.
 * @param x
 * @param y
 * @param color
 */
void HOT EPaperBase::draw_pixel_at(int x, int y, Color color) {
  if (!rotate_coordinates_(x, y))
    return;
  const size_t byte_position = y * this->row_width_ + x / 8;
  const uint8_t bit_position = x % 8;
  const uint8_t pixel_bit = 0x80u >> bit_position;
  const auto original = this->buffer_[byte_position];
  if (color_to_mono(color) == 0) {
    this->buffer_[byte_position] = original & ~pixel_bit;
  } else {
    this->buffer_[byte_position] = original | pixel_bit;
  }
}

namespace {
// One source row of an RGB565 block, written into a contiguous 1 bit per pixel buffer along a buffer row, the bit
// moving by step.
template<bool BigEndianSource>
void HOT write_rgb565_row(uint8_t *byte, uint8_t mask, int step, const uint8_t *source, int count,
                          const Rgb565ToMono &to_mono) {
  for (; count != 0; count--, source += 2) {
    const uint16_t value = BigEndianSource ? encode_uint16(source[0], source[1]) : encode_value_le<uint16_t>(source);
    if (to_mono(value)) {
      *byte |= mask;
    } else {
      *byte &= ~mask;
    }
    if (step > 0) {
      mask >>= 1;
      if (mask == 0) {
        mask = 0x80;
        byte++;
      }
    } else {
      mask <<= 1;
      if (mask == 0) {
        mask = 0x01;
        byte--;
      }
    }
  }
}

// The tiled swapped path below keeps a 2 KB table in flash and a 2 KB tile on the stack, both of which cost RAM on
// the ESP8266, where nothing but a user lambda reaches it: there a swapped block is drawn pixel by pixel.
#ifndef USE_ESP8266
// Luma rises with the blue field, so each red and green pair has a lowest blue value that comes out white: one
// compare per pixel instead of three weighted lookups. 32 marks a pair that is never white.
class MonoThreshold {
 public:
  constexpr MonoThreshold() {
    const Rgb565ToMono to_mono;
    for (uint32_t red_green = 0; red_green != 2048; red_green++) {
      uint8_t low = 0, high = 32;
      while (low != high) {
        const uint8_t blue = (low + high) / 2;
        if (to_mono(static_cast<uint16_t>((red_green << 5) | blue))) {
          high = blue;
        } else {
          low = blue + 1;
        }
      }
      this->lowest_white_blue_[red_green] = low;
    }
  }
  const uint8_t *table() const { return this->lowest_white_blue_; }

 protected:
  uint8_t lowest_white_blue_[2048] = {};
};

// Thresholds one source row into bit `shift` of every `stride`th byte from `dest`. Kept out of line: inlined into
// the block routine it runs out of registers and spills in the loop.
template<bool BigEndianSource, bool AlignedSource>
__attribute__((noinline)) void HOT threshold_rgb565_row(uint8_t *dest, size_t stride, const uint8_t *source, int count,
                                                        unsigned shift, const uint8_t *table) {
  for (; count != 0; count--, source += 2, dest += stride) {
    uint16_t value;
    if constexpr (BigEndianSource) {
      value = (source[0] << 8) | source[1];
    } else if constexpr (AlignedSource) {
      // An even source address and a little endian target: one load per pixel. A 2 byte memcpy becomes a call
      // under -Os, so the pixel is read through a type that may alias the bytes.
      using Pixel __attribute__((may_alias)) = uint16_t;
      value = *reinterpret_cast<const Pixel *>(source);
    } else {
      value = source[0] | (source[1] << 8);
    }
    *dest |= static_cast<uint8_t>((value & 0x1F) >= table[value >> 5]) << shift;
  }
}

// With the axes swapped a source row lies along a bit column of the buffer, so a run of TILE_ROWS source rows
// fills TILE_BYTES consecutive bytes in each buffer row. The rows are gathered TILE_COLUMNS at a time into a tile
// in internal RAM and each buffer row then gets its bytes in one go; a frame buffer in PSRAM would otherwise
// miss the cache on every byte.
constexpr int TILE_ROWS = 64;
constexpr int TILE_BYTES = TILE_ROWS / 8;
constexpr int TILE_COLUMNS = 256;

struct TileRow {
  uint8_t byte;
  uint8_t shift;
};

template<bool BigEndianSource, bool AlignedSource>
void HOT write_rgb565_tile(uint8_t *first_byte, ptrdiff_t byte_step, const TileRow *rows, int row_count,
                           const uint8_t *const *sources, int count, const uint8_t *table) {
  uint8_t tile[TILE_COLUMNS * TILE_BYTES];
  uint8_t covered[TILE_BYTES] = {};
  for (int i = 0; i != row_count; i++)
    covered[rows[i].byte] |= 1u << rows[i].shift;
  int covered_bytes = 0;
  bool full = true;
  for (int k = 0; k != TILE_BYTES; k++) {
    if (covered[k] != 0)
      covered_bytes = k + 1;
    full = full && covered[k] == 0xFF;
  }
  for (int start = 0; start < count; start += TILE_COLUMNS) {
    const int n = std::min(TILE_COLUMNS, count - start);
    memset(tile, 0, n * TILE_BYTES);
    for (int i = 0; i != row_count; i++) {
      threshold_rgb565_row<BigEndianSource, AlignedSource>(tile + rows[i].byte, TILE_BYTES, sources[i] + start * 2, n,
                                                           rows[i].shift, table);
    }
    uint8_t *byte = first_byte + start * byte_step;
    const uint8_t *column = tile;
    for (int c = 0; c != n; c++, byte += byte_step, column += TILE_BYTES) {
      if (full) {
        memcpy(byte, column, TILE_BYTES);
      } else {
        for (int k = 0; k != covered_bytes; k++)
          byte[k] = (byte[k] & ~covered[k]) | column[k];
      }
    }
  }
}
#endif
}  // namespace

// Each source column maps to one buffer coordinate and each source row to the other, each moving by 1 per pixel, so
// the visible part of a block and its dirty area are worked out once for the whole block.
void HOT EPaperBase::draw_rgb565_block_(uint8_t *buffer, int x_start, int y_start, int w, int h, const uint8_t *ptr,
                                        bool big_endian, int x_offset, int y_offset, int x_pad) {
  const bool swap = this->effective_transform_ & SWAP_XY;
  const bool mirror_x = this->effective_transform_ & MIRROR_X;
  const bool mirror_y = this->effective_transform_ & MIRROR_Y;
  // The buffer axis a source column moves along, and the one a source row moves along
  const int column_limit = swap ? this->height_ : this->width_;
  const int row_limit = swap ? this->width_ : this->height_;
  const bool column_mirrored = swap ? mirror_y : mirror_x;
  const bool row_mirrored = swap ? mirror_x : mirror_y;
  const int first_column = std::max(0, -x_start);
  const int end_column = std::min(w, column_limit - x_start);
  const int first_row = std::max(0, -y_start);
  const int end_row = std::min(h, row_limit - y_start);
  if (first_column >= end_column || first_row >= end_row)
    return;
  auto column_to_buffer = [&](int column) {
    const int u = x_start + column;
    return column_mirrored ? column_limit - 1 - u : u;
  };
  auto row_to_buffer = [&](int row) {
    const int v = y_start + row;
    return row_mirrored ? row_limit - 1 - v : v;
  };

  const int column_low = std::min(column_to_buffer(first_column), column_to_buffer(end_column - 1));
  const int row_low = std::min(row_to_buffer(first_row), row_to_buffer(end_row - 1));
  const int columns = end_column - first_column;
  const int rows = end_row - first_row;
  const int x_low = swap ? row_low : column_low;
  const int y_low = swap ? column_low : row_low;
  const int x_high = x_low + (swap ? rows : columns);
  const int y_high = y_low + (swap ? columns : rows);

  const Rgb565ToMono to_mono;
  const size_t line_stride = x_offset + w + x_pad;
  const size_t row_width = this->row_width_;
  const int step = column_mirrored ? -1 : 1;
  auto source_row = [&](int row) { return ptr + ((y_offset + row) * line_stride + x_offset + first_column) * 2; };
#ifndef USE_ESP8266
  if (swap) {
    static constexpr MonoThreshold THRESHOLD;
    // Every source row starts a whole number of pixels from ptr, so all share its address parity; VNC can pass an odd
    // one
    const bool aligned = (reinterpret_cast<uintptr_t>(ptr) & 1) == 0;
    const ptrdiff_t byte_step = step * static_cast<ptrdiff_t>(row_width);
    for (int row = first_row; row != end_row;) {
      // The buffer bytes this run of rows lands in, taken in the direction the rows move
      const int first_x = row_to_buffer(row);
      const int low_byte = row_mirrored ? std::max(0, first_x / 8 - (TILE_BYTES - 1)) : first_x / 8;
      const int end_byte = low_byte + TILE_BYTES;
      TileRow rows[TILE_ROWS];
      const uint8_t *sources[TILE_ROWS];
      int row_count = 0;
      while (row + row_count != end_row) {
        const int x = row_to_buffer(row + row_count);
        if (x / 8 < low_byte || x / 8 >= end_byte)
          break;
        rows[row_count] = {static_cast<uint8_t>(x / 8 - low_byte), static_cast<uint8_t>(7 - x % 8)};
        sources[row_count] = source_row(row + row_count);
        row_count++;
      }
      uint8_t *first_byte = buffer + column_to_buffer(first_column) * row_width + low_byte;
      if (big_endian) {
        write_rgb565_tile<true, true>(first_byte, byte_step, rows, row_count, sources, columns, THRESHOLD.table());
      } else if (aligned) {
        write_rgb565_tile<false, true>(first_byte, byte_step, rows, row_count, sources, columns, THRESHOLD.table());
      } else {
        write_rgb565_tile<false, false>(first_byte, byte_step, rows, row_count, sources, columns, THRESHOLD.table());
      }
      row += row_count;
    }
  } else
#endif
  {
    for (int row = first_row; row != end_row; row++) {
      const int x = column_to_buffer(first_column);
      uint8_t *byte = buffer + row_to_buffer(row) * row_width + x / 8;
      const uint8_t mask = 0x80u >> (x % 8);
      if (big_endian) {
        write_rgb565_row<true>(byte, mask, step, source_row(row), columns, to_mono);
      } else {
        write_rgb565_row<false>(byte, mask, step, source_row(row), columns, to_mono);
      }
    }
  }
  this->x_low_ = clamp_at_most(this->x_low_, x_low);
  this->x_high_ = clamp_at_least(this->x_high_, x_high);
  this->y_low_ = clamp_at_most(this->y_low_, y_low);
  this->y_high_ = clamp_at_least(this->y_high_, y_high);
  this->drawn_since_hash_ = true;
}

// Writes an RGB565 block of pixels, as LVGL sends them, straight into a 1 bit per pixel buffer that is one block (the
// usual case): the same result as draw_pixel_at() for each pixel. Anything else goes through draw_pixel_at().
void HOT EPaperBase::draw_pixels_internal(int x_start, int y_start, int w, int h, const uint8_t *ptr,
                                          ColorBitness bitness, bool big_endian, int x_offset, int y_offset,
                                          int x_pad) {
#ifdef USE_ESP8266
  const bool swapped = this->effective_transform_ & SWAP_XY;
#else
  const bool swapped = false;
#endif
  size_t contiguous_length;
  uint8_t *contiguous = this->buffer_.get_span(0, contiguous_length);
  if (swapped || !this->buffer_is_mono_() || this->get_clipping().is_set() || bitness != COLOR_BITNESS_565 ||
      contiguous_length != this->buffer_length_) {
    Display::draw_pixels_internal(x_start, y_start, w, h, ptr, bitness, big_endian, x_offset, y_offset, x_pad);
    return;
  }
  this->draw_rgb565_block_(contiguous, x_start, y_start, w, h, ptr, big_endian, x_offset, y_offset, x_pad);
}

void EPaperBase::dump_config() {
  LOG_DISPLAY("", "E-Paper SPI", this);
  ESP_LOGCONFIG(TAG,
                "  Model: %s\n"
                "  SPI Data Rate: %uMHz\n"
                "  Full update every: %d\n"
                "  Swap X/Y: %s\n"
                "  Mirror X: %s\n"
                "  Mirror Y: %s",
                this->name_, (unsigned) (this->data_rate_ / 1000000), this->full_update_every_,
                YESNO(this->transform_ & SWAP_XY), YESNO(this->transform_ & MIRROR_X),
                YESNO(this->transform_ & MIRROR_Y));
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::epaper_spi
