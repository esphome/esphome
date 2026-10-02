#include "i2s_audio.h"

#if defined(USE_ESP32) && defined(USE_I2S_AUDIO_FULL_DUPLEX)

#include "esphome/core/log.h"

namespace esphome::i2s_audio {

static const char *const TAG = "i2s_audio";

void I2SAudioComponent::setup() {
  if (!this->is_full_duplex())
    return;

  // ESP-IDF only shares the bit and word clocks between channels allocated in the same call, and deleting either
  // channel ends the pairing. Allocate both once here and keep them for the lifetime of the device.
  i2s_chan_config_t chan_cfg{};
  i2s_std_config_t tx_cfg{};
  i2s_std_config_t rx_cfg{};
  if (!this->audio_out_->build_full_duplex_config(chan_cfg, tx_cfg) ||
      !this->audio_in_->build_full_duplex_config(rx_cfg)) {
    ESP_LOGE(TAG, "Microphone or speaker does not support full duplex");
    this->mark_failed();
    return;
  }

  esp_err_t err = i2s_new_channel(&chan_cfg, &this->tx_handle_, &this->rx_handle_);
  if (err == ESP_OK) {
    // The first initialized channel drives the shared clocks. Use RX so the microphone runs on its own.
    err = i2s_channel_init_std_mode(this->rx_handle_, &rx_cfg);
  }
  if (err == ESP_OK) {
    err = i2s_channel_init_std_mode(this->tx_handle_, &tx_cfg);
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Full duplex channel setup failed: %s", esp_err_to_name(err));
    if (this->tx_handle_ != nullptr) {
      i2s_del_channel(this->tx_handle_);
      this->tx_handle_ = nullptr;
    }
    if (this->rx_handle_ != nullptr) {
      i2s_del_channel(this->rx_handle_);
      this->rx_handle_ = nullptr;
    }
    this->mark_failed();
  }
}

i2s_chan_handle_t I2SAudioComponent::acquire_rx_channel() {
  if (this->rx_handle_ == nullptr)
    return nullptr;
  this->rx_in_use_ = true;
  if (!this->update_rx_channel_()) {
    this->rx_in_use_ = false;
    return nullptr;
  }
  return this->rx_handle_;
}

void I2SAudioComponent::release_rx_channel() {
  this->rx_in_use_ = false;
  this->update_rx_channel_();
}

i2s_chan_handle_t I2SAudioComponent::acquire_tx_channel() {
  // Speakers sharing the bus take turns; a second one must not get the channel while the first is playing
  if (this->tx_handle_ == nullptr || this->tx_in_use_)
    return nullptr;
  this->tx_in_use_ = true;
  if (!this->update_rx_channel_()) {
    this->tx_in_use_ = false;
    return nullptr;
  }
  return this->tx_handle_;
}

void I2SAudioComponent::release_tx_channel() {
  // The speaker task may have left the channel disabled already, so an invalid state error is expected here
  i2s_channel_disable(this->tx_handle_);
  this->tx_in_use_ = false;
  this->update_rx_channel_();
}

bool I2SAudioComponent::update_rx_channel_() {
  const bool needed = this->rx_in_use_ || this->tx_in_use_;
  if (needed == this->rx_enabled_)
    return true;

  esp_err_t err = needed ? i2s_channel_enable(this->rx_handle_) : i2s_channel_disable(this->rx_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to %s RX channel: %s", needed ? LOG_STR_LITERAL("enable") : LOG_STR_LITERAL("disable"),
             esp_err_to_name(err));
    return false;
  }
  this->rx_enabled_ = needed;
  return true;
}

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32 && USE_I2S_AUDIO_FULL_DUPLEX
