#include <cinttypes>
#include "led_strip.h"

#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <esp_attr.h>

namespace esphome {
namespace esp32_rmt_led_strip {

static const char *const TAG = "esp32_rmt_led_strip";

static const uint8_t RMT_CLK_DIV = 2;

void ESP32RMTLEDStripLightOutput::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP32 LED Strip...");

  size_t buffer_size = this->get_buffer_size_();

  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  this->buf_ = allocator.allocate(buffer_size);
  if (this->buf_ == nullptr) {
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
  this->rmt_buf_ = rmt_allocator.allocate(buffer_size * 8);  // 8 bits per byte, 1 rmt_item32_t per bit

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
  float ratio = (float) APB_CLK_FREQ / RMT_CLK_DIV / 1e09f;

  // 0-bit
  this->bit0_.duration0 = (uint32_t) (ratio * bit0_high);
  this->bit0_.level0 = 1;
  this->bit0_.duration1 = (uint32_t) (ratio * bit0_low);
  this->bit0_.level1 = 0;
  // 1-bit
  this->bit1_.duration0 = (uint32_t) (ratio * bit1_high);
  this->bit1_.level0 = 1;
  this->bit1_.duration1 = (uint32_t) (ratio * bit1_low);
  this->bit1_.level1 = 0;
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

  size_t buffer_size = this->get_buffer_size_();

  size_t size = 0;
  size_t len = 0;
  uint8_t *psrc = this->buf_;
  rmt_item32_t *pdest = this->rmt_buf_;
  while (size < buffer_size) {
    uint8_t b = *psrc;
    for (int i = 0; i < 8; i++) {
      pdest->val = b & (1 << (7 - i)) ? this->bit1_.val : this->bit0_.val;
      pdest++;
      len++;
    }
    size++;
    psrc++;
  }

  if (rmt_write_items(this->channel_, this->rmt_buf_, len, false) != ESP_OK) {
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
  uint8_t multiplier = this->is_rgbw_ ? 4 : 3;
  return {this->buf_ + (index * multiplier) + r,
          this->buf_ + (index * multiplier) + g,
          this->buf_ + (index * multiplier) + b,
          this->is_rgbw_ ? this->buf_ + (index * multiplier) + 3 : nullptr,
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
  ESP_LOGCONFIG(TAG, "  Max refresh rate: %" PRIu32, *this->max_refresh_rate_);
  ESP_LOGCONFIG(TAG, "  Number of LEDs: %u", this->num_leds_);
}

float ESP32RMTLEDStripLightOutput::get_setup_priority() const { return setup_priority::HARDWARE; }

}  // namespace esp32_rmt_led_strip
}  // namespace esphome

#endif  // USE_ESP32
