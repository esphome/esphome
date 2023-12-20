#include <cinttypes>
#include "led_strip.h"
#include "rds_rgbw_02.h"

#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <esp_attr.h>

namespace esphome {
namespace esp32_rmt_led_strip {

static const char *const TAG = "esp32_rmt_led_strip";

static constexpr uint8_t RMT_CLK_DIV = 2;
static constexpr float CLK_RATIO = (float) APB_CLK_FREQ / RMT_CLK_DIV / 1e09f;

// 15 bits of duration per each high/low per item
static constexpr uint32_t MAX_CYCLES_PER_DURATION = std::numeric_limits<uint16_t>::max() >> 1;
static constexpr uint32_t MAX_CYCLES_PER_ITEM = MAX_CYCLES_PER_DURATION * 2;

void ESP32RMTLEDStripLightOutput::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP32 LED Strip...");

  size_t buffer_size = this->get_buffer_size_();

  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  this->buf_[0] = allocator.allocate(buffer_size);
  if (this->allow_partial_updates_) {
    // allocate a second buffer for swap->delta comparison
    this->buf_[1] = allocator.allocate(buffer_size);
  }
  if (this->buf_[0] == nullptr || (this->allow_partial_updates_ && this->buf_[1] == nullptr)) {
    ESP_LOGE(TAG, "Cannot allocate LED buffer!");
    this->mark_failed();
    return;
  }

  this->effect_data_ = allocator.allocate(this->num_leds_);
  if (this->effect_data_ == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate effect data!");
    this->mark_failed();
    return;
  }

  ExternalRAMAllocator<rmt_item32_t> rmt_allocator(ExternalRAMAllocator<rmt_item32_t>::ALLOW_FAILURE);
  this->rmt_buf_ = rmt_allocator.allocate(this->get_rmt_buffer_size_());

  rmt_config_t config;
  memset(&config, 0, sizeof(config));
  config.channel = this->channel_;
  config.rmt_mode = RMT_MODE_TX;
  config.gpio_num = gpio_num_t(this->pin_);
  config.mem_block_num = 1;
  config.clk_div = RMT_CLK_DIV;
  config.tx_config.loop_en = false;
  config.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;
  config.tx_config.carrier_en = false;
  config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  config.tx_config.idle_output_en = true;
  if (this->is_inverted_) {
    config.flags |= RMT_CHANNEL_FLAGS_INVERT_SIG;
  }

  if (rmt_config(&config) != ESP_OK) {
    ESP_LOGE(TAG, "Cannot initialize RMT!");
    this->mark_failed();
    return;
  }
  if (rmt_driver_install(config.channel, 0, 0) != ESP_OK) {
    ESP_LOGE(TAG, "Cannot install RMT driver!");
    this->mark_failed();
    return;
  }
}

void ESP32RMTLEDStripLightOutput::set_led_params(uint32_t bit0_high, uint32_t bit0_low, uint32_t bit1_high,
                                                 uint32_t bit1_low) {
  // 0-bit
  this->bit0_.duration0 = (uint32_t) (CLK_RATIO * bit0_high);
  this->bit0_.level0 = 1;
  this->bit0_.duration1 = (uint32_t) (CLK_RATIO * bit0_low);
  this->bit0_.level1 = 0;
  // 1-bit
  this->bit1_.duration0 = (uint32_t) (CLK_RATIO * bit1_high);
  this->bit1_.level0 = this->encoding_ != ENCODING_BI_PHASE ? 1 : 0;
  this->bit1_.duration1 = (uint32_t) (CLK_RATIO * bit1_low);
  this->bit1_.level1 = this->encoding_ != ENCODING_BI_PHASE ? 0 : 1;
}

void ESP32RMTLEDStripLightOutput::set_sync_start(uint32_t sync_start) {
  this->sync_start_ = (uint32_t) (CLK_RATIO * sync_start);
}

void ESP32RMTLEDStripLightOutput::write_state(light::LightState *state) {
  // protect from refreshing too often
  uint32_t now = micros();
  if (*this->max_refresh_rate_ != 0 && (now - this->last_refresh_) < *this->max_refresh_rate_) {
    // try again next loop iteration, so that this change won't get lost
    this->schedule_show();
    return;
  }
  this->last_refresh_ = now;
  this->mark_shown_();

  ESP_LOGVV(TAG, "Writing RGB values to bus...");

  if (rmt_wait_tx_done(this->channel_, pdMS_TO_TICKS(1000)) != ESP_OK) {
    ESP_LOGE(TAG, "RMT TX timeout");
    this->status_set_warning();
    return;
  }
  delayMicroseconds(50);

  const int command_len = this->get_bits_per_command_();

  int len = 0;
  uint8_t *src = this->get_current_buf_();
  rmt_item32_t *dest = this->rmt_buf_;
  for (size_t i = 0; i < this->num_leds_; i++) {
    if (this->allow_partial_updates_ && this->bufs_same_at_index_(i)) {
      src += this->get_bytes_per_led_();
      continue;
    }

    this->rmt_generator_(*this, i, src, dest, state);
    src += this->get_bytes_per_led_();
    dest += command_len;
    len += command_len;

    // Pulse distance requires a final pulse to bookend the last distance.
    if (this->encoding_ == ENCODING_PULSE_DISTANCE) {
      dest->val = this->bit0_.val;
      dest++;
      len++;
    }

    if (this->intermission_ > 0) {
      generate_rmt_items_for_micros_delay(dest, this->intermission_);
      const uint32_t delay_count = get_rmt_item_count_for_micros_delay(this->intermission_);
      dest += delay_count;
      len += delay_count;
    }
  }

  if (len > this->get_rmt_buffer_size_()) {
    ESP_LOGE(TAG, "Attempting to write %d items to RMT buffer size %d", len, this->get_rmt_buffer_size_());
  } else if (len > 0 && this->rmt_write_items_(len)) {
    this->status_clear_warning();
    this->swap_buf_();
  }
}

light::ESPColorView ESP32RMTLEDStripLightOutput::get_view_internal(int32_t index) const {
  int32_t r = 0, g = 0, b = 0;
  switch (this->rgb_order_) {
    case ORDER_RGB:
      r = 0;
      g = 1;
      b = 2;
      break;
    case ORDER_RBG:
      r = 0;
      g = 2;
      b = 1;
      break;
    case ORDER_GRB:
      r = 1;
      g = 0;
      b = 2;
      break;
    case ORDER_GBR:
      r = 2;
      g = 0;
      b = 1;
      break;
    case ORDER_BGR:
      r = 2;
      g = 1;
      b = 0;
      break;
    case ORDER_BRG:
      r = 1;
      g = 2;
      b = 0;
      break;
  }
  uint8_t multiplier = this->is_rgbw_ ? 4 : 3;
  return {this->get_current_buf_() + (index * multiplier) + r,
          this->get_current_buf_() + (index * multiplier) + g,
          this->get_current_buf_() + (index * multiplier) + b,
          this->is_rgbw_ ? this->get_current_buf_() + (index * multiplier) + 3 : nullptr,
          &this->effect_data_[index],
          &this->correction_};
}

void ESP32RMTLEDStripLightOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP32 RMT LED Strip:");
  ESP_LOGCONFIG(TAG, "  Pin: %u", this->pin_);
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  const char *rgb_order;
  switch (this->rgb_order_) {
    case ORDER_RGB:
      rgb_order = "RGB";
      break;
    case ORDER_RBG:
      rgb_order = "RBG";
      break;
    case ORDER_GRB:
      rgb_order = "GRB";
      break;
    case ORDER_GBR:
      rgb_order = "GBR";
      break;
    case ORDER_BGR:
      rgb_order = "BGR";
      break;
    case ORDER_BRG:
      rgb_order = "BRG";
      break;
    default:
      rgb_order = "UNKNOWN";
      break;
  }
  ESP_LOGCONFIG(TAG, "  RGB Order: %s", rgb_order);
  ESP_LOGCONFIG(TAG, "  ColorModes:");
  for (const light::ColorMode mode : this->supported_color_modes_) {
    const char *mode_string = "UNKNOWN";
    switch (mode) {
      case light::ColorMode::ON_OFF:
        mode_string = "ON_OFF";
        break;
      case light::ColorMode::BRIGHTNESS:
        mode_string = "BRIGHTNESS";
        break;
      case light::ColorMode::WHITE:
        mode_string = "WHITE";
        break;
      case light::ColorMode::COLOR_TEMPERATURE:
        mode_string = "COLOR_TEMPERATURE";
        break;
      case light::ColorMode::COLD_WARM_WHITE:
        mode_string = "COLD_WARM_WHITE";
        break;
      case light::ColorMode::RGB:
        mode_string = "RGB";
        break;
      case light::ColorMode::RGB_WHITE:
        mode_string = "RGB_WHITE";
        break;
      case light::ColorMode::RGB_COLOR_TEMPERATURE:
        mode_string = "RGB_COLOR_TEMPERATURE";
        break;
      case light::ColorMode::RGB_COLD_WARM_WHITE:
        mode_string = "RGB_COLD_WARM_WHITE";
        break;
      default:
        mode_string = "UNKNOWN";
        break;
    }
    ESP_LOGCONFIG(TAG, "    %s", mode_string);
  }
  ESP_LOGCONFIG(TAG, "  Max refresh rate: %" PRIu32, *this->max_refresh_rate_);
  ESP_LOGCONFIG(TAG, "  Number of LEDs: %u", this->num_leds_);
  const char *encoding = "UNKNOWN";
  switch (this->encoding_) {
    case ENCODING_PULSE_LENGTH:
      encoding = "PULSE_LENGTH";
      break;
    case ENCODING_PULSE_DISTANCE:
      encoding = "PULSE_DISTANCE";
      break;
    case ENCODING_BI_PHASE:
      encoding = "BI_PHASE";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Encoding: %s", encoding);

  ESP_LOGD(TAG, "RMT item buffer size: %d", this->get_rmt_buffer_size_());
}

float ESP32RMTLEDStripLightOutput::get_setup_priority() const { return setup_priority::HARDWARE; }

light::LightTraits ESP32RMTLEDStripLightOutput::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes(this->supported_color_modes_);
  return traits;
}

size_t ESP32RMTLEDStripLightOutput::get_bits_per_command_() const {
  return this->bits_per_command_ > 0 ? this->bits_per_command_ : this->get_bytes_per_led_() * 8;
}

size_t ESP32RMTLEDStripLightOutput::get_items_per_command_() const {
  int count = this->get_bits_per_command_();
  count += (this->encoding_ == ENCODING_PULSE_DISTANCE);
  if (this->intermission_ > 0) {
    count += get_rmt_item_count_for_micros_delay(this->intermission_);
  }
  return count;
}

size_t ESP32RMTLEDStripLightOutput::get_rmt_buffer_size_() const {
  return this->num_leds_ * this->get_items_per_command_();
}

uint32_t ESP32RMTLEDStripLightOutput::get_cycle_count_for_micros_delay(const uint32_t delay) {
  return (uint32_t) (CLK_RATIO * (float) delay * 1000.f);  // CLK_RATIO is to nanos
}

uint32_t ESP32RMTLEDStripLightOutput::get_rmt_item_count_for_micros_delay(const uint32_t delay) {
  return (uint32_t) std::ceil((float) get_cycle_count_for_micros_delay(delay) / (float) MAX_CYCLES_PER_ITEM);
}

static constexpr int32_t constexpr_ceil(float num) {
  return (static_cast<float>(static_cast<int32_t>(num)) == num) ? static_cast<int32_t>(num)
                                                                : static_cast<int32_t>(num) + ((num > 0) ? 1 : 0);
}

void ESP32RMTLEDStripLightOutput::generate_rmt_items_for_micros_delay(rmt_item32_t *dest, const uint32_t delay) {
  // the RMT driver will glitch with duration values below this
  static constexpr int32_t RMT_MIN_DURATION = (int32_t) constexpr_ceil(2.f / CLK_RATIO);

  assert(get_cycle_count_for_micros_delay(delay) <= (std::numeric_limits<uint32_t>::max() >> 1));
  int32_t cycles_remaining = (int32_t) get_cycle_count_for_micros_delay(delay);
  while (cycles_remaining > 0) {
    dest->duration0 = (uint32_t) std::min(cycles_remaining, (int32_t) MAX_CYCLES_PER_DURATION);
    cycles_remaining -= (int32_t) dest->duration0;
    dest->level0 = 0;
    dest->duration1 =
        (uint32_t) std::min(std::max(cycles_remaining, (int32_t) RMT_MIN_DURATION), (int32_t) MAX_CYCLES_PER_DURATION);
    cycles_remaining -= (int32_t) dest->duration1;
    dest->level1 = 0;
    dest++;
  }
}

void ESP32RMTLEDStripLightOutput::set_supported_color_modes(const std::set<esphome::light::ColorMode> &color_modes) {
  this->supported_color_modes_ = color_modes;
  if (color_modes.find(light::ColorMode::RGB_WHITE) != color_modes.cend()) {
    this->is_rgbw_ = true;
  }
}

bool ESP32RMTLEDStripLightOutput::bufs_same_at_index_(const int i) const {
  assert(this->allow_partial_updates_);
  assert(this->buf_[1]);

  const int bpc = this->get_bytes_per_led_();
  const uint8_t *buf0 = this->buf_[0];
  const uint8_t *buf1 = this->buf_[1];
  return buf0[i * bpc] == buf1[i * bpc] && buf0[i * bpc + 1] == buf1[i * bpc + 1] &&
         buf0[i * bpc + 2] == buf1[i * bpc + 2] && (!this->is_rgbw_ || buf0[i * bpc + 3] == buf1[i * bpc + 3]);
}

bool ESP32RMTLEDStripLightOutput::rmt_write_items_(const size_t len, const bool wait_tx_done) {
  if (rmt_write_items(this->channel_, this->rmt_buf_, len, wait_tx_done) != ESP_OK) {
    ESP_LOGE(TAG, "RMT TX error");
    this->status_set_warning();
    return false;
  }
  return true;
}

void ESP32RMTLEDStripLightOutput::default_rmt_generate(const ESP32RMTLEDStripLightOutput &light, const int index,
                                                       const uint8_t *src, rmt_item32_t *dest,
                                                       light::LightState *state) {
  const int color_count = 3 + light.get_is_rgbw();
  for (int i = 0; i < color_count; i++) {
    const uint8_t color = src[i];
    for (int j = 7; j >= 0; j--) {
      dest->val = (color >> j) & 1 ? light.get_bit1().val : light.get_bit0().val;
      dest++;
    }
  }
}

}  // namespace esp32_rmt_led_strip
}  // namespace esphome

#endif  // USE_ESP32
