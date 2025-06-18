#include "remote_transmitter.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32
#include <driver/gpio.h>

namespace esphome {
namespace remote_transmitter {

static const char *const TAG = "remote_transmitter";

void RemoteTransmitterComponent::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");
  this->inverted_ = this->pin_->is_inverted();
  this->configure_rmt_();
}

void RemoteTransmitterComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Remote Transmitter:");
  ESP_LOGCONFIG(TAG,
                "  Clock resolution: %" PRIu32 " hz\n"
                "  RMT symbols: %" PRIu32,
                this->clock_resolution_, this->rmt_symbols_);
  LOG_PIN("  Pin: ", this->pin_);

  if (this->current_carrier_frequency_ != 0 && this->carrier_duty_percent_ != 100) {
    ESP_LOGCONFIG(TAG, "    Carrier Duty: %u%%", this->carrier_duty_percent_);
  }

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Configuring RMT driver failed: %s (%s)", esp_err_to_name(this->error_code_),
             this->error_string_.c_str());
  }
}

void RemoteTransmitterComponent::digital_write(bool value) {
  rmt_symbol_word_t symbol = {
      .duration0 = 1,
      .level0 = value,
      .duration1 = 0,
      .level1 = value,
  };
  rmt_transmit_config_t config;
  memset(&config, 0, sizeof(config));
  config.loop_count = 0;
  config.flags.eot_level = value;
  esp_err_t error = rmt_transmit(this->channel_, this->encoder_, &symbol, sizeof(symbol), &config);
  if (error != ESP_OK) {
    ESP_LOGW(TAG, "rmt_transmit failed: %s", esp_err_to_name(error));
    this->status_set_warning();
  }
  error = rmt_tx_wait_all_done(this->channel_, -1);
  if (error != ESP_OK) {
    ESP_LOGW(TAG, "rmt_tx_wait_all_done failed: %s", esp_err_to_name(error));
    this->status_set_warning();
  }
}

void RemoteTransmitterComponent::configure_rmt_() {
  esp_err_t error;

  if (!this->initialized_) {
    bool open_drain = (this->pin_->get_flags() & gpio::FLAG_OPEN_DRAIN) != 0;
    rmt_tx_channel_config_t channel;
    memset(&channel, 0, sizeof(channel));
    channel.clk_src = RMT_CLK_SRC_DEFAULT;
    channel.resolution_hz = this->clock_resolution_;
    channel.gpio_num = gpio_num_t(this->pin_->get_pin());
    channel.mem_block_symbols = this->rmt_symbols_;
    channel.trans_queue_depth = 1;
    channel.flags.io_loop_back = open_drain;
    channel.flags.io_od_mode = open_drain;
    channel.flags.invert_out = 0;
    channel.flags.with_dma = this->with_dma_;
    channel.intr_priority = 0;
    error = rmt_new_tx_channel(&channel, &this->channel_);
    if (error != ESP_OK) {
      this->error_code_ = error;
      if (error == ESP_ERR_NOT_FOUND) {
        this->error_string_ = "out of RMT symbol memory";
      } else {
        this->error_string_ = "in rmt_new_tx_channel";
      }
      this->mark_failed();
      return;
    }
    if (this->pin_->get_flags() & gpio::FLAG_PULLUP) {
      gpio_pullup_en(gpio_num_t(this->pin_->get_pin()));
    } else {
      gpio_pullup_dis(gpio_num_t(this->pin_->get_pin()));
    }

    rmt_copy_encoder_config_t encoder;
    memset(&encoder, 0, sizeof(encoder));
    error = rmt_new_copy_encoder(&encoder, &this->encoder_);
    if (error != ESP_OK) {
      this->error_code_ = error;
      this->error_string_ = "in rmt_new_copy_encoder";
      this->mark_failed();
      return;
    }

    error = rmt_enable(this->channel_);
    if (error != ESP_OK) {
      this->error_code_ = error;
      this->error_string_ = "in rmt_enable";
      this->mark_failed();
      return;
    }
    this->digital_write(open_drain || this->inverted_);
    this->initialized_ = true;
  }

  if (this->current_carrier_frequency_ == 0 || this->carrier_duty_percent_ == 100) {
    error = rmt_apply_carrier(this->channel_, nullptr);
  } else {
    rmt_carrier_config_t carrier;
    memset(&carrier, 0, sizeof(carrier));
    carrier.frequency_hz = this->current_carrier_frequency_;
    carrier.duty_cycle = (float) this->carrier_duty_percent_ / 100.0f;
    carrier.flags.polarity_active_low = this->inverted_;
    carrier.flags.always_on = 1;
    error = rmt_apply_carrier(this->channel_, &carrier);
  }
  if (error != ESP_OK) {
    this->error_code_ = error;
    this->error_string_ = "in rmt_apply_carrier";
    this->mark_failed();
    return;
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
  rmt_symbol_word_t rmt_item;

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
  this->transmit_trigger_->trigger();
  for (uint32_t i = 0; i < send_times; i++) {
    rmt_transmit_config_t config;
    memset(&config, 0, sizeof(config));
    config.loop_count = 0;
    config.flags.eot_level = this->eot_level_;
    esp_err_t error = rmt_transmit(this->channel_, this->encoder_, this->rmt_temp_.data(),
                                   this->rmt_temp_.size() * sizeof(rmt_symbol_word_t), &config);
    if (error != ESP_OK) {
      ESP_LOGW(TAG, "rmt_transmit failed: %s", esp_err_to_name(error));
      this->status_set_warning();
    } else {
      this->status_clear_warning();
    }
    error = rmt_tx_wait_all_done(this->channel_, -1);
    if (error != ESP_OK) {
      ESP_LOGW(TAG, "rmt_tx_wait_all_done failed: %s", esp_err_to_name(error));
      this->status_set_warning();
    }
    if (i + 1 < send_times)
      delayMicroseconds(send_wait);
  }
  this->complete_trigger_->trigger();
}

}  // namespace remote_transmitter
}  // namespace esphome

#endif
