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
// RMT source clock is used by the RX range filter hardware. The maximum
// configurable minimum pulse width is limited to 255 source clock cycles.
#ifdef USE_ESP32_VARIANT_ESP32H2
static const uint32_t RMT_CLK_FREQ = 32000000;
#else
static const uint32_t RMT_CLK_FREQ = 80000000;
#endif

OpenTherm::OpenTherm(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin, int32_t device_timeout)
    : in_pin_(in_pin),
      out_pin_(out_pin),
      mode_(OperationMode::IDLE),
      error_type_(ProtocolErrorType::NO_ERROR),
      capture_(0),
      clock_(0),
      data_(0),
      bit_pos_(0),
      timeout_counter_(-1),
      device_timeout_(device_timeout) {
  this->isr_in_pin_ = in_pin->to_isr();
  this->isr_out_pin_ = out_pin->to_isr();
}

bool OpenTherm::initialize() {
  this->in_pin_->pin_mode(gpio::FLAG_INPUT);
  this->in_pin_->setup();
  this->out_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->out_pin_->setup();
  this->out_pin_->digital_write(true);

  return this->init_rmt_();
}

void OpenTherm::listen() {
  this->mode_ = OperationMode::LISTEN;
  this->data_ = 0;
  this->bit_pos_ = 0;
  this->start_read_rmt_();
}

void OpenTherm::send(OpenthermData &data) {
  this->data_ = data.type;
  this->data_ = (this->data_ << 12) | data.id;
  this->data_ = (this->data_ << 8) | data.valueHB;
  this->data_ = (this->data_ << 8) | data.valueLB;
  if (!check_parity_(this->data_)) {
    this->data_ = this->data_ | 0x80000000;
  }

  this->clock_ = 1;     // clock starts at HIGH
  this->bit_pos_ = 33;  // count down (33 == start bit, 32-1 data, 0 == stop bit)
  this->mode_ = OperationMode::WRITE;
  this->start_write_rmt_();
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

bool OpenTherm::get_protocol_error(OpenThermError &error) {
  if (this->mode_ != OperationMode::ERROR_PROTOCOL) {
    return false;
  }

  error.error_type = this->error_type_;
  error.bit_pos = this->bit_pos_;
  error.capture = this->capture_;
  error.clock = this->clock_;
  error.data = this->data_;

  return true;
}

void OpenTherm::stop() { this->mode_ = OperationMode::IDLE; }

// Timer ISR removed in RMT implementation

// (legacy) verify_stop_bit_ no longer used with RMT decoding

bool OpenTherm::init_rmt_() {
  // Configure RX channel
  rmt_rx_channel_config_t rx_chan_cfg;
  memset(&rx_chan_cfg, 0, sizeof(rx_chan_cfg));
  rx_chan_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
  rx_chan_cfg.resolution_hz = RMT_RESOLUTION_HZ;  // 1 tick = 1 us
  rx_chan_cfg.mem_block_symbols = 64;             // enough symbols
  rx_chan_cfg.gpio_num = gpio_num_t(this->in_pin_->get_pin());
  rx_chan_cfg.intr_priority = 0;
  rx_chan_cfg.flags.invert_in = 0;
  rx_chan_cfg.flags.with_dma = 0;
  rx_chan_cfg.flags.io_loop_back = 0;
  if (rmt_new_rx_channel(&rx_chan_cfg, &this->rx_channel_) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create RMT RX channel");
    return false;
  }

  // Configure TX channel
  rmt_tx_channel_config_t tx_chan_cfg;
  memset(&tx_chan_cfg, 0, sizeof(tx_chan_cfg));
  tx_chan_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
  tx_chan_cfg.resolution_hz = RMT_RESOLUTION_HZ;
  tx_chan_cfg.gpio_num = gpio_num_t(this->out_pin_->get_pin());
  tx_chan_cfg.mem_block_symbols = 64;
  tx_chan_cfg.trans_queue_depth = 1;
  tx_chan_cfg.flags.io_loop_back = 0;
  tx_chan_cfg.flags.io_od_mode = 0;
  tx_chan_cfg.flags.invert_out = 0;
  tx_chan_cfg.flags.with_dma = 0;
  tx_chan_cfg.intr_priority = 0;
  if (rmt_new_tx_channel(&tx_chan_cfg, &this->tx_channel_) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create RMT TX channel");
    return false;
  }

  // Simple copy encoder for raw symbols
  rmt_copy_encoder_config_t enc_cfg;
  memset(&enc_cfg, 0, sizeof(enc_cfg));
  if (rmt_new_copy_encoder(&enc_cfg, &this->tx_encoder_) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create RMT TX encoder");
    return false;
  }

  if (rmt_enable(this->rx_channel_) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable RMT RX channel");
    return false;
  }
  if (rmt_enable(this->tx_channel_) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable RMT TX channel");
    return false;
  }

  // RX done callback
  rmt_rx_event_callbacks_t cbs;
  memset(&cbs, 0, sizeof(cbs));
  cbs.on_recv_done = &OpenTherm::rmt_rx_callback;
  if (rmt_rx_register_event_callbacks(this->rx_channel_, &cbs, this) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register RMT RX callbacks");
    return false;
  }

  // Configure receive timing window
  memset(&this->rx_config_, 0, sizeof(this->rx_config_));
  // Filter out short glitches; consider signal ended after >2000us without edge
  // Note: signal_range_min_ns must be < 255 cycles of the RMT source clock.
  // Clamp to the hardware-supported maximum to avoid runtime errors.
  uint32_t max_filter_ns = 255 * 1000 / (RMT_CLK_FREQ / 1000000);
  this->rx_config_.signal_range_min_ns = std::min(static_cast<uint32_t>(100 * 1000), max_filter_ns);
  this->rx_config_.signal_range_max_ns = 2000 * 1000;

  return true;
}

// RX decoding removed for a fresh implementation start.
bool IRAM_ATTR OpenTherm::rmt_rx_callback(rmt_channel_handle_t, const rmt_rx_done_event_data_t *, void *) {
  return false;
}

void OpenTherm::start_read_rmt_() {
  // Start single receive into internal buffer
  memset(this->rx_buffer_, 0, sizeof(this->rx_buffer_));
  if (!this->rx_receiving_) {
    esp_err_t err = rmt_receive(this->rx_channel_, this->rx_buffer_, sizeof(this->rx_buffer_), &this->rx_config_);
    if (err == ESP_OK) {
      this->rx_receiving_ = true;
    } else {
      ESP_LOGW(TAG, "Failed to start RMT receive: %s", esp_err_to_name(err));
      this->mode_ = OperationMode::ERROR_TIMEOUT;
    }
  }
}

void OpenTherm::start_write_rmt_() {
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

  rmt_transmit_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.loop_count = 0;
  cfg.flags.eot_level = 1;  // idle high after transmit

  esp_err_t err = rmt_transmit(this->tx_channel_, this->tx_encoder_, syms, sizeof(syms), &cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to transmit RMT symbols: %s", esp_err_to_name(err));
    this->mode_ = OperationMode::ERROR_PROTOCOL;
    return;
  }
  // Wait until transmission completes to move to SENT state (simple and robust)
  err = rmt_tx_wait_all_done(this->tx_channel_, -1);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed waiting for TX done: %s", esp_err_to_name(err));
    this->mode_ = OperationMode::ERROR_PROTOCOL;
    return;
  }
  this->mode_ = OperationMode::SENT;
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
    TO_STRING_MEMBER(ERROR_TIMER)
    default:
      return "<INVALID>";
  }
}
const char *OpenTherm::protocol_error_to_str(ProtocolErrorType error_type) {
  switch (error_type) {
    TO_STRING_MEMBER(NO_ERROR)
    TO_STRING_MEMBER(NO_TRANSITION)
    TO_STRING_MEMBER(INVALID_STOP_BIT)
    TO_STRING_MEMBER(PARITY_ERROR)
    TO_STRING_MEMBER(NO_CHANGE_TOO_LONG)
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
void OpenTherm::debug_error(OpenThermError &error) const {
  ESP_LOGD(TAG, "OpenTherm error: %s (bit_pos=%u)", OpenTherm::protocol_error_to_str(error.error_type),
           (unsigned) error.bit_pos);
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
