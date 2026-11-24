#include "opentherm_rmt.h"

#if defined(USE_ESP32) && SOC_RMT_SUPPORTED

#include <cstring>
#include <esp_clk_tree.h>
#include "esphome/core/wake.h"

namespace esphome::opentherm {

ESPHOME_LOG_TAG(TAG, "opentherm");

bool IRAM_ATTR HOT OpenThermRMT::rx_done_isr(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata,
                                             void *user_ctx) {
  auto *arg = static_cast<OpenThermRMT *>(user_ctx);
  arg->symbol_count_ = edata->num_symbols;
  arg->rx_done_ = true;
  // This is the only moment the channel is free, so the next capture starts here and the receiver never stops.
  arg->rx_armed_ = rmt_receive(channel, arg->buffer_.data(), arg->buffer_.size_bytes(), &arg->rx_config_) == ESP_OK;
  BaseType_t task_woken = pdFALSE;
  wake_loop_isrsafe(&task_woken);
  return task_woken != pdFALSE;
}

bool IRAM_ATTR HOT OpenThermRMT::tx_done_isr(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata,
                                             void *user_ctx) {
  auto *arg = static_cast<OpenThermRMT *>(user_ctx);
  // All data written to the line; the hub moves on to listening when it sees this.
  arg->mode_ = OperationMode::SENT;
  BaseType_t task_woken = pdFALSE;
  wake_loop_isrsafe(&task_woken);
  return task_woken != pdFALSE;
}

bool OpenThermRMT::initialize_impl_() { return this->init_rx_() && this->init_tx_(); }

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

  esp_err_t error = rmt_new_tx_channel(&channel, &this->tx_);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "RMT driver failed: %s", esp_err_to_name(error));
    return false;
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

  ESP_LOGI(TAG, "RMT on GPIO%u (rx) and GPIO%u (tx), %" PRIu32 " symbols at %" PRIu32 " Hz",
           this->in_pin_->get_pin(), this->out_pin_->get_pin(), this->rmt_symbols_, this->clock_resolution_);
  return true;
}

void OpenThermRMT::listen_impl_() {
  this->listen_start_ = millis();
  // The channel arms itself again after every capture, so it is normally already listening by the time we get here.
  if (!this->rx_armed_) {
    this->arm_rx_();
  }
}

void OpenThermRMT::send_impl_() {
  this->build_frame_();

  // The receiver is armed permanently, so it would record this transmission as if it were the answer. Drop the
  // capture and leave the channel idle until listen() arms it again.
  if (this->rx_armed_) {
    rmt_disable(this->rx_);
    rmt_enable(this->rx_);
    this->rx_armed_ = false;
  }
  this->rx_done_ = false;
  this->symbol_count_ = 0;

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

void OpenThermRMT::stop_impl_() {
  // The capture is left alone: it either closed itself after the stop bit or is still waiting for an answer, and
  // either way the next listen() replaces it.
}

void OpenThermRMT::loop_impl_() {
  if (this->rx_done_) {
    this->rx_done_ = false;
    this->decode_rx_();
  }
  // A capture that found nothing is armed again, so the timeout is what ends an unanswered request.
  if (this->mode_ == OperationMode::LISTEN &&
      millis() - this->listen_start_ >= static_cast<uint32_t>(this->device_timeout_)) {
    this->mode_ = OperationMode::ERROR_TIMEOUT;
  }
}

void OpenThermRMT::build_frame_() {
  size_t index = 0;
  // Manchester: the left half carries the inverse of the bit, the right half the bit itself.
  auto add_bit = [&](uint8_t value) {
    this->tx_symbols_[index].duration0 = this->from_microseconds_(HALF_BIT_US);
    this->tx_symbols_[index].level0 = !value;
    this->tx_symbols_[index].duration1 = this->from_microseconds_(HALF_BIT_US);
    this->tx_symbols_[index].level1 = value;
    index++;
  };

  add_bit(1);  // start bit
  for (int8_t bit = 31; bit >= 0; bit--) {
    add_bit(read_bit(this->data_, bit));
  }
  add_bit(1);  // stop bit
}

void OpenThermRMT::arm_rx_() {
  this->rx_done_ = false;
  this->symbol_count_ = 0;

  // The glitch filter counts ticks of the RMT clock source in an 8 bit register, so 255 ticks is the ceiling -
  // about 3.2 us at 80 MHz. Ask for the maximum, which is as much immunity as the peripheral offers.
  uint32_t clock_hz = 80000000;
  esp_clk_tree_src_get_freq_hz((soc_module_clk_t) RMT_CLK_SRC_DEFAULT, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED,
                               &clock_hz);
  memset(&this->rx_config_, 0, sizeof(this->rx_config_));
  this->rx_config_.signal_range_min_ns = UINT8_MAX * 1000u / (clock_hz / 1000000u);
  this->rx_config_.signal_range_max_ns = RMT_IDLE_US * 1000;

  this->rx_armed_ =
      rmt_receive(this->rx_, this->buffer_.data(), this->buffer_.size_bytes(), &this->rx_config_) == ESP_OK;
  if (!this->rx_armed_) {
    ESP_LOGW(TAG, "Failed to arm RMT capture, no response will be decoded this round");
  }
}

void OpenThermRMT::decode_rx_() {
  // Rebuild edge timestamps from the recorded durations. Each symbol holds two (duration, level) halves.
  const auto *symbols = reinterpret_cast<const rmt_symbol_word_t *>(this->buffer_.data());
  const uint8_t invert = this->in_pin_->is_inverted() ? 1 : 0;
  size_t count = 0;
  uint32_t ticks = 0;
  int previous = -1;
  for (size_t i = 0; i < this->symbol_count_ && i < this->buffer_.size(); i++) {
    const rmt_symbol_word_t &symbol = symbols[i];
    for (uint8_t half = 0; half < 2; half++) {
      const uint32_t duration = half == 0 ? symbol.duration0 : symbol.duration1;
      const uint8_t level = half == 0 ? symbol.level0 : symbol.level1;
      if (previous < 0) {
        previous = level;  // The level the capture opened on is not a transition.
      } else if (level != previous && count < RMT_MAX_EDGES) {
        this->edge_us_[count] = this->to_microseconds_(ticks);
        this->edge_level_[count] = level ^ invert;
        count++;
        previous = level;
      }
      if (duration == 0) {
        break;  // End of capture. The transition above still counts: the driver ends the capture on it.
      }
      ticks += duration;
    }
  }

  if (count < 2) {
    return;  // Nothing usable, and the channel has already armed itself again.
  }

  uint32_t data = 0;
  uint8_t position = 0;
  const ProtocolErrorType error = decode_edges_(this->edge_us_, this->edge_level_, count, &data, &position);

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

ProtocolErrorType OpenThermRMT::decode_edges_(const uint32_t *edges_us, const uint8_t *levels, uint8_t count,
                                              uint32_t *data, uint8_t *bit_pos) {
  if (count < 2) {
    *bit_pos = 0;
    return ProtocolErrorType::NO_TRANSITION;
  }

  // Every bit, the start bit included, is one mid-bit transition, and consecutive ones are one bit apart. An edge
  // closer than 750 µs to the previous bit is the optional boundary transition and is skipped, so the frame is simply
  // 34 bits long and the start and stop bits are the first and last of them.
  constexpr uint32_t BIT_GAP_MIN_US = 750;

  uint32_t reference = edges_us[0];
  uint8_t scan = 0;
  uint32_t value = 0;

  for (uint8_t bit = 0; bit < 34; bit++) {
    *bit_pos = bit;
    if (bit > 0) {
      const uint32_t low = reference + BIT_GAP_MIN_US;
      while (scan < count && edges_us[scan] <= low) {
        scan++;
      }
      if (scan >= count) {
        return ProtocolErrorType::NO_TRANSITION;
      }
      reference = edges_us[scan];
    }

    // The value is the level held before the transition, which is the right half of the bit.
    const uint8_t level = levels[scan] ^ 1;
    if (bit == 0 && level != 1) {
      return ProtocolErrorType::NO_TRANSITION;  // A response opens with the start bit.
    }
    if (bit >= 1 && bit <= 32) {
      value = (value << 1) | level;
    }
    if (bit == BitPositions::STOP_BIT && level != 1) {
      return ProtocolErrorType::INVALID_STOP_BIT;
    }
    scan++;
  }

  // The frame carries one parity bit over the whole 32-bit word.
  if (!OpenThermBase::check_parity_(value)) {
    return ProtocolErrorType::PARITY_ERROR;
  }

  *data = value;
  return ProtocolErrorType::NO_ERROR;
}

}  // namespace esphome::opentherm

#endif  // USE_ESP32 && SOC_RMT_SUPPORTED
