#include "remote_transmitter.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32

namespace esphome {
namespace remote_transmitter {

static const char *const TAG = "remote_transmitter";

void RemoteTransmitterComponent::setup() { this->configure_rmt_(); }

void RemoteTransmitterComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Remote Transmitter...");
  ESP_LOGCONFIG(TAG, "  Channel: %d", this->channel_);
  ESP_LOGCONFIG(TAG, "  RMT memory blocks: %d", this->mem_block_num_);
  ESP_LOGCONFIG(TAG, "  Clock divider: %u", this->clock_divider_);
  LOG_PIN("  Pin: ", this->pin_);

  if (this->current_carrier_frequency_ != 0 && this->carrier_duty_percent_ != 100) {
    ESP_LOGCONFIG(TAG, "    Carrier Duty: %u%%", this->carrier_duty_percent_);
  }

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Configuring RMT driver failed: %s", esp_err_to_name(this->error_code_));
  }
}

void RemoteTransmitterComponent::configure_rmt_() {
  rmt_config_t c{};

  this->config_rmt(c);
  c.rmt_mode = RMT_MODE_TX;
  c.gpio_num = gpio_num_t(this->pin_->get_pin());
  c.tx_config.loop_en = false;

  if (this->current_carrier_frequency_ == 0 || this->carrier_duty_percent_ == 100) {
    c.tx_config.carrier_en = false;
  } else {
    c.tx_config.carrier_en = true;
    c.tx_config.carrier_freq_hz = this->current_carrier_frequency_;
    c.tx_config.carrier_duty_percent = this->carrier_duty_percent_;
  }

  c.tx_config.idle_output_en = true;
  if (!this->pin_->is_inverted()) {
    c.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH;
    c.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  } else {
    c.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;
    c.tx_config.idle_level = RMT_IDLE_LEVEL_HIGH;
    this->inverted_ = true;
  }

  esp_err_t error = rmt_config(&c);
  if (error != ESP_OK) {
    this->error_code_ = error;
    this->mark_failed();
    return;
  }

  if (!this->initialized_) {
    error = rmt_driver_install(this->channel_, 0, 0);
    if (error != ESP_OK) {
      this->error_code_ = error;
      this->mark_failed();
      return;
    }
    this->initialized_ = true;
  }
}

void RemoteTransmitterComponent::send_internal(uint32_t send_times, uint32_t send_wait) {
  if (this->is_failed())
    return;

  if (this->current_carrier_frequency_ != this->temp_.get_carrier_frequency()) {
    this->current_carrier_frequency_ = this->temp_.get_carrier_frequency();
    this->configure_rmt_();
  }

  this->rmt_temp_.clear();
  this->rmt_temp_.reserve((this->temp_.get_data().size() + 1) / 2);
  uint32_t rmt_i = 0;
  rmt_item32_t rmt_item;

  for (int32_t val : this->temp_.get_data()) {
    bool level = val >= 0;
    if (!level)
      val = -val;
    val = this->from_microseconds_(static_cast<uint32_t>(val));

    do {
      int32_t item = std::min(val, int32_t(32767));
      val -= item;

      if (rmt_i % 2 == 0) {
        rmt_item.level0 = static_cast<uint32_t>(level ^ this->inverted_);
        rmt_item.duration0 = static_cast<uint32_t>(item);
      } else {
        rmt_item.level1 = static_cast<uint32_t>(level ^ this->inverted_);
        rmt_item.duration1 = static_cast<uint32_t>(item);
        this->rmt_temp_.push_back(rmt_item);
      }
      rmt_i++;
    } while (val != 0);
  }

  if (rmt_i % 2 == 1) {
    rmt_item.level1 = 0;
    rmt_item.duration1 = 0;
    this->rmt_temp_.push_back(rmt_item);
  }

  if ((this->rmt_temp_.data() == nullptr) || this->rmt_temp_.empty()) {
    ESP_LOGE(TAG, "Empty data");
    return;
  }
  for (uint32_t i = 0; i < send_times; i++) {
    esp_err_t error = rmt_write_items(this->channel_, this->rmt_temp_.data(), this->rmt_temp_.size(), true);
    if (error != ESP_OK) {
      ESP_LOGW(TAG, "rmt_write_items failed: %s", esp_err_to_name(error));
      this->status_set_warning();
    } else {
      this->status_clear_warning();
    }
    if (i + 1 < send_times)
      delayMicroseconds(send_wait);
  }
}

}  // namespace remote_transmitter
}  // namespace esphome

#endif
