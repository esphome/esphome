#include "epaper_spi.h"
#include <algorithm>
#include <cinttypes>
#include <cstring>
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32
#include <esp_system.h>
#ifdef EPAPER_SPI_IMAGE_STORE_SIZE
#include <esp_attr.h>
#include <miniz.h>
#endif
#endif
#ifdef USE_ESP8266
#include <user_interface.h>
#endif

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
  this->load_sleep_state_();
}

bool EPaperBase::woke_from_deep_sleep_() const {
#if defined(USE_ESP32)
  return esp_reset_reason() == ESP_RST_DEEPSLEEP;
#elif defined(USE_ESP8266)
  return system_get_rst_info()->reason == REASON_DEEP_SLEEP_AWAKE;
#else
  return false;
#endif
}

// The sleep state survives the controller's deep sleep in RTC memory: the update count, and whether
// the panel was left holding its image, so the first update after the wake can be partial.
void EPaperBase::load_sleep_state_() {
  if (this->sleep_state_hash_ == 0 || !this->is_using_partial_update_())
    return;
  this->sleep_state_ = global_preferences->make_preference<SleepState>(this->sleep_state_hash_, false);
  SleepState state{};
  if (this->woke_from_deep_sleep_() && this->sleep_state_.load(&state)) {
    if (state.panel_holds_image) {
      this->update_count_ = state.update_count % this->full_update_every_;
      this->panel_holds_image_ = true;
      ESP_LOGD(TAG, "Panel kept its image through deep sleep; next update is %s",
               this->update_count_ != 0 ? LOG_STR_LITERAL("partial") : LOG_STR_LITERAL("full"));
    }
#ifdef EPAPER_SPI_IMAGE_STORE_SIZE
    // The comparison frame does not exist yet; the image is restored into it once it is allocated
    if (state.image_size != 0)
      this->stored_image_ = state;
#endif
  }
  // Only a wake from deep sleep may use the state, so it is cleared once read
  this->save_sleep_state_(false);
}

void EPaperBase::save_sleep_state_(bool panel_holds_image, bool image_compressed, uint16_t image_size) {
  if (this->sleep_state_hash_ == 0 || !this->is_using_partial_update_())
    return;
  SleepState state{this->update_count_, panel_holds_image, image_compressed, image_size, 0};
#ifdef EPAPER_SPI_IMAGE_STORE_SIZE
  if (image_size != 0)
    state.image_hash = this->image_store_hash_(image_size);
#endif
  this->sleep_state_.save(&state);
}

#ifdef EPAPER_SPI_IMAGE_STORE_SIZE
static RTC_NOINIT_ATTR uint8_t s_image_store[EPAPER_SPI_IMAGE_STORE_SIZE];  // NOLINT

uint32_t EPaperBase::image_store_hash_(size_t size) const {
  uint32_t hash = FNV1_OFFSET_BASIS;
  for (size_t i = 0; i != size; i++)
    hash = fnv1_hash_extend(hash, s_image_store[i]);
  return hash;
}

struct StoreWriter {
  size_t position;
};

static mz_bool write_to_store(const void *data, int length, void *user) {
  auto *writer = static_cast<StoreWriter *>(user);
  if (writer->position + length > EPAPER_SPI_IMAGE_STORE_SIZE)
    return MZ_FALSE;
  memcpy(s_image_store + writer->position, data, length);
  writer->position += length;
  return MZ_TRUE;
}

// Copies the comparison frame into RTC memory as it is when it fits, otherwise as a DEFLATE stream
// made by the miniz in the chip's ROM.
bool EPaperBase::store_image_(bool &compressed, uint16_t &size) {
  const size_t length = this->sent_.size();
  if (length <= EPAPER_SPI_IMAGE_STORE_SIZE) {
    for (size_t index = 0; index != length;) {
      size_t span;
      const uint8_t *data = this->sent_.get_span(index, span);
      memcpy(s_image_store + index, data, span);
      index += span;
    }
    compressed = false;
    size = length;
    return true;
  }
  RAMAllocator<tdefl_compressor> allocator;
  tdefl_compressor *compressor = allocator.allocate(1);
  if (compressor == nullptr) {
    ESP_LOGW(TAG, "No memory to compress the image; the next update after the wake will be full");
    return false;
  }
  StoreWriter writer{};
  tdefl_status status = tdefl_init(compressor, write_to_store, &writer, TDEFL_DEFAULT_MAX_PROBES);
  for (size_t index = 0; index != length && status == TDEFL_STATUS_OKAY;) {
    size_t span;
    const uint8_t *data = this->sent_.get_span(index, span);
    index += span;
    status = tdefl_compress_buffer(compressor, data, span, index == length ? TDEFL_FINISH : TDEFL_NO_FLUSH);
  }
  allocator.deallocate(compressor, 1);
  if (status != TDEFL_STATUS_DONE) {
    ESP_LOGW(TAG, "Image does not compress into %u bytes; the next update after the wake will be full",
             (unsigned) EPAPER_SPI_IMAGE_STORE_SIZE);
    return false;
  }
  compressed = true;
  size = writer.position;
  return true;
}

bool EPaperBase::restore_image_() {
  const SleepState &state = this->stored_image_;
  const size_t length = this->sent_.size();
  if (state.image_size > EPAPER_SPI_IMAGE_STORE_SIZE || this->image_store_hash_(state.image_size) != state.image_hash) {
    ESP_LOGW(TAG, "Image in RTC memory is damaged; the next update will be full");
    return false;
  }
  if (!state.image_compressed) {
    if (state.image_size != length)
      return false;
    this->sent_.write(0, s_image_store, length);
    return true;
  }
  RAMAllocator<uint8_t> frame_allocator;
  RAMAllocator<tinfl_decompressor> allocator;
  uint8_t *frame = frame_allocator.allocate(length);
  tinfl_decompressor *decompressor = allocator.allocate(1);
  bool ok = false;
  if (frame != nullptr && decompressor != nullptr) {
    tinfl_init(decompressor);
    size_t in_size = state.image_size;
    size_t out_size = length;
    const tinfl_status status = tinfl_decompress(decompressor, s_image_store, &in_size, frame, frame, &out_size,
                                                 TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    ok = status == TINFL_STATUS_DONE && out_size == length;
    if (ok)
      this->sent_.write(0, frame, length);
  }
  if (decompressor != nullptr)
    allocator.deallocate(decompressor, 1);
  if (frame != nullptr)
    frame_allocator.deallocate(frame, length);
  if (!ok) {
    ESP_LOGW(TAG, "Image in RTC memory could not be restored; the next update will be full");
  }
  return ok;
}
#endif

// Runs before the controller sleeps or reboots, after on_safe_shutdown(): finish an update in flight,
// then note that the panel holds its image, so the first update after a wake is partial.
bool EPaperBase::teardown() {
  if (this->state_ != EPaperState::IDLE) {
    this->loop();
    return false;
  }
  if (this->parked_)
    return true;
  this->parked_ = true;
  if (this->sleep_state_hash_ == 0)
    return true;
#ifdef EPAPER_SPI_IMAGE_STORE_SIZE
  // With the image kept on this side the panel may lose its RAM, or its power, while asleep
  bool compressed;
  uint16_t size;
  if (this->image_in_rtc_memory_ && this->sent_valid_ && this->store_image_(compressed, size)) {
    this->save_sleep_state_(false, compressed, size);
    return true;
  }
#endif
  if (this->panel_holds_image_ && this->image_survives_sleep())
    this->save_sleep_state_(true);
  return true;
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
  if (!this->sent_.init(length)) {
    ESP_LOGW(TAG, "No memory for the comparison frame; every update will be refreshed");
    return false;
  }
#ifdef EPAPER_SPI_IMAGE_STORE_SIZE
  if (this->stored_image_.image_size != 0 && this->restore_image_()) {
    this->sent_valid_ = true;
    this->restore_previous_ = true;
    this->update_count_ = this->stored_image_.update_count % this->full_update_every_;
    ESP_LOGD(TAG, "Image restored from RTC memory; next update is %s",
             this->update_count_ != 0 ? LOG_STR_LITERAL("partial") : LOG_STR_LITERAL("full"));
  }
  this->stored_image_.image_size = 0;
#endif
  return true;
}

bool EPaperBase::frame_unchanged_() const {
  if (!this->sent_valid_ || this->sent_.size() != this->buffer_.size())
    return false;
  size_t index = 0;
  while (index != this->buffer_.size()) {
    size_t length;
    size_t sent_length;
    const uint8_t *data = this->buffer_.get_span(index, length);
    const uint8_t *sent = this->sent_.get_span(index, sent_length);
    length = std::min(length, sent_length);
    if (memcmp(data, sent, length) != 0)
      return false;
    index += length;
  }
  return true;
}

bool EPaperBase::bounds_from_changes_() {
  if (!this->sent_valid_ || this->sent_.size() != this->buffer_.size())
    return true;
  bool changed = false;
  uint16_t row_low = this->height_, row_high = 0, col_low = this->row_width_, col_high = 0;
  for (uint16_t row = 0; row != this->height_; row++) {
    const size_t base = row * this->row_width_;
    for (uint16_t col = 0; col != this->row_width_; col++) {
      if (this->buffer_[base + col] == this->sent_[base + col])
        continue;
      changed = true;
      row_low = std::min(row_low, row);
      row_high = std::max<uint16_t>(row_high, row + 1);
      col_low = std::min(col_low, col);
      col_high = std::max<uint16_t>(col_high, col + 1);
    }
  }
  if (!changed)
    return false;
  this->x_low_ = col_low * 8;
  this->x_high_ = std::min<uint16_t>(col_high * 8, this->width_);
  this->y_low_ = row_low;
  this->y_high_ = row_high;
  return true;
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

bool EPaperBase::reset() {
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
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  this->waiting_for_idle_start_ = millis();
#endif
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
  if (diff < 0)
    return;
  if (this->waiting_for_idle_) {
    if (this->is_idle_()) {
      this->waiting_for_idle_ = false;
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
      ESP_LOGV(TAG, "Screen was busy for %u ms", (unsigned) (millis() - this->waiting_for_idle_start_));
#endif
    } else {
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
        this->x_low_ = 0;
        this->y_low_ = 0;
        this->x_high_ = this->width_;
        this->y_high_ = this->height_;
      }
      if (this->x_high_ < this->x_low_ || this->y_high_ < this->y_low_) {
        this->set_state_(EPaperState::IDLE);
        return;
      }
      if (this->update_count_ != 0 && this->frame_unchanged_()) {
        ESP_LOGD(TAG, "Frame unchanged, refresh skipped");
        this->reset_bounds_();
        this->set_state_(EPaperState::IDLE);
        return;
      }
      this->full_window_ =
          this->x_low_ == 0 && this->y_low_ == 0 && this->x_high_ == this->width_ && this->y_high_ == this->height_;
      this->set_state_(EPaperState::RESET);
      break;
    case EPaperState::INITIALISE:
      if (!this->initialise(this->update_count_ != 0)) {
        return;  // Not done yet, come back next loop
      }
      this->set_state_(EPaperState::TRANSFER_DATA);
      break;
    case EPaperState::TRANSFER_DATA:
      if (!this->transfer_data()) {
        return;  // Not done yet, come back next loop
      }
      if (this->full_window_)
        this->sent_valid_ = this->sent_.is_valid();
      this->restore_previous_ = false;
      this->reset_bounds_();
      this->set_state_(EPaperState::POWER_ON);
      break;
    case EPaperState::POWER_ON:
      this->power_on();
      this->set_state_(EPaperState::REFRESH_SCREEN);
      break;
    case EPaperState::REFRESH_SCREEN:
      this->refresh_screen(this->update_count_ != 0);
      this->update_count_ = (this->update_count_ + 1) % this->full_update_every_;
      this->set_state_(EPaperState::POWER_OFF);
      break;
    case EPaperState::POWER_OFF:
      this->power_off();
      this->set_state_(EPaperState::DEEP_SLEEP);
      break;
    case EPaperState::DEEP_SLEEP:
      this->deep_sleep();
      this->panel_holds_image_ = this->is_using_partial_update_();
      this->set_state_(EPaperState::IDLE);
      ESP_LOGD(TAG, "Display update took %" PRIu32 " ms", millis() - this->update_start_time_);
      break;
  }
}

void EPaperBase::set_state_(EPaperState state, uint16_t delay) {
  ESP_LOGV(TAG, "Exit state %s", this->epaper_state_to_string_());
  this->state_ = state;
  this->wait_for_idle_(state > EPaperState::SHOULD_WAIT);
  // allow subclasses to nominate delays
  if (delay == 0)
    delay = this->next_delay_;
  this->next_delay_ = 0;
  this->delay_until_ = millis() + delay;
  ESP_LOGV(TAG, "Enter state %s, delay %u, wait_for_idle=%s", this->epaper_state_to_string_(), delay,
           TRUEFALSE(this->waiting_for_idle_));
  if (state == EPaperState::IDLE) {
    this->disable_loop();
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

void EPaperBase::dump_config() {
  LOG_DISPLAY("", "E-Paper SPI", this);
  ESP_LOGCONFIG(TAG,
                "  Model: %s\n"
                "  SPI Data Rate: %uMHz\n"
                "  Full update every: %d\n"
                "  Partial update after deep sleep: %s\n"
                "  Swap X/Y: %s\n"
                "  Mirror X: %s\n"
                "  Mirror Y: %s",
                this->name_, (unsigned) (this->data_rate_ / 1000000), this->full_update_every_,
                this->sleep_state_hash_ == 0 ? LOG_STR_LITERAL("no")
#ifdef EPAPER_SPI_IMAGE_STORE_SIZE
                : this->image_in_rtc_memory_ ? LOG_STR_LITERAL("RTC memory")
#endif
                                             : LOG_STR_LITERAL("panel"),
                YESNO(this->transform_ & SWAP_XY), YESNO(this->transform_ & MIRROR_X),
                YESNO(this->transform_ & MIRROR_Y));
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::epaper_spi
