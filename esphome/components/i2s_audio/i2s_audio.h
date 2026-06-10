#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include <esp_idf_version.h>
#include <driver/i2s_std.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>

namespace esphome::i2s_audio {

class I2SAudioComponent;

class I2SAudioBase : public Parented<I2SAudioComponent> {
 public:
  void set_i2s_role(i2s_role_t role) { this->i2s_role_ = role; }
  void set_slot_mode(i2s_slot_mode_t slot_mode) { this->slot_mode_ = slot_mode; }
  void set_std_slot_mask(i2s_std_slot_mask_t std_slot_mask) { this->std_slot_mask_ = std_slot_mask; }
  void set_slot_bit_width(i2s_slot_bit_width_t slot_bit_width) { this->slot_bit_width_ = slot_bit_width; }
  void set_sample_rate(uint32_t sample_rate) { this->sample_rate_ = sample_rate; }
  void set_use_apll(uint32_t use_apll) { this->use_apll_ = use_apll; }
  void set_mclk_multiple(i2s_mclk_multiple_t mclk_multiple) { this->mclk_multiple_ = mclk_multiple; }

 protected:
  i2s_role_t i2s_role_{};
  i2s_slot_mode_t slot_mode_;
  i2s_std_slot_mask_t std_slot_mask_;
  i2s_slot_bit_width_t slot_bit_width_;
  uint32_t sample_rate_;
  bool use_apll_;
  i2s_mclk_multiple_t mclk_multiple_;
};

class I2SAudioIn : public I2SAudioBase {};

class I2SAudioOut : public I2SAudioBase {};

class I2SAudioComponent : public Component {
 public:
  i2s_std_gpio_config_t get_pin_config() const {
    return {.mclk = (gpio_num_t) this->mclk_pin_,
            .bclk = (gpio_num_t) this->bclk_pin_,
            .ws = (gpio_num_t) this->lrclk_pin_,
            .dout = I2S_GPIO_UNUSED,  // add local ports
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            }};
  }
  i2s_std_gpio_config_t get_full_duplex_pin_config() const {
    return {.mclk = (gpio_num_t) this->mclk_pin_,
            .bclk = (gpio_num_t) this->bclk_pin_,
            .ws = (gpio_num_t) this->lrclk_pin_,
            .dout = (gpio_num_t) this->dout_pin_,
            .din = (gpio_num_t) this->din_pin_,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            }};
  }

  void set_mclk_pin(int pin) { this->mclk_pin_ = pin; }
  void set_bclk_pin(int pin) { this->bclk_pin_ = pin; }
  void set_lrclk_pin(int pin) { this->lrclk_pin_ = pin; }
  void set_din_pin(int pin) { this->din_pin_ = pin; }
  void set_dout_pin(int pin) { this->dout_pin_ = pin; }
  void set_port(int port) { this->port_ = port; }
  void set_full_duplex(bool full_duplex) { this->full_duplex_ = full_duplex; }
  bool is_full_duplex() const { return this->full_duplex_; }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
  int get_port() const { return this->port_; }
#else
  i2s_port_t get_port() const { return static_cast<i2s_port_t>(this->port_); }
#endif

  void lock() { this->lock_.lock(); }
  bool try_lock() { return this->lock_.try_lock(); }
  void unlock() { this->lock_.unlock(); }

  esp_err_t setup_full_duplex_rx_channel(const i2s_chan_config_t &chan_cfg, const i2s_std_config_t &std_cfg,
                                         i2s_chan_handle_t *rx_handle);
  esp_err_t setup_full_duplex_tx_channel(const i2s_chan_config_t &chan_cfg, const i2s_std_config_t &std_cfg,
                                         i2s_chan_handle_t *tx_handle);
  esp_err_t ensure_full_duplex_tx_running();
  void attach_full_duplex_tx_event_queue(QueueHandle_t queue, EventGroupHandle_t event_group,
                                         EventBits_t overflow_bits);
  void detach_full_duplex_tx_event_queue(QueueHandle_t queue);
  size_t get_full_duplex_dma_desc_num() const { return this->full_duplex_dma_desc_num_; }
  void mark_full_duplex_rx_running() { this->full_duplex_rx_enabled_ = true; }
  void release_full_duplex_rx_channel(i2s_chan_handle_t rx_handle);
  void release_full_duplex_tx_channel(i2s_chan_handle_t tx_handle);

 protected:
  esp_err_t allocate_full_duplex_channels_(const i2s_chan_config_t &chan_cfg);
  esp_err_t initialize_full_duplex_channels_(const i2s_std_config_t &std_cfg);
  static bool full_duplex_on_sent_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx);

  Mutex lock_;

  I2SAudioIn *audio_in_{nullptr};
  I2SAudioOut *audio_out_{nullptr};
  int mclk_pin_{I2S_GPIO_UNUSED};
  int bclk_pin_{I2S_GPIO_UNUSED};
  int din_pin_{I2S_GPIO_UNUSED};
  int dout_pin_{I2S_GPIO_UNUSED};
  int lrclk_pin_;
  int port_{};
  bool full_duplex_{false};

  i2s_chan_handle_t rx_handle_{nullptr};
  i2s_chan_handle_t tx_handle_{nullptr};
  bool rx_channel_initialized_{false};
  bool tx_channel_initialized_{false};
  bool full_duplex_tx_enabled_{false};
  bool full_duplex_rx_enabled_{false};
  bool full_duplex_tx_callback_registered_{false};
  size_t full_duplex_dma_desc_num_{0};
  QueueHandle_t full_duplex_tx_event_queue_{nullptr};
  EventGroupHandle_t full_duplex_tx_event_group_{nullptr};
  EventBits_t full_duplex_tx_overflow_bits_{0};
};

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
