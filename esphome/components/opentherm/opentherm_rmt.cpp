#ifdef USE_ESP32

#include "opentherm_rmt.h"
#include "esphome/core/helpers.h"
#include <driver/rmt_rx.h>
#include <driver/rmt_tx.h>
#include <driver/gpio.h>
#include <esp_err.h>
#include <string>

namespace esphome {
namespace opentherm {

using std::string;

static const char *const TAG = "opentherm";

#ifdef USE_ESP32_VARIANT_ESP32H2
static const uint32_t RMT_CLK_FREQ = 32000000;
#else
static const uint32_t RMT_CLK_FREQ = 80000000;
#endif

OpenTherm::OpenTherm(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin) : OpenThermBase(in_pin, out_pin) {}

bool OpenTherm::initialize() { return OpenThermBase::initialize() && this->rmt_init_(); }

void OpenTherm::listen() {
  OpenThermBase::listen();
  this->rmt_read_();
}

void OpenTherm::send(OpenthermData &data) {
  OpenThermBase::send(data);
  this->rmt_write_();
}

void OpenTherm::stop() { OpenThermBase::stop(); }

bool OpenTherm::rmt_init_() {
  static_assert(SOC_RMT_MEM_WORDS_PER_CHANNEL >= RMT_SYMBOL_CAPACITY, "RMT buffer is too small on this ESP32 variant");

  // Configure RX channel
  rmt_rx_channel_config_t rx_chan_cfg = {};
  rx_chan_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
  rx_chan_cfg.resolution_hz = RMT_RESOLUTION_HZ;  // 1 tick = 1 us
  rx_chan_cfg.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL;
  rx_chan_cfg.gpio_num = static_cast<gpio_num_t>(this->in_pin_->get_pin());
  rx_chan_cfg.intr_priority = 0;
  rx_chan_cfg.flags.invert_in = 0;
  rx_chan_cfg.flags.with_dma = 0;
  rx_chan_cfg.flags.io_loop_back = 0;
  if (rmt_new_rx_channel(&rx_chan_cfg, &this->rx_channel_) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create RMT RX channel");
    this->mode_ = OperationMode::ERROR_RMT;
    return false;
  }

  // Configure TX channel
  rmt_tx_channel_config_t tx_chan_cfg = {};
  tx_chan_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
  tx_chan_cfg.resolution_hz = RMT_RESOLUTION_HZ;
  tx_chan_cfg.gpio_num = static_cast<gpio_num_t>(this->out_pin_->get_pin());
  tx_chan_cfg.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL;
  tx_chan_cfg.trans_queue_depth = 1;
  tx_chan_cfg.flags.io_loop_back = 0;
  tx_chan_cfg.flags.io_od_mode = 0;
  tx_chan_cfg.flags.invert_out = 0;
  tx_chan_cfg.flags.with_dma = 0;
  tx_chan_cfg.intr_priority = 0;
  if (rmt_new_tx_channel(&tx_chan_cfg, &this->tx_channel_) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create RMT TX channel");
    this->mode_ = OperationMode::ERROR_RMT;
    return false;
  }

  // Simple copy encoder for raw symbols
  constexpr rmt_copy_encoder_config_t enc_cfg = {};
  if (rmt_new_copy_encoder(&enc_cfg, &this->tx_encoder_) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create RMT TX encoder");
    this->mode_ = OperationMode::ERROR_RMT;
    return false;
  }

  if (rmt_enable(this->rx_channel_) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable RMT RX channel");
    this->mode_ = OperationMode::ERROR_RMT;
    return false;
  }
  if (rmt_enable(this->tx_channel_) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable RMT TX channel");
    this->mode_ = OperationMode::ERROR_RMT;
    return false;
  }

  // RX done callback
  rmt_rx_event_callbacks_t cbs = {};
  cbs.on_recv_done = &OpenTherm::rmt_read_callback;
  if (rmt_rx_register_event_callbacks(this->rx_channel_, &cbs, this) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register RMT RX callback");
    this->mode_ = OperationMode::ERROR_RMT;
    return false;
  }

  // Configure receive timing window
  // Filter out short glitches; consider signal ended after >2000us without edge
  // Note: signal_range_min_ns must be < 255 cycles of the RMT source clock.
  // Clamp to the hardware-supported maximum to avoid runtime errors.
  constexpr uint32_t max_filter_ns = 255 * 1000 / (RMT_CLK_FREQ / 1000000);
  this->rx_config_.signal_range_min_ns = std::min(static_cast<uint32_t>(100 * 1000), max_filter_ns);
  this->rx_config_.signal_range_max_ns = 2000 * 1000;

  // Transmit one RMT frame so that output pin becomes high
  rmt_symbol_word_t syms[1];
  syms[0].level0 = 0;
  syms[0].duration0 = 10;
  syms[0].level1 = 1;
  syms[0].duration1 = 10;

  rmt_transmit_config_t cfg = {};
  cfg.loop_count = 0;
  cfg.flags.eot_level = 1;

  esp_err_t err = rmt_transmit(this->tx_channel_, this->tx_encoder_, syms, sizeof(syms), &cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to transmit RMT init sequence: %s", esp_err_to_name(err));
    this->mode_ = OperationMode::ERROR_RMT;
    return false;
  }

  // Wait until transmission completes to move to SENT state (simple and robust)
  err = rmt_tx_wait_all_done(this->tx_channel_, -1);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed waiting for RMT init sequence completion: %s", esp_err_to_name(err));
    this->mode_ = OperationMode::ERROR_RMT;
    return false;
  }

  // TX done callback
  rmt_tx_event_callbacks_t tx_cbs = {};
  tx_cbs.on_trans_done = &OpenTherm::rmt_write_callback;
  if (rmt_tx_register_event_callbacks(this->tx_channel_, &tx_cbs, this) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register RMT TX callback");
    this->mode_ = OperationMode::ERROR_RMT;
    return false;
  }

  return true;
}

void OpenTherm::rmt_read_() {
  // Start single receive into internal buffer
  memset(this->rmt_buffer_, 0, sizeof(this->rmt_buffer_));

  esp_err_t err = rmt_receive(this->rx_channel_, this->rmt_buffer_, sizeof(this->rmt_buffer_), &this->rx_config_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to start RMT receive: %s", esp_err_to_name(err));
    this->mode_ = OperationMode::ERROR_RMT;
  }
}

void OpenTherm::rmt_write_() {
  // Build Manchester stream as raw RMT symbols, 1 bit = two halves of 500us
  rmt_symbol_word_t syms[34];

  // Helper to encode a bit: 1 => low,high; 0 => high,low
  auto encode_bit = [&](bool bit) -> rmt_symbol_word_t {
    rmt_symbol_word_t s{};
    s.level0 = bit ? 0 : 1;
    s.duration0 = 500;
    s.level1 = bit ? 1 : 0;
    s.duration1 = 500;
    return s;
  };

  // Start bit '1'
  syms[0] = encode_bit(true);
  // Data bits MSB..LSB (bits 31..0)
  for (int i = 31; i >= 0; i--) {
    uint8_t b = (this->data_ >> i) & 0x1;
    syms[32 - i] = encode_bit(b);
  }
  // Stop bit '1'
  syms[33] = encode_bit(true);

  rmt_transmit_config_t cfg = {};
  cfg.loop_count = 0;
  cfg.flags.eot_level = 1;  // idle high after transmit

  esp_err_t err = rmt_transmit(this->tx_channel_, this->tx_encoder_, syms, sizeof(syms), &cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to transmit RMT symbols: %s", esp_err_to_name(err));
    this->mode_ = OperationMode::ERROR_RMT;
    return;
  }

  this->mode_ = OperationMode::WRITE;
}

bool IRAM_ATTR OpenTherm::rmt_read_callback(rmt_channel_handle_t, const rmt_rx_done_event_data_t *evt, void *arg) {
  auto *self = static_cast<OpenTherm *>(arg);
  if (evt == nullptr || evt->received_symbols == nullptr) {
    ESP_LOGW(TAG, "RMT pointer is null");
    self->mode_ = OperationMode::ERROR_RMT;
    return false;
  }
  if (evt->num_symbols == 0) {
    ESP_LOGW(TAG, "RMT reported 0 symbols");
    self->mode_ = OperationMode::ERROR_RMT;
    return false;
  }
  if (evt->num_symbols > RMT_SYMBOL_CAPACITY) {
    ESP_LOGE(TAG, "Received %u symbols from RMT, but capacity is limited at %u. This is a bug, please report it.",
             evt->num_symbols, RMT_SYMBOL_CAPACITY);
    self->mode_ = OperationMode::ERROR_RMT;
    return false;
  }

  self->rmt_buffer_symbol_count_ = evt->num_symbols;

  if (!self->decode_rmt_symbols_(evt->num_symbols)) {
    return false;
  }

  self->mode_ = OperationMode::RECEIVED;

  return false;
}

bool IRAM_ATTR OpenTherm::rmt_write_callback(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *evt,
                                             void *arg) {
  auto *self = static_cast<OpenTherm *>(arg);

  if (evt == nullptr) {
    ESP_LOGW(TAG, "NULL RMT tx event");
    self->mode_ = OperationMode::ERROR_RMT;
    return false;
  }
  if (evt->num_symbols == 0) {
    ESP_LOGW(TAG, "RMT tx reported 0 symbols");
    self->mode_ = OperationMode::ERROR_RMT;
    return false;
  }

  self->mode_ = OperationMode::SENT;

  return false;
}

bool IRAM_ATTR OpenTherm::decode_rmt_symbols_(size_t num_symbols) {
  static constexpr uint16_t SHORT_DURATION = 500;
  static constexpr uint16_t LONG_DURATION = 1000;
  static constexpr uint16_t LEFT_TOLERANCE = 100;
  static constexpr uint16_t RIGHT_TOLERANCE = 150;
  static constexpr uint8_t ERROR_BIT_VALUE = 254;
  static constexpr uint8_t NO_BIT_VALUE = 253;

  this->bit_index_ = 0;
  this->error_type_ = ProtocolErrorType::NO_ERROR;

  uint8_t prev_level = 0;
  uint8_t tick = 0;

  auto is_short = [](uint16_t duration) -> bool {
    return duration >= SHORT_DURATION - LEFT_TOLERANCE && duration <= SHORT_DURATION + RIGHT_TOLERANCE;
  };

  auto is_long = [](uint16_t duration) -> bool {
    return duration >= LONG_DURATION - LEFT_TOLERANCE * 2 && duration <= LONG_DURATION + RIGHT_TOLERANCE * 2;
  };

  auto produce_bit = [&](uint16_t level) -> uint8_t {
    if (prev_level == level) {
      this->set_protocol_error_(ProtocolErrorType::NO_TRANSITION);
      return ERROR_BIT_VALUE;
    }

    return level == 1 ? 0 : 1;  // Rising edge → logical 0, falling edge → logical 1.
  };

  auto process_level = [&](uint16_t level, uint16_t duration) -> uint8_t {
    uint8_t result = ERROR_BIT_VALUE;
    if (is_short(duration)) {
      result = tick == 1 ? produce_bit(level) : NO_BIT_VALUE;
      tick = tick == 0 ? 1 : 0;
      prev_level = level;
    } else if (is_long(duration)) {
      if (tick == 1) {
        result = produce_bit(level);
        // Since we have a long interval, it contains both the second half for the current bit and the first half for
        // the next bit.
        tick = 1;
      } else {
        // Long intervals should happen only when we already have first half of a bit.
        this->set_protocol_error_(ProtocolErrorType::NO_CHANGE_TOO_LONG);
      }
    } else {
      this->set_protocol_error_(ProtocolErrorType::INVALID_DURATION);
    }

    prev_level = level;
    return result;
  };

  for (size_t rmt_idx = 0; rmt_idx < num_symbols; rmt_idx++) {
    auto symbol = this->rmt_buffer_[rmt_idx];
    for (uint8_t half = 0; half < 2; half++) {
      uint16_t level = half == 0 ? symbol.level0 : symbol.level1;
      uint16_t duration = half == 0 ? symbol.duration0 : symbol.duration1;
      uint8_t bit;

      if (duration == 0 && this->bit_index_ == 33) {
        // Edge case for the stop bit. RMT reports last low level with 0 duration.
        if (level == 0 && prev_level == 1) {
          bit = 1;
        } else {
          this->set_protocol_error_(ProtocolErrorType::INVALID_START_STOP_BIT);
          return false;
        }
      } else {
        bit = process_level(level, duration);
      }

      if (bit == ERROR_BIT_VALUE) {
        return false;
      }

      if (bit == NO_BIT_VALUE)
        continue;

      if (this->bit_index_ == 0 || this->bit_index_ == 33) {  // Check start and stop bit
        if (bit != 1) {
          this->set_protocol_error_(ProtocolErrorType::INVALID_START_STOP_BIT);
          return false;
        }
      } else {
        this->data_ = (this->data_ << 1) | bit;
      }

      if (this->bit_index_ == 33)  // And we are done!
        return true;

      this->bit_index_++;
    }
  }

  // If we reached here, something went wrong and our data is incomplete.
  this->set_protocol_error_(ProtocolErrorType::INSUFFICIENT_DATA);
  return false;
}

void IRAM_ATTR OpenTherm::set_protocol_error_(ProtocolErrorType error_type) {
  this->mode_ = OperationMode::ERROR_PROTOCOL;
  this->error_type_ = error_type;
}

void OpenTherm::log_protocol_state() const {
  ESP_LOGD(TAG,
           "OpenTherm protocol error: %s\n"
           "Bit index: %u\n"
           "Data: %s",
           protocol_error_to_str(this->error_type_), this->bit_index_, format_hex(this->data_).c_str());

  if (this->rmt_buffer_symbol_count_ == 0) {
    ESP_LOGD(TAG, "RMT debug: no data available");
    return;
  }
  ESP_LOGD(TAG, "RX raw begin =====================================");
  ESP_LOGD(TAG, "symbols=%u", this->rmt_buffer_symbol_count_);
  for (size_t i = 0; i < this->rmt_buffer_symbol_count_; i++) {
    const auto &s = this->rmt_buffer_[i];
    ESP_LOGD(TAG, "SYM[%03u]: L0=%u D0=%u us | L1=%u D1=%u us", i, s.level0, s.duration0, s.level1, s.duration1);
  }
  ESP_LOGD(TAG, "RX raw end =======================================");
}

}  // namespace opentherm
}  // namespace esphome

#endif
