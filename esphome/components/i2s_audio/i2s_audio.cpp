#include "i2s_audio.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include <cstring>
#include <memory>

#include "esp_timer.h"

namespace esphome::i2s_audio {

static const char *const TAG = "i2s_audio";

esp_err_t I2SAudioComponent::allocate_full_duplex_channels_(const i2s_chan_config_t &chan_cfg) {
  if (this->rx_handle_ != nullptr && this->tx_handle_ != nullptr) {
    return ESP_OK;
  }
  if (!this->full_duplex_) {
    return ESP_ERR_INVALID_STATE;
  }
  if (this->din_pin_ == I2S_GPIO_UNUSED || this->dout_pin_ == I2S_GPIO_UNUSED) {
    ESP_LOGE(TAG, "Full duplex requires both DIN and DOUT pins on the same I2S bus");
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = i2s_new_channel(&chan_cfg, &this->tx_handle_, &this->rx_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Full duplex channel allocation failed: %s", esp_err_to_name(err));
    this->tx_handle_ = nullptr;
    this->rx_handle_ = nullptr;
    this->full_duplex_dma_desc_num_ = 0;
  } else {
    this->full_duplex_dma_desc_num_ = chan_cfg.dma_desc_num;
  }
  return err;
}

esp_err_t I2SAudioComponent::initialize_full_duplex_channels_(const i2s_std_config_t &std_cfg) {
  if (this->tx_handle_ == nullptr || this->rx_handle_ == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }

  if (!this->tx_channel_initialized_) {
    esp_err_t err = i2s_channel_init_std_mode(this->tx_handle_, &std_cfg);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to initialize full duplex TX channel: %s", esp_err_to_name(err));
      return err;
    }
    this->tx_channel_initialized_ = true;
  }

  if (!this->rx_channel_initialized_) {
    esp_err_t err = i2s_channel_init_std_mode(this->rx_handle_, &std_cfg);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to initialize full duplex RX channel: %s", esp_err_to_name(err));
      return err;
    }
    this->rx_channel_initialized_ = true;
  }

  return ESP_OK;
}

esp_err_t I2SAudioComponent::setup_full_duplex_rx_channel(const i2s_chan_config_t &chan_cfg,
                                                          const i2s_std_config_t &std_cfg,
                                                          i2s_chan_handle_t *rx_handle) {
  esp_err_t err = this->allocate_full_duplex_channels_(chan_cfg);
  if (err != ESP_OK) {
    return err;
  }
  err = this->initialize_full_duplex_channels_(std_cfg);
  if (err == ESP_OK) {
    *rx_handle = this->rx_handle_;
  }
  return err;
}

esp_err_t I2SAudioComponent::setup_full_duplex_tx_channel(const i2s_chan_config_t &chan_cfg,
                                                          const i2s_std_config_t &std_cfg,
                                                          i2s_chan_handle_t *tx_handle) {
  esp_err_t err = this->allocate_full_duplex_channels_(chan_cfg);
  if (err != ESP_OK) {
    return err;
  }
  err = this->initialize_full_duplex_channels_(std_cfg);
  if (err == ESP_OK) {
    *tx_handle = this->tx_handle_;
  }
  return err;
}

esp_err_t I2SAudioComponent::ensure_full_duplex_tx_running() {
  if (!this->full_duplex_) {
    return ESP_ERR_INVALID_STATE;
  }
  if (this->tx_handle_ == nullptr || !this->tx_channel_initialized_) {
    return ESP_ERR_INVALID_STATE;
  }
  if (this->full_duplex_tx_enabled_) {
    return ESP_OK;
  }

  i2s_chan_info_t chan_info;
  size_t dma_buffer_bytes = 0;
  if (i2s_channel_get_info(this->tx_handle_, &chan_info) == ESP_OK && chan_info.total_dma_buf_size > 0 &&
      this->full_duplex_dma_desc_num_ > 0) {
    dma_buffer_bytes = chan_info.total_dma_buf_size / this->full_duplex_dma_desc_num_;
  }
  if (dma_buffer_bytes == 0) {
    ESP_LOGE(TAG, "Full duplex TX clock start failed: no DMA buffer size available");
    return ESP_ERR_INVALID_SIZE;
  }

  if (!this->full_duplex_tx_callback_registered_) {
    const i2s_event_callbacks_t callbacks = {.on_sent = I2SAudioComponent::full_duplex_on_sent_cb};
    esp_err_t err = i2s_channel_register_event_callback(this->tx_handle_, &callbacks, this);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Full duplex TX callback registration failed: %s", esp_err_to_name(err));
      return err;
    }
    this->full_duplex_tx_callback_registered_ = true;
  }

  auto silence_buffer = std::make_unique<uint8_t[]>(dma_buffer_bytes);
  if (silence_buffer == nullptr) {
    return ESP_ERR_NO_MEM;
  }
  memset(silence_buffer.get(), 0, dma_buffer_bytes);

  for (size_t i = 0; i < this->full_duplex_dma_desc_num_; i++) {
    size_t bytes_loaded = 0;
    esp_err_t err = i2s_channel_preload_data(this->tx_handle_, silence_buffer.get(), dma_buffer_bytes, &bytes_loaded);
    if (err != ESP_OK || bytes_loaded != dma_buffer_bytes) {
      ESP_LOGW(TAG, "Full duplex TX silence preload failed: %s (%u/%u bytes)", esp_err_to_name(err),
               (unsigned) bytes_loaded, (unsigned) dma_buffer_bytes);
      break;
    }
  }

  esp_err_t err = i2s_channel_enable(this->tx_handle_);
  if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
    this->full_duplex_tx_enabled_ = true;
    ESP_LOGD(TAG, "Full duplex TX clock is running");
    return ESP_OK;
  }
  ESP_LOGE(TAG, "Full duplex TX clock enable failed: %s", esp_err_to_name(err));
  return err;
}

void I2SAudioComponent::attach_full_duplex_tx_event_queue(QueueHandle_t queue, EventGroupHandle_t event_group,
                                                          EventBits_t overflow_bits) {
  this->full_duplex_tx_event_queue_ = queue;
  this->full_duplex_tx_event_group_ = event_group;
  this->full_duplex_tx_overflow_bits_ = overflow_bits;
}

void I2SAudioComponent::detach_full_duplex_tx_event_queue(QueueHandle_t queue) {
  if (this->full_duplex_tx_event_queue_ == queue) {
    this->full_duplex_tx_event_queue_ = nullptr;
    this->full_duplex_tx_event_group_ = nullptr;
    this->full_duplex_tx_overflow_bits_ = 0;
  }
}

bool IRAM_ATTR I2SAudioComponent::full_duplex_on_sent_cb(i2s_chan_handle_t handle, i2s_event_data_t *event,
                                                         void *user_ctx) {
  (void) handle;
  (void) event;
  auto *component = static_cast<I2SAudioComponent *>(user_ctx);
  QueueHandle_t queue = component->full_duplex_tx_event_queue_;
  if (queue == nullptr) {
    return false;
  }

  int64_t now = esp_timer_get_time();
  BaseType_t need_yield1 = pdFALSE;
  BaseType_t need_yield2 = pdFALSE;
  BaseType_t need_yield3 = pdFALSE;

  if (xQueueIsQueueFullFromISR(queue)) {
    int64_t dummy;
    xQueueReceiveFromISR(queue, &dummy, &need_yield1);
    if (component->full_duplex_tx_event_group_ != nullptr && component->full_duplex_tx_overflow_bits_ != 0) {
      xEventGroupSetBitsFromISR(component->full_duplex_tx_event_group_, component->full_duplex_tx_overflow_bits_,
                                &need_yield2);
    }
  }

  xQueueSendToBackFromISR(queue, &now, &need_yield3);
  return need_yield1 | need_yield2 | need_yield3;
}

void I2SAudioComponent::release_full_duplex_rx_channel(i2s_chan_handle_t rx_handle) {
  if (rx_handle == this->rx_handle_ && this->full_duplex_rx_enabled_) {
    i2s_channel_disable(this->rx_handle_);
    this->full_duplex_rx_enabled_ = false;
  }
}

void I2SAudioComponent::release_full_duplex_tx_channel(i2s_chan_handle_t tx_handle) {
  (void) tx_handle;
  // Keep TX enabled in full-duplex mode. The TX channel is the clock source for
  // external ADCs such as ES7210, and disabling it makes RX reads time out until
  // playback starts again.
}

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
