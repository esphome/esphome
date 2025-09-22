/*
 * OpenTherm protocol implementation. Originally taken from https://github.com/jpraus/arduino-opentherm, but
 * heavily modified to comply with ESPHome coding standards and provide better logging.
 * Original code is licensed under Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International
 * Public License, which is compatible with GPLv3 license, which covers C++ part of ESPHome project.
 */

#include "opentherm.h"
#include "esphome/core/helpers.h"
#include <driver/rmt_rx.h>
#include <driver/rmt_tx.h>
#include <driver/gpio.h>
#include <esp_err.h>
#include <string>

namespace esphome {
namespace opentherm {

using std::string;
using std::to_string;

static const char *const TAG = "opentherm";

#ifdef USE_ESP32_VARIANT_ESP32H2
static const uint32_t RMT_CLK_FREQ = 32000000;
#else
static const uint32_t RMT_CLK_FREQ = 80000000;
#endif

OpenTherm::OpenTherm(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin, int32_t) : in_pin_(in_pin), out_pin_(out_pin) {
  this->isr_in_pin_ = in_pin->to_isr();
  this->isr_out_pin_ = out_pin->to_isr();
}

bool OpenTherm::initialize() {
  this->in_pin_->pin_mode(gpio::FLAG_INPUT);
  this->in_pin_->setup();
  this->out_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->out_pin_->setup();

  return this->rmt_init_();
}

void OpenTherm::listen() {
  this->mode_ = OperationMode::LISTEN;
  this->data_ = 0;
  this->rmt_read_();
}

void OpenTherm::send(OpenthermData &data) {
  this->data_ = data.type;
  this->data_ = (this->data_ << 12) | data.id;
  this->data_ = (this->data_ << 8) | data.valueHB;
  this->data_ = (this->data_ << 8) | data.valueLB;
  if (!check_parity_(this->data_)) {
    this->data_ = this->data_ | 0x80000000;
  }

  this->mode_ = OperationMode::WRITE;
  this->rmt_write_();
}

bool OpenTherm::get_message(OpenthermData &data) {
  if (this->mode_ == OperationMode::RECEIVED) {
    data.type = (this->data_ >> 28) & 0x7;
    data.id = (this->data_ >> 16) & 0xFF;
    data.valueHB = (this->data_ >> 8) & 0xFF;
    data.valueLB = this->data_ & 0xFF;
    return true;
  }
  return false;
}

const OpenThermProtocolError &OpenTherm::get_protocol_error() const { return this->error_; }

void OpenTherm::stop() { this->mode_ = OperationMode::IDLE; }

bool OpenTherm::rmt_init_() {
  // Configure RX channel
  rmt_rx_channel_config_t rx_chan_cfg = {};
  rx_chan_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
  rx_chan_cfg.resolution_hz = RMT_RESOLUTION_HZ;  // 1 tick = 1 us
  rx_chan_cfg.mem_block_symbols = 64;             // enough symbols
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
  tx_chan_cfg.mem_block_symbols = 64;
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
  auto set_sym = [](rmt_symbol_word_t &s, bool level0, uint16_t dur0, bool level1, uint16_t dur1) {
    s.level0 = level0;
    s.duration0 = dur0;
    s.level1 = level1;
    s.duration1 = dur1;
  };

  // Helper to encode a bit: 1 => low,high; 0 => high,low
  auto encode_bit = [&](uint8_t bit) -> rmt_symbol_word_t {
    rmt_symbol_word_t s{};
    if (bit) {
      set_sym(s, 0, 500, 1, 500);
    } else {
      set_sym(s, 1, 500, 0, 500);
    }
    return s;
  };

  // Start bit '1'
  syms[0] = encode_bit(1);
  // Data bits MSB..LSB (bits 31..0)
  for (int i = 31; i >= 0; i--) {
    uint8_t b = (this->data_ >> i) & 0x1;
    syms[32 - i] = encode_bit(b);
  }
  // Stop bit '1'
  syms[33] = encode_bit(1);

  rmt_transmit_config_t cfg = {};
  cfg.loop_count = 0;
  cfg.flags.eot_level = 1;  // idle high after transmit

  esp_err_t err = rmt_transmit(this->tx_channel_, this->tx_encoder_, syms, sizeof(syms), &cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to transmit RMT symbols: %s", esp_err_to_name(err));
    this->mode_ = OperationMode::ERROR_RMT;
    return;
  }

  // Wait until transmission completes to move to SENT state (simple and robust)
  err = rmt_tx_wait_all_done(this->tx_channel_, -1);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed waiting for TX done: %s", esp_err_to_name(err));
    this->mode_ = OperationMode::ERROR_RMT;
    return;
  }

  this->mode_ = OperationMode::SENT;
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

bool IRAM_ATTR OpenTherm::decode_rmt_symbols_(size_t num_symbols) {
  static constexpr uint16_t SHORT_DURATION = 500;
  static constexpr uint16_t LONG_DURATION = 1000;
  static constexpr uint16_t LEFT_TOLERANCE = 100;
  static constexpr uint16_t RIGHT_TOLERANCE = 150;
  static constexpr uint8_t ERROR_BIT_VALUE = 254;
  static constexpr uint8_t NO_BIT_VALUE = 253;

  uint8_t bit_idx = 0;
  uint8_t prev_level = 0;
  uint8_t tick = 0;

  this->error_ = {};

  auto is_short = [](uint16_t duration) -> bool {
    return duration >= SHORT_DURATION - LEFT_TOLERANCE && duration <= SHORT_DURATION + RIGHT_TOLERANCE;
  };

  auto is_long = [](uint16_t duration) -> bool {
    return duration >= LONG_DURATION - LEFT_TOLERANCE * 2 && duration <= LONG_DURATION + RIGHT_TOLERANCE * 2;
  };

  auto produce_bit = [&](uint16_t level) -> uint8_t {
    if (prev_level == level) {
      this->set_protocol_error(ProtocolErrorType::NO_TRANSITION, bit_idx);
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
        this->set_protocol_error(ProtocolErrorType::NO_CHANGE_TOO_LONG, bit_idx);
      }
    } else {
      this->set_protocol_error(ProtocolErrorType::INVALID_DURATION, bit_idx);
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

      if (duration == 0 && bit_idx == 33) {
        // Edge case for the stop bit. RMT reports last low level with 0 duration.
        if (level == 0 && prev_level == 1) {
          bit = 1;
        } else {
          this->set_protocol_error(ProtocolErrorType::INVALID_START_STOP_BIT, bit_idx);
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

      if (bit_idx == 0 || bit_idx == 33) {  // Check start and stop bit
        if (bit != 1) {
          this->set_protocol_error(ProtocolErrorType::INVALID_START_STOP_BIT, bit_idx);
          return false;
        }
      } else {
        this->data_ = (this->data_ << 1) | bit;
      }

      if (bit_idx == 33)  // And we are done!
        return true;

      bit_idx++;
    }
  }

  // If we reached here, something went wrong and our data is incomplete.
  this->set_protocol_error(ProtocolErrorType::INSUFFICIENT_DATA, bit_idx);
  return false;
}

void OpenTherm::set_protocol_error(ProtocolErrorType error_type, size_t bit_index) {
  this->mode_ = OperationMode::ERROR_PROTOCOL;
  this->error_.error_type = error_type;
  this->error_.bit_index = bit_index;
  this->error_.data = data_;
}

// https://stackoverflow.com/questions/21617970/how-to-check-if-value-has-even-parity-of-bits-or-odd
bool IRAM_ATTR OpenTherm::check_parity_(uint32_t val) {
  val ^= val >> 16;
  val ^= val >> 8;
  val ^= val >> 4;
  val ^= val >> 2;
  val ^= val >> 1;
  return (~val) & 1;
}

#define TO_STRING_MEMBER(name) \
  case name: \
    return #name;

const char *OpenTherm::operation_mode_to_str(OperationMode mode) {
  switch (mode) {
    TO_STRING_MEMBER(IDLE)
    TO_STRING_MEMBER(LISTEN)
    TO_STRING_MEMBER(READ)
    TO_STRING_MEMBER(RECEIVED)
    TO_STRING_MEMBER(WRITE)
    TO_STRING_MEMBER(SENT)
    TO_STRING_MEMBER(ERROR_PROTOCOL)
    TO_STRING_MEMBER(ERROR_TIMEOUT)
    TO_STRING_MEMBER(ERROR_RMT)
    default:
      return "<INVALID>";
  }
}
const char *OpenTherm::protocol_error_to_str(ProtocolErrorType error_type) {
  switch (error_type) {
    TO_STRING_MEMBER(NO_ERROR)
    TO_STRING_MEMBER(NO_TRANSITION)
    TO_STRING_MEMBER(INVALID_START_STOP_BIT)
    TO_STRING_MEMBER(PARITY_ERROR)
    TO_STRING_MEMBER(NO_CHANGE_TOO_LONG)
    TO_STRING_MEMBER(INVALID_DURATION)
    TO_STRING_MEMBER(INSUFFICIENT_DATA)
    default:
      return "<INVALID>";
  }
}
const char *OpenTherm::timer_error_to_str(TimerErrorType error_type) {
  switch (error_type) {
    TO_STRING_MEMBER(NO_TIMER_ERROR)
    TO_STRING_MEMBER(SET_ALARM_VALUE_ERROR)
    TO_STRING_MEMBER(TIMER_START_ERROR)
    TO_STRING_MEMBER(TIMER_PAUSE_ERROR)
    TO_STRING_MEMBER(SET_COUNTER_VALUE_ERROR)
    default:
      return "<INVALID>";
  }
}
const char *OpenTherm::message_type_to_str(MessageType message_type) {
  switch (message_type) {
    TO_STRING_MEMBER(READ_DATA)
    TO_STRING_MEMBER(READ_ACK)
    TO_STRING_MEMBER(WRITE_DATA)
    TO_STRING_MEMBER(WRITE_ACK)
    TO_STRING_MEMBER(INVALID_DATA)
    TO_STRING_MEMBER(DATA_INVALID)
    TO_STRING_MEMBER(UNKNOWN_DATAID)
    default:
      return "<INVALID>";
  }
}

const char *OpenTherm::message_id_to_str(MessageId id) {
  switch (id) {
    TO_STRING_MEMBER(STATUS)
    TO_STRING_MEMBER(CH_SETPOINT)
    TO_STRING_MEMBER(CONTROLLER_CONFIG)
    TO_STRING_MEMBER(DEVICE_CONFIG)
    TO_STRING_MEMBER(COMMAND_CODE)
    TO_STRING_MEMBER(FAULT_FLAGS)
    TO_STRING_MEMBER(REMOTE)
    TO_STRING_MEMBER(COOLING_CONTROL)
    TO_STRING_MEMBER(CH2_SETPOINT)
    TO_STRING_MEMBER(CH_SETPOINT_OVERRIDE)
    TO_STRING_MEMBER(TSP_COUNT)
    TO_STRING_MEMBER(TSP_COMMAND)
    TO_STRING_MEMBER(FHB_SIZE)
    TO_STRING_MEMBER(FHB_COMMAND)
    TO_STRING_MEMBER(MAX_MODULATION_LEVEL)
    TO_STRING_MEMBER(MAX_BOILER_CAPACITY)
    TO_STRING_MEMBER(ROOM_SETPOINT)
    TO_STRING_MEMBER(MODULATION_LEVEL)
    TO_STRING_MEMBER(CH_WATER_PRESSURE)
    TO_STRING_MEMBER(DHW_FLOW_RATE)
    TO_STRING_MEMBER(DAY_TIME)
    TO_STRING_MEMBER(DATE)
    TO_STRING_MEMBER(YEAR)
    TO_STRING_MEMBER(ROOM_SETPOINT_CH2)
    TO_STRING_MEMBER(ROOM_TEMP)
    TO_STRING_MEMBER(FEED_TEMP)
    TO_STRING_MEMBER(DHW_TEMP)
    TO_STRING_MEMBER(OUTSIDE_TEMP)
    TO_STRING_MEMBER(RETURN_WATER_TEMP)
    TO_STRING_MEMBER(SOLAR_STORE_TEMP)
    TO_STRING_MEMBER(SOLAR_COLLECT_TEMP)
    TO_STRING_MEMBER(FEED_TEMP_CH2)
    TO_STRING_MEMBER(DHW2_TEMP)
    TO_STRING_MEMBER(EXHAUST_TEMP)
    TO_STRING_MEMBER(FAN_SPEED)
    TO_STRING_MEMBER(FLAME_CURRENT)
    TO_STRING_MEMBER(ROOM_TEMP_CH2)
    TO_STRING_MEMBER(REL_HUMIDITY)
    TO_STRING_MEMBER(DHW_BOUNDS)
    TO_STRING_MEMBER(CH_BOUNDS)
    TO_STRING_MEMBER(OTC_CURVE_BOUNDS)
    TO_STRING_MEMBER(DHW_SETPOINT)
    TO_STRING_MEMBER(MAX_CH_SETPOINT)
    TO_STRING_MEMBER(OTC_CURVE_RATIO)
    TO_STRING_MEMBER(HVAC_STATUS)
    TO_STRING_MEMBER(REL_VENT_SETPOINT)
    TO_STRING_MEMBER(DEVICE_VENT)
    TO_STRING_MEMBER(HVAC_VER_ID)
    TO_STRING_MEMBER(REL_VENTILATION)
    TO_STRING_MEMBER(REL_HUMID_EXHAUST)
    TO_STRING_MEMBER(EXHAUST_CO2)
    TO_STRING_MEMBER(SUPPLY_INLET_TEMP)
    TO_STRING_MEMBER(SUPPLY_OUTLET_TEMP)
    TO_STRING_MEMBER(EXHAUST_INLET_TEMP)
    TO_STRING_MEMBER(EXHAUST_OUTLET_TEMP)
    TO_STRING_MEMBER(EXHAUST_FAN_SPEED)
    TO_STRING_MEMBER(SUPPLY_FAN_SPEED)
    TO_STRING_MEMBER(REMOTE_VENTILATION_PARAM)
    TO_STRING_MEMBER(NOM_REL_VENTILATION)
    TO_STRING_MEMBER(HVAC_NUM_TSP)
    TO_STRING_MEMBER(HVAC_IDX_TSP)
    TO_STRING_MEMBER(HVAC_FHB_SIZE)
    TO_STRING_MEMBER(HVAC_FHB_IDX)
    TO_STRING_MEMBER(RF_SIGNAL)
    TO_STRING_MEMBER(DHW_MODE)
    TO_STRING_MEMBER(OVERRIDE_FUNC)
    TO_STRING_MEMBER(SOLAR_MODE_FLAGS)
    TO_STRING_MEMBER(SOLAR_ASF)
    TO_STRING_MEMBER(SOLAR_VERSION_ID)
    TO_STRING_MEMBER(SOLAR_PRODUCT_ID)
    TO_STRING_MEMBER(SOLAR_NUM_TSP)
    TO_STRING_MEMBER(SOLAR_IDX_TSP)
    TO_STRING_MEMBER(SOLAR_FHB_SIZE)
    TO_STRING_MEMBER(SOLAR_FHB_IDX)
    TO_STRING_MEMBER(SOLAR_STARTS)
    TO_STRING_MEMBER(SOLAR_HOURS)
    TO_STRING_MEMBER(SOLAR_ENERGY)
    TO_STRING_MEMBER(SOLAR_TOTAL_ENERGY)
    TO_STRING_MEMBER(FAILED_BURNER_STARTS)
    TO_STRING_MEMBER(BURNER_FLAME_LOW)
    TO_STRING_MEMBER(OEM_DIAGNOSTIC)
    TO_STRING_MEMBER(BURNER_STARTS)
    TO_STRING_MEMBER(CH_PUMP_STARTS)
    TO_STRING_MEMBER(DHW_PUMP_STARTS)
    TO_STRING_MEMBER(DHW_BURNER_STARTS)
    TO_STRING_MEMBER(BURNER_HOURS)
    TO_STRING_MEMBER(CH_PUMP_HOURS)
    TO_STRING_MEMBER(DHW_PUMP_HOURS)
    TO_STRING_MEMBER(DHW_BURNER_HOURS)
    TO_STRING_MEMBER(OT_VERSION_CONTROLLER)
    TO_STRING_MEMBER(OT_VERSION_DEVICE)
    TO_STRING_MEMBER(VERSION_CONTROLLER)
    TO_STRING_MEMBER(VERSION_DEVICE)
    default:
      return "<INVALID>";
  }
}

void OpenTherm::debug_data(OpenthermData &data) {
  ESP_LOGD(TAG, "%s %s %s %s", format_bin(data.type).c_str(), format_bin(data.id).c_str(),
           format_bin(data.valueHB).c_str(), format_bin(data.valueLB).c_str());
  ESP_LOGD(TAG, "type: %s; id: %s; HB: %s; LB: %s; uint_16: %s; float: %s",
           this->message_type_to_str((MessageType) data.type), to_string(data.id).c_str(),
           to_string(data.valueHB).c_str(), to_string(data.valueLB).c_str(), to_string(data.u16()).c_str(),
           to_string(data.f88()).c_str());
}
void OpenTherm::debug_error(OpenThermProtocolError &error) const {
  ESP_LOGD(TAG,
           "OpenTherm protocol error: %s\n"
           "Bit index: %u\n"
           "Data: %s",
           OpenTherm::protocol_error_to_str(error.error_type), error.bit_index, format_hex(error.data).c_str());
}

void OpenTherm::debug_rmt() const {
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

// RX diagnostics removed for fresh implementation

float OpenthermData::f88() { return ((float) this->s16()) / 256.0; }

void OpenthermData::f88(float value) { this->s16((int16_t) (value * 256)); }

uint16_t OpenthermData::u16() {
  uint16_t const value = this->valueHB;
  return (value << 8) | this->valueLB;
}

void OpenthermData::u16(uint16_t value) {
  this->valueLB = value & 0xFF;
  this->valueHB = (value >> 8) & 0xFF;
}

int16_t OpenthermData::s16() {
  int16_t const value = this->valueHB;
  return (value << 8) | this->valueLB;
}

void OpenthermData::s16(int16_t value) {
  this->valueLB = value & 0xFF;
  this->valueHB = (value >> 8) & 0xFF;
}

}  // namespace opentherm
}  // namespace esphome
