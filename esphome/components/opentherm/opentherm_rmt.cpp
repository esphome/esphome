#include "opentherm_rmt.h"

#if defined(USE_ESP32) && SOC_RMT_SUPPORTED

#include <cstring>
#include <driver/gpio.h>
#include <esp_clk_tree.h>
#include "esphome/core/wake.h"

namespace esphome::opentherm {

ESPHOME_LOG_TAG(TAG, "opentherm");

bool IRAM_ATTR HOT OpenThermRMT::rx_done_isr(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata,
                                             void *user_ctx) {
  auto *arg = static_cast<OpenThermRMT *>(user_ctx);
  arg->symbol_count_ = edata->num_symbols;
  arg->rx_armed_ = false;
  arg->rx_done_ = true;
  BaseType_t task_woken = pdFALSE;
  wake_loop_isrsafe(&task_woken);
  return task_woken != pdFALSE;
}

bool IRAM_ATTR HOT OpenThermRMT::tx_done_isr(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata,
                                             void *user_ctx) {
  auto *arg = static_cast<OpenThermRMT *>(user_ctx);
  if (!arg->transmitted()) {
    return false;  // Not the end of a request: the idle level was written, or the request was abandoned.
  }
  // The conversation is listening from here, and the loop arms the capture on its next pass.
  BaseType_t task_woken = pdFALSE;
  wake_loop_isrsafe(&task_woken);
  return task_woken != pdFALSE;
}

bool OpenThermRMT::initialize() { return OpenThermBase::initialize() && this->init_rx_() && this->init_tx_(); }

bool OpenThermRMT::init_rx_() {
  rmt_rx_channel_config_t channel;
  memset(&channel, 0, sizeof(channel));
  channel.clk_src = RMT_CLK_SRC_DEFAULT;
  channel.resolution_hz = this->clock_resolution_;
  channel.mem_block_symbols = this->rmt_symbols_;
  channel.gpio_num = gpio_num_t(this->in_pin_->get_pin());
  channel.intr_priority = 0;
  // A reversed input is corrected when the edges are decoded.
  channel.flags.invert_in = 0;

  esp_err_t error = rmt_new_rx_channel(&channel, &this->rx_);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "RMT driver failed: %s", esp_err_to_name(error));
    return false;
  }
  // The driver forces a pullup on the input pin; restore what the configuration asked for.
  if (this->in_pin_->get_flags() & gpio::FLAG_PULLUP) {
    gpio_pullup_en(gpio_num_t(this->in_pin_->get_pin()));
  } else {
    gpio_pullup_dis(gpio_num_t(this->in_pin_->get_pin()));
  }

  rmt_rx_event_callbacks_t callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.on_recv_done = rx_done_isr;
  error = rmt_rx_register_event_callbacks(this->rx_, &callbacks, this);
  if (error == ESP_OK) {
    error = rmt_enable(this->rx_);
  }
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "RMT driver failed: %s", esp_err_to_name(error));
    return false;
  }

  // The glitch filter counts ticks of the RMT clock source in an 8 bit register, so 255 ticks is the ceiling -
  // about 3.2 us at 80 MHz. Ask for the maximum, which is as much immunity as the peripheral offers.
  uint32_t clock_hz = 80000000;
  esp_clk_tree_src_get_freq_hz((soc_module_clk_t) RMT_CLK_SRC_DEFAULT, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED,
                               &clock_hz);
  memset(&this->rx_config_, 0, sizeof(this->rx_config_));
  this->rx_config_.signal_range_min_ns = UINT8_MAX * 1000u / (clock_hz / 1000000u);
  this->rx_config_.signal_range_max_ns = FRAME_IDLE_US * 1000;
  return true;
}

bool OpenThermRMT::init_tx_() {
  rmt_tx_channel_config_t channel;
  memset(&channel, 0, sizeof(channel));
  channel.clk_src = RMT_CLK_SRC_DEFAULT;
  channel.resolution_hz = this->clock_resolution_;
  channel.mem_block_symbols = this->rmt_symbols_;
  channel.gpio_num = gpio_num_t(this->out_pin_->get_pin());
  channel.trans_queue_depth = 1;
  channel.intr_priority = 0;
  // The peripheral bypasses the pin's software inversion, so apply it in the GPIO matrix instead.
  // This inverts the whole output, the end-of-transmission idle level included.
  channel.flags.invert_out = this->out_pin_->is_inverted() ? 1 : 0;

  // Keep the configured electrical mode: the driver would otherwise drive an open-drain line actively high.
  const bool open_drain = (this->out_pin_->get_flags() & gpio::FLAG_OPEN_DRAIN) != 0;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
  channel.flags.io_loop_back = open_drain;
  channel.flags.io_od_mode = open_drain;
#endif

  esp_err_t error = rmt_new_tx_channel(&channel, &this->tx_);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "RMT driver failed: %s", esp_err_to_name(error));
    return false;
  }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
  if (open_drain) {
    gpio_od_enable(gpio_num_t(this->out_pin_->get_pin()));
    gpio_input_enable(gpio_num_t(this->out_pin_->get_pin()));
  }
#endif
  if (this->out_pin_->get_flags() & gpio::FLAG_PULLUP) {
    gpio_pullup_en(gpio_num_t(this->out_pin_->get_pin()));
  } else {
    gpio_pullup_dis(gpio_num_t(this->out_pin_->get_pin()));
  }

  rmt_tx_event_callbacks_t callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.on_trans_done = tx_done_isr;
  rmt_copy_encoder_config_t encoder_config;
  memset(&encoder_config, 0, sizeof(encoder_config));

  error = rmt_tx_register_event_callbacks(this->tx_, &callbacks, this);
  if (error == ESP_OK) {
    error = rmt_new_copy_encoder(&encoder_config, &this->encoder_);
  }
  if (error == ESP_OK) {
    error = rmt_enable(this->tx_);
  }
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "RMT driver failed: %s", esp_err_to_name(error));
    return false;
  }

  // The pin is only driven once the channel has transmitted, so put the line at its idle level now.
  this->write_idle_();

  ESP_LOGI(TAG, "RMT on GPIO%u (rx) and GPIO%u (tx), %" PRIu32 " symbols at %" PRIu32 " Hz", this->in_pin_->get_pin(),
           this->out_pin_->get_pin(), this->rmt_symbols_, this->clock_resolution_);
  return true;
}

void OpenThermRMT::write_idle_() {
  rmt_symbol_word_t symbol = {
      .duration0 = 1,
      .level0 = 1,
      .duration1 = 0,
      .level1 = 1,
  };
  rmt_transmit_config_t config;
  memset(&config, 0, sizeof(config));
  config.flags.eot_level = 1;
  const esp_err_t error = rmt_transmit(this->tx_, this->encoder_, &symbol, sizeof(symbol), &config);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "RMT driver failed: %s", esp_err_to_name(error));
  }
}

void OpenThermRMT::listen() {
  if (this->rx_armed_) {
    return;  // Already waiting for the answer.
  }
  this->arm_rx_();
}

void OpenThermRMT::write_frame() {
  this->build_frame_();

  // Nothing is waiting for an answer while we talk, so the receiver is released. It would otherwise record this
  // transmission as if it were the answer.
  this->disarm_rx_();

  rmt_transmit_config_t config;
  memset(&config, 0, sizeof(config));
  // The line rests high between frames.
  config.flags.eot_level = 1;
  const esp_err_t error =
      rmt_transmit(this->tx_, this->encoder_, this->tx_symbols_, FRAME_BITS * sizeof(rmt_symbol_word_t), &config);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "RMT driver failed: %s", esp_err_to_name(error));
    // The closest existing state: the transport, not the protocol, failed.
    this->mode_ = OperationMode::ERROR_TIMER;
  }
}

void OpenThermRMT::abort() {
  this->disarm_rx_();
  // A request still on the line is abandoned, and the line is put back to its idle level.
  if (rmt_tx_wait_all_done(this->tx_, 0) == ESP_ERR_TIMEOUT) {
    rmt_disable(this->tx_);
    rmt_enable(this->tx_);
    this->write_idle_();
  }
}

void OpenThermRMT::loop() {
  if (this->rx_done_) {
    this->rx_done_ = false;
    this->decode_rx_();
  }
}

void OpenThermRMT::build_frame_() {
  // One symbol per bit: the left half then the right half, each half a half bit long.
  for (uint8_t bit = 0; bit < FRAME_BITS; bit++) {
    this->tx_symbols_[bit].duration0 = this->from_microseconds_(HALF_BIT_US);
    this->tx_symbols_[bit].level0 = half_bit_level(this->data_, bit, 0);
    this->tx_symbols_[bit].duration1 = this->from_microseconds_(HALF_BIT_US);
    this->tx_symbols_[bit].level1 = half_bit_level(this->data_, bit, 1);
  }
}

void OpenThermRMT::disarm_rx_() {
  if (!this->rx_armed_) {
    return;
  }
  // The driver has no way to cancel a pending capture, so the channel is disabled and enabled again to abandon it.
  rmt_disable(this->rx_);
  rmt_enable(this->rx_);
  this->rx_armed_ = false;
  this->rx_done_ = false;
  this->symbol_count_ = 0;
}

void OpenThermRMT::arm_rx_() {
  this->rx_done_ = false;
  this->symbol_count_ = 0;
  this->rx_armed_ =
      rmt_receive(this->rx_, this->buffer_.data(), this->buffer_.size_bytes(), &this->rx_config_) == ESP_OK;
  if (!this->rx_armed_) {
    ESP_LOGW(TAG, "Failed to arm RMT capture, no response will be decoded this round");
  }
}

void OpenThermRMT::decode_rx_() {
  // A capture that completes after the conversation ended must not revive it.
  if (this->is_idle()) {
    return;
  }
  // Rebuild edge timestamps from the recorded durations. Each symbol holds two (duration, level) halves.
  const auto *symbols = reinterpret_cast<const rmt_symbol_word_t *>(this->buffer_.data());
  const uint8_t invert = this->in_pin_->is_inverted() ? 1 : 0;
  size_t count = 0;
  uint32_t ticks = 0;
  int previous = -1;
  bool ended = false;
  for (size_t i = 0; i < this->symbol_count_ && i < this->buffer_.size() && !ended; i++) {
    const rmt_symbol_word_t &symbol = symbols[i];
    for (uint8_t half = 0; half < 2; half++) {
      const uint32_t duration = half == 0 ? symbol.duration0 : symbol.duration1;
      const uint8_t level = half == 0 ? symbol.level0 : symbol.level1;
      if (previous < 0) {
        previous = level;  // The level the capture opened on is not a transition.
      } else if (level != previous) {
        const uint32_t edge_us = this->to_microseconds_(ticks);
        // A pulse that opens the capture has no stored first edge, so its closing edge would be read as the start.
        const bool opening_noise = count == 0 && edge_us < GLITCH_MAX_US;
        if (!opening_noise && count < RMT_MAX_EDGES) {
          this->edge_us_[count] = edge_us;
          this->edge_level_[count] = level ^ invert;
          count++;
        }
        previous = level;
      }
      if (duration == 0) {
        // End of capture. The transition above still counts: the driver ends the capture on it.
        ended = true;
        break;
      }
      ticks += duration;
    }
  }

  if (count < 2) {
    // Nothing usable was captured, so the answer may still be on its way.
    this->arm_rx_();
    return;
  }

  uint32_t data = 0;
  uint8_t position = 0;
  const ProtocolErrorType error = decode_edges(this->edge_us_, this->edge_level_, &count, &data, &position);

  this->bit_pos_ = position;
  if (error == ProtocolErrorType::NO_ERROR) {
    this->data_ = data;
    this->mode_ = OperationMode::RECEIVED;
  } else {
    this->error_type_ = error;
    this->capture_ = 0;
    this->mode_ = OperationMode::ERROR_PROTOCOL;
  }
}

}  // namespace esphome::opentherm

#endif  // USE_ESP32 && SOC_RMT_SUPPORTED
