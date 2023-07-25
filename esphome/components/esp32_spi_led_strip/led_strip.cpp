#ifdef USE_ESP32
#include <cinttypes>

#include "esphome/core/log.h"

#include "led_strip.h"

namespace esphome {
namespace esp32_spi_led_strip {

static const char *const TAG = "esp32_spi_led_strip";

inline static const char *rgb_order_to_string(RGBOrder order) {
  switch (order) {
    case RGBOrder::ORDER_RGB:
      return "RGB";
    case RGBOrder::ORDER_RBG:
      return "RBG";
    case RGBOrder::ORDER_GRB:
      return "GRB";
    case RGBOrder::ORDER_GBR:
      return "GBR";
    case RGBOrder::ORDER_BGR:
      return "BGR";
    case RGBOrder::ORDER_BRG:
      return "BRG";
    default:
      return "UNKNOWN";
  }
}

void LedStripSpi::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP32 SPI LED Strip:");
  ESP_LOGCONFIG(TAG, "  Data Pin: GPIO%d", this->data_pin_);
  ESP_LOGCONFIG(TAG, "  Clock Pin: GPIO%d", this->clock_pin_);
  ESP_LOGCONFIG(TAG, "  Number of LEDs: %d" PRId16, this->num_leds_);
  ESP_LOGCONFIG(TAG, "  RGB Order: %s", rgb_order_to_string(this->rgb_order_));
  if (this->max_refresh_rate_) {
    ESP_LOGCONFIG(TAG, "  Max Refresh Rate: %" PRIu32 " Hz", this->max_refresh_rate_);
  }
}

void LedStripSpi::setup() {
  ESP_LOGD(TAG, "Setting up ESP32 SPI LED Strip...");

  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);

  this->effect_data_ = allocator.allocate(this->num_leds_);
  if (this->effect_data_ == nullptr) {
    ESP_LOGE(TAG, "Failed allocate effect data");
    this->mark_failed();
    return;
  }
  memset(this->effect_data_, 0, this->num_leds_);

  this->buf_ = allocator.allocate(this->buf_size_());
  if (this->buf_ == nullptr) {
    ESP_LOGE(TAG, "Failed allocate LED buffer");
    allocator.deallocate(this->effect_data_, this->num_leds_);
    this->effect_data_ = nullptr;
    this->mark_failed();
    return;
  }
  memset(this->buf_, 0, this->buf_size_());

  if (!this->spi_init_()) {
    allocator.deallocate(this->effect_data_, this->num_leds_);
    this->effect_data_ = nullptr;
    allocator.deallocate(this->buf_, this->buf_size_());
    this->buf_ = nullptr;
    this->mark_failed();
    return;
  }

  // clear all pixels and set brightness to maximum, skipping start frame
  for (int i = 1; i <= this->num_leds_; i++) {
    this->buf_[i * FRAME_SIZE] = 0xFF;
  }
  this->spi_flush_();
}

bool LedStripSpi::spi_init_() {
  spi_bus_config_t bus_cfg = {
      .mosi_io_num = this->data_pin_,
      .miso_io_num = -1,
      .sclk_io_num = this->clock_pin_,
      .data2_io_num = -1,
      .data3_io_num = -1,
      .data4_io_num = -1,
      .data5_io_num = -1,
      .data6_io_num = -1,
      .data7_io_num = -1,
      .max_transfer_sz = this->buf_size_(),
      .flags = SPICOMMON_BUSFLAG_MASTER,
      .intr_flags = 0,
  };

  auto err = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed initialize SPI bus: %d, %s", err, esp_err_to_name(err));
    return false;
  }

  spi_device_interface_config_t dev_cfg = {};
  dev_cfg.clock_speed_hz = 1000000 * 10;  // 10Mhz
  dev_cfg.queue_size = 1;
  dev_cfg.spics_io_num = -1;
  dev_cfg.mode = 3;

  err = spi_bus_add_device(SPI2_HOST, &dev_cfg, &this->spi_device_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed add SPI bus device: %d, %s", err, esp_err_to_name(err));
    return false;
  }

  return true;
}

void LedStripSpi::spi_flush_() {
  spi_transaction_t tx{};
  tx.length = this->buf_size_() * 8;
  tx.tx_buffer = this->buf_;
  spi_device_queue_trans(this->spi_device_, &tx, portMAX_DELAY);
}

void LedStripSpi::write_state(light::LightState *state) {
  // protect from refreshing too often
  if (this->max_refresh_rate_) {
    uint32_t now = micros();
    if ((now - this->last_refresh_) < this->max_refresh_rate_) {
      // try again next loop iteration, so that this change won't get lost
      this->schedule_show();
      return;
    }
    this->last_refresh_ = now;
  }

  this->mark_shown_();

  ESP_LOGVV(TAG, "Writing RGB values to bus...");
  this->spi_flush_();
}

enum : size_t { OFFSET_BRIGHTNESS = 0, OFFSET_B = 1, OFFSET_G = 2, OFFSET_R = 3 };

light::ESPColorView LedStripSpi::get_view_internal(int32_t index) const {
  size_t r, g, b;
  switch (this->rgb_order_) {
    case RGBOrder::ORDER_RBG:
      r = OFFSET_R;
      g = OFFSET_B;
      b = OFFSET_G;
      break;
    case RGBOrder::ORDER_GRB:
      r = OFFSET_G;
      g = OFFSET_R;
      b = OFFSET_B;
      break;
    case RGBOrder::ORDER_GBR:
      r = OFFSET_G;
      g = OFFSET_B;
      b = OFFSET_R;
      break;
    case RGBOrder::ORDER_BGR:
      r = OFFSET_B;
      g = OFFSET_G;
      b = OFFSET_R;
      break;
    case RGBOrder::ORDER_BRG:
      r = OFFSET_B;
      g = OFFSET_R;
      b = OFFSET_G;
      break;
    default:  // RGBOrder::ORDER_RGB
      r = OFFSET_R;
      g = OFFSET_G;
      b = OFFSET_B;
      break;
  }
  auto offset = (index + 1) * FRAME_SIZE;  // skip start frame
  return {
      this->buf_ + offset + r,     // red
      this->buf_ + offset + g,     // green
      this->buf_ + offset + b,     // blue
      nullptr,                     // white
      &this->effect_data_[index],  // effect data
      &this->correction_,          // color correction
  };
}

}  // namespace esp32_spi_led_strip
}  // namespace esphome
#endif
