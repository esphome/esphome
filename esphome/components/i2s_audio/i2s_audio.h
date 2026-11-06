#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include <esp_idf_version.h>
#include <driver/i2s_std.h>
#ifdef USE_I2S_AUDIO_FULL_DUPLEX
#include <atomic>
#endif

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
  i2s_clock_src_t get_clock_source_() const {
#ifdef I2S_CLK_SRC_APLL
    if (this->use_apll_) {
      return I2S_CLK_SRC_APLL;
    }
#endif
    return I2S_CLK_SRC_DEFAULT;
  }

  i2s_role_t i2s_role_{};
  i2s_slot_mode_t slot_mode_;
  i2s_std_slot_mask_t std_slot_mask_;
  i2s_slot_bit_width_t slot_bit_width_;
  uint32_t sample_rate_;
  bool use_apll_;
  i2s_mclk_multiple_t mclk_multiple_;
};

class I2SAudioIn : public I2SAudioBase {
#ifdef USE_I2S_AUDIO_FULL_DUPLEX
 public:
  /// @brief Builds the RX configuration the parent uses to set up a full duplex channel pair.
  virtual void build_full_duplex_config(i2s_std_config_t &std_cfg) = 0;
  /// @brief Called by the parent on the main loop just before it enables the full duplex RX channel, which clears
  /// its receive queue. The channel is still disabled, so event callbacks can be registered on it.
  virtual void full_duplex_rx_enabling(i2s_chan_handle_t rx_handle, uint32_t dma_desc_num) = 0;
#endif
};

class I2SAudioOut : public I2SAudioBase {
#ifdef USE_I2S_AUDIO_FULL_DUPLEX
 public:
  /// @brief Builds the TX configuration the parent uses to set up a full duplex channel pair. The channel
  /// configuration (DMA layout, role, interrupt priority) is shared by both channels. Validation only puts
  /// standard speakers on a full duplex bus, so the SPDIF speaker never gets here.
  virtual void build_full_duplex_config(i2s_chan_config_t &chan_cfg, i2s_std_config_t &std_cfg) {}
#endif
};

class I2SAudioComponent final : public Component {
 public:
#ifdef USE_I2S_AUDIO_FULL_DUPLEX
  using TxSentCallback = bool (*)(i2s_chan_handle_t, i2s_event_data_t *, void *);
  void set_tx_sent_callback(TxSentCallback callback, void *context) {
    if (callback == nullptr) {
      this->tx_sent_callback_.store(nullptr, std::memory_order_release);
      this->tx_sent_callback_context_.store(nullptr, std::memory_order_release);
      return;
    }
    this->tx_sent_callback_context_.store(context, std::memory_order_release);
    this->tx_sent_callback_.store(callback, std::memory_order_release);
  }
  bool is_keep_tx_active() const { return this->keep_tx_active_; }
  void setup() override;

  void set_audio_in(I2SAudioIn *audio_in) { this->audio_in_ = audio_in; }
  void set_audio_out(I2SAudioOut *audio_out) { this->audio_out_ = audio_out; }
  void set_keep_tx_active(bool keep_tx_active) { this->keep_tx_active_ = keep_tx_active; }
  void set_continuous_tx_running(bool running) {
    this->continuous_tx_running_.store(running, std::memory_order_release);
  }

  /// @brief True when a microphone and a speaker share this bus at the same time.
  bool is_full_duplex() const { return this->audio_in_ != nullptr && this->audio_out_ != nullptr; }

  /// @brief Enables the full duplex RX channel. Main loop only.
  /// @return The enabled RX handle, or nullptr if the channel pair is unavailable
  i2s_chan_handle_t acquire_rx_channel();
  /// @brief Releases RX; it remains enabled while the TX side uses the bus. Main loop only.
  void release_rx_channel();

  /// @brief Hands over full duplex TX; it is already running with keep_tx_active, otherwise the caller starts it.
  /// Main loop only.
  /// @return The TX handle, or nullptr if the channel pair is unavailable or another speaker holds it
  i2s_chan_handle_t acquire_tx_channel();
  /// @brief Releases TX; it stays clocked with keep_tx_active, otherwise it is disabled. Only the speaker that
  /// acquired the channel may call this. Main loop only.
  void release_tx_channel();
  /// @brief True while another speaker holds the full duplex TX channel
  bool is_tx_in_use() const { return this->tx_in_use_; }
#endif
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

  void set_mclk_pin(int pin) { this->mclk_pin_ = pin; }
  void set_bclk_pin(int pin) { this->bclk_pin_ = pin; }
  void set_lrclk_pin(int pin) { this->lrclk_pin_ = pin; }
  void set_port(int port) { this->port_ = port; }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
  int get_port() const { return this->port_; }
#else
  i2s_port_t get_port() const { return static_cast<i2s_port_t>(this->port_); }
#endif

  void lock() { this->lock_.lock(); }
  bool try_lock() { return this->lock_.try_lock(); }
  void unlock() { this->lock_.unlock(); }

 protected:
  Mutex lock_;

#ifdef USE_I2S_AUDIO_FULL_DUPLEX
  /// @brief Marks one side active and brings the shared clocks up; undoes the mark if that fails.
  i2s_chan_handle_t acquire_(bool &in_use, i2s_chan_handle_t handle);
  /// @brief Enables RX while either side uses the bus (TX owns the clocks when keep_tx_active is set).
  bool update_rx_channel_();
  /// @brief Run the TX clock with zeroed DMA buffers while no speaker owns the channel.
  bool start_idle_tx_();
  static bool on_bus_tx_sent(i2s_chan_handle_t handle, i2s_event_data_t *event, void *context);

  I2SAudioIn *audio_in_{nullptr};
  I2SAudioOut *audio_out_{nullptr};
  i2s_chan_handle_t rx_handle_{nullptr};
  i2s_chan_handle_t tx_handle_{nullptr};
  uint32_t dma_desc_num_{0};
  bool rx_in_use_{false};
  bool tx_in_use_{false};
  bool rx_enabled_{false};
  bool keep_tx_active_{false};
  bool idle_tx_restore_warning_{false};
  std::atomic<bool> continuous_tx_running_{false};
  std::atomic<TxSentCallback> tx_sent_callback_{nullptr};
  std::atomic<void *> tx_sent_callback_context_{nullptr};
#endif
  int mclk_pin_{I2S_GPIO_UNUSED};
  int bclk_pin_{I2S_GPIO_UNUSED};
  int lrclk_pin_;
  int port_{};
};

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
