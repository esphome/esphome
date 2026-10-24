#include "i2s_audio.h"

#if defined(USE_ESP32) && defined(USE_I2S_AUDIO_FULL_DUPLEX)

#include "esphome/core/log.h"

namespace esphome::i2s_audio {

ESPHOME_LOG_TAG(TAG, "i2s_audio");

void I2SAudioComponent::setup() {
  if (!this->is_full_duplex())
    return;

  // ESP-IDF only shares the bit and word clocks between channels allocated in the same call, and deleting either
  // channel ends the pairing. Allocate both once here and keep them for the lifetime of the device.
  i2s_chan_config_t chan_cfg{};
  i2s_std_config_t tx_cfg{};
  i2s_std_config_t rx_cfg{};
  this->audio_out_->build_full_duplex_config(chan_cfg, tx_cfg);
  this->audio_in_->build_full_duplex_config(rx_cfg);

  esp_err_t err = i2s_new_channel(&chan_cfg, &this->tx_handle_, &this->rx_handle_);
  // On a continuously clocked bus TX must be initialized first: it owns the clocks used by RX.
  if (this->keep_tx_active_) {
    if (err == ESP_OK)
      err = i2s_channel_init_std_mode(this->tx_handle_, &tx_cfg);
    if (err == ESP_OK)
      err = i2s_channel_init_std_mode(this->rx_handle_, &rx_cfg);
  } else {
    if (err == ESP_OK)
      err = i2s_channel_init_std_mode(this->rx_handle_, &rx_cfg);
    if (err == ESP_OK)
      err = i2s_channel_init_std_mode(this->tx_handle_, &tx_cfg);
  }
  if (err == ESP_OK && this->keep_tx_active_ && !this->start_idle_tx_())
    err = ESP_FAIL;
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

i2s_chan_handle_t I2SAudioComponent::acquire_(bool &in_use, i2s_chan_handle_t handle) {
  if (handle == nullptr)
    return nullptr;
  in_use = true;
  if (!this->update_rx_channel_()) {
    in_use = false;
    return nullptr;
  }
  return handle;
}

i2s_chan_handle_t I2SAudioComponent::acquire_rx_channel() { return this->acquire_(this->rx_in_use_, this->rx_handle_); }

void I2SAudioComponent::release_rx_channel() {
  this->rx_in_use_ = false;
  this->update_rx_channel_();
}

i2s_chan_handle_t I2SAudioComponent::acquire_tx_channel() {
  // Speakers sharing the bus take turns; a second one must not get the channel while the first is playing
  if (this->tx_in_use_)
    return nullptr;
  return this->acquire_(this->tx_in_use_, this->tx_handle_);
}

void I2SAudioComponent::release_tx_channel() {
  if (this->keep_tx_active_) {
    this->set_tx_sent_callback(nullptr, nullptr);
    this->tx_in_use_ = false;
    // A failed DMA resync can leave TX disabled. Restore the clocks before the microphone keeps listening.
    if (!this->continuous_tx_running_.load(std::memory_order_acquire)) {
      const esp_err_t err = i2s_channel_enable(this->tx_handle_);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to restore idle TX: %s", esp_err_to_name(err));
        this->status_set_warning();
        this->idle_tx_restore_warning_ = true;
      } else {
        this->set_continuous_tx_running(true);
        if (this->idle_tx_restore_warning_) {
          this->status_clear_warning();
          this->idle_tx_restore_warning_ = false;
        }
      }
    }
    this->update_rx_channel_();
    return;
  }
  // The speaker task may have left the channel disabled already, so an invalid state error is expected here
  i2s_channel_disable(this->tx_handle_);
  this->tx_in_use_ = false;
  this->update_rx_channel_();
}

bool I2SAudioComponent::start_idle_tx_() {
  // Keep one ISR callback registered while TX runs, forwarding completion events only
  // while a speaker owns the channel.
  const i2s_event_callbacks_t callbacks{.on_sent = on_bus_tx_sent};
  esp_err_t err = i2s_channel_register_event_callback(this->tx_handle_, &callbacks, this);
  if (err == ESP_OK)
    err = i2s_channel_enable(this->tx_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start idle TX: %s", esp_err_to_name(err));
    return false;
  }
  this->set_continuous_tx_running(true);
  return true;
}

bool IRAM_ATTR I2SAudioComponent::on_bus_tx_sent(i2s_chan_handle_t handle, i2s_event_data_t *event, void *context) {
  auto *self = static_cast<I2SAudioComponent *>(context);
#ifdef USE_I2S_AUDIO_TX_DMA_OBSERVER
  self->notify_tx_dma_sent(event);
#endif
  const auto callback = self->tx_sent_callback_.load(std::memory_order_acquire);
  auto *const callback_context = self->tx_sent_callback_context_.load(std::memory_order_acquire);
  if (callback != nullptr && callback_context != nullptr)
    return callback(handle, event, callback_context);
  return false;
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
