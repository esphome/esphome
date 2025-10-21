#include "led_strip.h"
#include <cinttypes>

#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <esp_attr.h>

namespace esphome {
namespace esp32_rmt_led_strip {

static const char *const TAG = "esp32_rmt_led_strip";

#ifdef USE_ESP32_VARIANT_ESP32H2
static const uint32_t RMT_CLK_FREQ = 32000000;
static const uint8_t RMT_CLK_DIV = 1;
#else
static const uint32_t RMT_CLK_FREQ = 80000000;
static const uint8_t RMT_CLK_DIV = 2;
#endif

static const size_t RMT_SYMBOLS_PER_BYTE = 8;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
static size_t IRAM_ATTR HOT encoder_callback(const void *data, size_t size, size_t symbols_written, size_t symbols_free,
                                             rmt_symbol_word_t *symbols, bool *done, void *arg) {
  auto *params = static_cast<LedParams *>(arg);
  const auto *bytes = static_cast<const uint8_t *>(data);
  size_t index = symbols_written / RMT_SYMBOLS_PER_BYTE;

  // convert byte to symbols
  if (index < size) {
    if (symbols_free < RMT_SYMBOLS_PER_BYTE) {
      return 0;
    }
    for (size_t i = 0; i < RMT_SYMBOLS_PER_BYTE; i++) {
      if (bytes[index] & (1 << (7 - i))) {
        symbols[i] = params->bit1;
      } else {
        symbols[i] = params->bit0;
      }
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 1)
    if ((index + 1) >= size && params->reset.duration0 == 0 && params->reset.duration1 == 0) {
      *done = true;
    }
#endif
    return RMT_SYMBOLS_PER_BYTE;
  }

  // send reset
  if (symbols_free < 1) {
    return 0;
  }
  symbols[0] = params->reset;
  *done = true;
  return 1;
}
#endif

void ESP32RMTLEDStripLightOutput::setup() {
  size_t buffer_size = this->get_buffer_size_();

  RAMAllocator<uint8_t> allocator(this->use_psram_ ? 0 : RAMAllocator<uint8_t>::ALLOC_INTERNAL);
  this->buf_ = allocator.allocate(buffer_size);
  if (this->buf_ == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate LED buffer!");
    this->mark_failed();
    return;
  }
  memset(this->buf_, 0, buffer_size);

  this->effect_data_ = allocator.allocate(this->num_leds_);
  if (this->effect_data_ == nullptr) {
    ESP_LOGE(TAG, "Cannot allocate effect data!");
    this->mark_failed();
    return;
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  // copy of the led buffer
  this->rmt_buf_ = allocator.allocate(buffer_size);
#else
  RAMAllocator<rmt_symbol_word_t> rmt_allocator(this->use_psram_ ? 0 : RAMAllocator<rmt_symbol_word_t>::ALLOC_INTERNAL);

  // 8 bits per byte, 1 rmt_symbol_word_t per bit + 1 rmt_symbol_word_t for reset
  this->rmt_buf_ = rmt_allocator.allocate(buffer_size * 8 + 1);
#endif

  rmt_tx_channel_config_t channel;
  memset(&channel, 0, sizeof(channel));
  channel.clk_src = RMT_CLK_SRC_DEFAULT;
  channel.resolution_hz = RMT_CLK_FREQ / RMT_CLK_DIV;
  channel.gpio_num = gpio_num_t(this->pin_);
  channel.mem_block_symbols = this->rmt_symbols_;
  channel.trans_queue_depth = 1;
  channel.flags.io_loop_back = 0;
  channel.flags.io_od_mode = 0;
  channel.flags.invert_out = 0;
  channel.flags.with_dma = this->use_dma_;
  channel.intr_priority = 0;
  if (rmt_new_tx_channel(&channel, &this->channel_) != ESP_OK) {
    ESP_LOGE(TAG, "Channel creation failed");
    this->mark_failed();
    return;
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  rmt_simple_encoder_config_t encoder;
  memset(&encoder, 0, sizeof(encoder));
  encoder.callback = encoder_callback;
  encoder.arg = &this->params_;
  encoder.min_chunk_size = RMT_SYMBOLS_PER_BYTE;
  if (rmt_new_simple_encoder(&encoder, &this->encoder_) != ESP_OK) {
    ESP_LOGE(TAG, "Encoder creation failed");
    this->mark_failed();
    return;
  }
#else
  rmt_copy_encoder_config_t encoder;
  memset(&encoder, 0, sizeof(encoder));
  if (rmt_new_copy_encoder(&encoder, &this->encoder_) != ESP_OK) {
    ESP_LOGE(TAG, "Encoder creation failed");
    this->mark_failed();
    return;
  }
#endif

  if (rmt_enable(this->channel_) != ESP_OK) {
    ESP_LOGE(TAG, "Enabling channel failed");
    this->mark_failed();
    return;
  }
}

void ESP32RMTLEDStripLightOutput::set_led_params(uint32_t bit0_high, uint32_t bit0_low, uint32_t bit1_high,
                                                 uint32_t bit1_low, uint32_t reset_time_high, uint32_t reset_time_low) {
  float ratio = (float) RMT_CLK_FREQ / RMT_CLK_DIV / 1e09f;

  // 0-bit
  this->params_.bit0.duration0 = (uint32_t) (ratio * bit0_high);
  this->params_.bit0.level0 = 1;
  this->params_.bit0.duration1 = (uint32_t) (ratio * bit0_low);
  this->params_.bit0.level1 = 0;
  // 1-bit
  this->params_.bit1.duration0 = (uint32_t) (ratio * bit1_high);
  this->params_.bit1.level0 = 1;
  this->params_.bit1.duration1 = (uint32_t) (ratio * bit1_low);
  this->params_.bit1.level1 = 0;
  // reset
  this->params_.reset.duration0 = (uint32_t) (ratio * reset_time_high);
  this->params_.reset.level0 = 1;
  this->params_.reset.duration1 = (uint32_t) (ratio * reset_time_low);
  this->params_.reset.level1 = 0;
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

  ESP_LOGVV(TAG, "Writing RGB values to bus");

  esp_err_t error = rmt_tx_wait_all_done(this->channel_, 1000);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "RMT TX timeout");
    this->status_set_warning();
    return;
  }
  delayMicroseconds(50);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  memcpy(this->rmt_buf_, this->buf_, this->get_buffer_size_());
#else
  size_t buffer_size = this->get_buffer_size_();

  size_t size = 0;
  size_t len = 0;
  uint8_t *psrc = this->buf_;
  rmt_symbol_word_t *pdest = this->rmt_buf_;
  while (size < buffer_size) {
    uint8_t b = *psrc;
    for (int i = 0; i < 8; i++) {
      pdest->val = b & (1 << (7 - i)) ? this->params_.bit1.val : this->params_.bit0.val;
      pdest++;
      len++;
    }
    size++;
    psrc++;
  }

  if (this->params_.reset.duration0 > 0 || this->params_.reset.duration1 > 0) {
    pdest->val = this->params_.reset.val;
    pdest++;
    len++;
  }
#endif

  rmt_transmit_config_t config;
  memset(&config, 0, sizeof(config));
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  error = rmt_transmit(this->channel_, this->encoder_, this->rmt_buf_, this->get_buffer_size_(), &config);
#else
  error = rmt_transmit(this->channel_, this->encoder_, this->rmt_buf_, len * sizeof(rmt_symbol_word_t), &config);
#endif
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "RMT TX error");
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();
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
  uint8_t multiplier = this->is_rgbw_ || this->is_wrgb_ ? 4 : 3;
  uint8_t white = this->is_wrgb_ ? 0 : 3;

  return {this->buf_ + (index * multiplier) + r + this->is_wrgb_,
          this->buf_ + (index * multiplier) + g + this->is_wrgb_,
          this->buf_ + (index * multiplier) + b + this->is_wrgb_,
          this->is_rgbw_ || this->is_wrgb_ ? this->buf_ + (index * multiplier) + white : nullptr,
          &this->effect_data_[index],
          &this->correction_};
}

void ESP32RMTLEDStripLightOutput::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ESP32 RMT LED Strip:\n"
                "  Pin: %u",
                this->pin_);
  ESP_LOGCONFIG(TAG, "  RMT Symbols: %" PRIu32, this->rmt_symbols_);
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
  ESP_LOGCONFIG(TAG,
                "  RGB Order: %s\n"
                "  Max refresh rate: %" PRIu32 "\n"
                "  Number of LEDs: %u",
                rgb_order, *this->max_refresh_rate_, this->num_leds_);
}

float ESP32RMTLEDStripLightOutput::get_setup_priority() const { return setup_priority::HARDWARE; }

}  // namespace esp32_rmt_led_strip
}  // namespace esphome

#endif  // USE_ESP32
