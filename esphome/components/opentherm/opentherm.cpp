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

ProtocolErrorType IRAM_ATTR OpenTherm::verify_stop_bit_(uint8_t value) {
  if (value) {  // stop bit detected
    return check_parity_(this->data_) ? ProtocolErrorType::NO_ERROR : ProtocolErrorType::PARITY_ERROR;
  } else {  // no stop bit detected, error
    return ProtocolErrorType::INVALID_STOP_BIT;
  }
}

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
  if (this->in_pin_->get_flags() & gpio::FLAG_PULLUP) {
    gpio_pullup_en(gpio_num_t(this->in_pin_->get_pin()));
  } else {
    gpio_pullup_dis(gpio_num_t(this->in_pin_->get_pin()));
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
  tx_chan_cfg.flags.io_od_mode = (this->out_pin_->get_flags() & gpio::FLAG_OPEN_DRAIN) ? 1 : 0;
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
  {
    uint32_t const max_filter_ns = 255u * 1000u / (RMT_CLK_FREQ / 1000000u);
    uint32_t desired_min_ns = 100u * 1000u;  // 100us target, will be clamped
    // Must be strictly less than the limit reported by the driver
    uint32_t safe_min_ns = (max_filter_ns > 0) ? (max_filter_ns - 1) : 0;
    if (desired_min_ns > safe_min_ns)
      desired_min_ns = safe_min_ns;
    this->rx_config_.signal_range_min_ns = desired_min_ns;
  }
  this->rx_config_.signal_range_max_ns = 2000u * 1000u;

  return true;
}

// Helper to sample level at specific time offset (us) from first captured edge
static inline bool sample_level_at(const rmt_symbol_word_t *syms, size_t n, uint32_t t_us) {
  uint32_t acc = 0;
  bool level = syms[0].level0;
  for (size_t i = 0; i < n; i++) {
    uint32_t d0 = syms[i].duration0;
    if (d0 == 0)
      break;
    if (acc + d0 > t_us)
      return syms[i].level0;  // t_us lies within duration0
    acc += d0;
    level = syms[i].level1;
    uint32_t d1 = syms[i].duration1;
    if (d1 == 0)
      return level;
    if (acc + d1 > t_us)
      return level;
    acc += d1;
    // After duration1, the signal level equals the last level (level1) until the next symbol starts.
    // The next symbol's level0 must equal this for continuity per RMT driver.
    // Do NOT reset to syms[i].level0 here.
    // Keep 'level' as level1 for the subsequent iteration.
  }
  return level;
}

// Helper: convert a sequence of RMT symbols into sequential level-duration spans
// and locate the start bit (idle-high gap -> low ~500us -> high ~500us).
static inline bool IRAM_ATTR find_start_timestamp_us(const rmt_symbol_word_t *syms, size_t count, uint32_t *t_start_out,
                                                     bool *low_high_out) {
  if (syms == nullptr || count == 0)
    return false;
  // Tolerances for half-bit durations
  const uint32_t HALF_MIN = 300;  // us
  const uint32_t HALF_MAX = 700;  // us
  // Find the first [low ~500us, high ~500us] or [high ~500us, low ~500us] pair and
  // use its first half as the frame start. Report whether the pair is low->high.

  struct Span {
    bool level;
    uint32_t dur;
    uint32_t start;
    bool valid;
  } a{true, 0, 0, false}, b{true, 0, 0, false}, c{true, 0, 0, false};

  uint32_t t_us = 0;
  // iterate spans in order
  for (size_t i = 0; i < count; i++) {
    bool levels[2] = {syms[i].level0, syms[i].level1};
    uint32_t durs[2] = {syms[i].duration0, syms[i].duration1};
    for (int k = 0; k < 2; k++) {
      if (durs[k] == 0)
        break;
      // shift history: a <- b <- c <- new
      a = b;
      b = c;
      c.level = levels[k];
      c.dur = durs[k];
      c.start = t_us;
      c.valid = true;

      // Check for [b ~500us], [c ~500us] with different levels
      if (a.valid && b.valid && c.valid) {
        bool b_ok = (b.dur >= HALF_MIN && b.dur <= HALF_MAX);
        bool c_ok = (c.dur >= HALF_MIN && c.dur <= HALF_MAX);
        if (b_ok && c_ok && (b.level != c.level)) {
          *t_start_out = b.start;  // start at beginning of the first half
          *low_high_out = (b.level == 0 && c.level == 1);
          return true;
        }
      }
      t_us += durs[k];
    }
  }
  return false;
}

bool IRAM_ATTR OpenTherm::rmt_rx_callback(rmt_channel_handle_t, const rmt_rx_done_event_data_t *evt, void *arg) {
  auto *self = static_cast<OpenTherm *>(arg);
  // Start decoding
  self->mode_ = OperationMode::READ;

  auto *syms = evt->received_symbols;
  size_t count = evt->num_symbols;
  if (syms == nullptr || count == 0) {
    self->mode_ = OperationMode::ERROR_PROTOCOL;
    self->error_type_ = ProtocolErrorType::NO_CHANGE_TOO_LONG;
    return false;
  }

  // Store debug symbols for later logging outside ISR
  size_t dbg_n = (count < OpenTherm::DEBUG_RX_SYMBOLS) ? count : OpenTherm::DEBUG_RX_SYMBOLS;
  for (size_t i = 0; i < dbg_n; i++)
    self->last_rx_symbols_[i] = syms[i];
  self->last_rx_symbol_count_ = dbg_n;

  // Build half-bit levels (0/1) by sampling every ~500us starting at a few different phases and step sizes
  uint8_t halves[256];
  uint32_t total_us = 0;
  for (size_t i = 0; i < count; i++) {
    uint32_t d0 = syms[i].duration0;
    uint32_t d1 = syms[i].duration1;
    if (d0 == 0)
      break;
    total_us += d0;
    if (d1 == 0)
      break;
    total_us += d1;
  }
  const uint32_t PAD_US = 2000;  // ensure we cover stop-bit tail into idle
  int n_halves_base = (int) ((total_us + PAD_US) / 500);
  if (n_halves_base > 256)
    n_halves_base = 256;
  // phases to try (us)
  const uint32_t phases[] = {200, 250, 300};
  // step sizes to try (us)
  const uint32_t steps[] = {490, 495, 500, 505, 510};
  int chosen_phase_idx = -1;
  int chosen_step_idx = -1;
  int n_halves = 0;
  // Try to find a valid frame by scanning phases, polarities and start indices
  auto is_one = [](bool pol_lh, uint8_t h0, uint8_t h1) -> bool {
    return pol_lh ? (h0 == 0 && h1 == 1) : (h0 == 1 && h1 == 0);
  };
  uint32_t bits = 0;
  bool found = false;
  bool pol_lh = true;
  int start_half = -1;
  self->last_scan_info_count_ = 0;
  for (size_t ph = 0; ph < sizeof(phases) / sizeof(phases[0]) && !found; ph++) {
    uint32_t phase = phases[ph];
    for (size_t st = 0; st < sizeof(steps) / sizeof(steps[0]) && !found; st++) {
      uint32_t step = steps[st];
      n_halves = (int) ((total_us + PAD_US) / step);
      if (n_halves > 256)
        n_halves = 256;
      for (int k = 0; k < n_halves; k++) {
        uint32_t t = phase + k * step;
        halves[k] = sample_level_at(syms, count, t) ? 1 : 0;
      }
      // Try both polarities and all plausible start positions
      for (int pol = 0; pol < 2 && !found; pol++) {
        pol_lh = (pol == 0);
        for (int s = 0; s + 33 * 2 < n_halves; s++) {
          if (!is_one(pol_lh, halves[s + 0], halves[s + 1]))
            continue;
          uint32_t tmp = 0;
          bool ok = true;
          uint8_t fail_bit = 255;
          for (int i = 1; i <= 32; i++) {
            int idx = s + i * 2;
            if (idx + 1 >= n_halves) {
              ok = false;
              fail_bit = (uint8_t) i;
              break;
            }
            uint8_t a = halves[idx + 0];
            uint8_t b = halves[idx + 1];
            if (a == b) {
              ok = false;
              fail_bit = (uint8_t) i;
              break;
            }
            uint8_t bit = is_one(pol_lh, a, b) ? 1 : 0;
            tmp = (tmp << 1) | bit;
          }
          if (!ok) {
            if (self->last_scan_info_count_ < OpenTherm::MAX_SCAN_INFO) {
              self->last_scan_info_[self->last_scan_info_count_++] =
                  OpenTherm::ScanInfo{(uint8_t) ph, (uint8_t) s, (uint8_t) (pol_lh ? 1 : 0), fail_bit};
            }
            continue;
          }
          if (!is_one(pol_lh, halves[s + 33 * 2 + 0], halves[s + 33 * 2 + 1]))
            continue;
          // Found a valid frame
          bits = tmp;
          start_half = s;
          chosen_phase_idx = (int) ph;
          chosen_step_idx = (int) st;
          found = true;
          break;
        }
      }
    }
  }

  // Save debug halves snapshot (from the first phase for visibility)
  self->last_halves_count_ = (uint16_t) n_halves_base;
  for (int i = 0; i < n_halves_base && i < 32; i++)
    self->last_halves_[i] = sample_level_at(syms, count, phases[0] + i * 500) ? 1 : 0;

  if (!found) {
    self->mode_ = OperationMode::ERROR_PROTOCOL;
    self->error_type_ = ProtocolErrorType::NO_TRANSITION;
    return false;
  }

  // Record first few half samples for debug
  for (int i = 0; i < 6; i++) {
    int idx = start_half + i * 2;
    if (idx + 1 < n_halves) {
      self->last_sample_L_[i] = halves[idx + 0];
      self->last_sample_R_[i] = halves[idx + 1];
    }
  }
  self->last_polarity_low_high_ = pol_lh;
  // Approximate start time in microseconds by summing half periods
  uint32_t phase_us = phases[chosen_phase_idx >= 0 ? chosen_phase_idx : 0];
  uint32_t step_us = steps[chosen_step_idx >= 0 ? chosen_step_idx : 2 /*500*/];
  self->last_t_start_us_ = (uint32_t) (phase_us + start_half * step_us);

  self->data_ = bits;  // 32-bit payload including parity
  if (!self->check_parity_(self->data_)) {
    self->mode_ = OperationMode::ERROR_PROTOCOL;
    self->error_type_ = ProtocolErrorType::PARITY_ERROR;
    return false;
  }

  self->mode_ = OperationMode::RECEIVED;
  return false;
}

void OpenTherm::start_read_rmt_() {
  // Start single receive into internal buffer
  memset(this->rx_buffer_, 0, sizeof(this->rx_buffer_));
  if (rmt_receive(this->rx_channel_, this->rx_buffer_, sizeof(this->rx_buffer_), &this->rx_config_) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start RMT receive");
    this->mode_ = OperationMode::ERROR_PROTOCOL;
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
  ESP_LOGD(TAG, "data: %s; clock: %s; capture: %s; bit_pos: %s", format_hex(error.data).c_str(),
           to_string(clock_).c_str(), format_bin(error.capture).c_str(), to_string(error.bit_pos).c_str());
  ESP_LOGD(TAG, "rxdbg: start=%u us, polarity=%s, symbols=%u", (unsigned) this->last_t_start_us_,
           this->last_polarity_low_high_ ? "low->high" : "high->low", (unsigned) this->last_rx_symbol_count_);
  uint32_t to_show = (this->last_rx_symbol_count_ < 8) ? this->last_rx_symbol_count_ : 8u;
  for (uint32_t i = 0; i < to_show; i++) {
    const auto &s = this->last_rx_symbols_[i];
    ESP_LOGD(TAG, "  sym[%u]: L0=%u D0=%u | L1=%u D1=%u", (unsigned) i, (unsigned) s.level0, (unsigned) s.duration0,
             (unsigned) s.level1, (unsigned) s.duration1);
  }
  // Show first few sampled half-bit levels before polarity adjustment
  ESP_LOGD(TAG, "  sample L: %u %u %u %u %u %u", (unsigned) this->last_sample_L_[0], (unsigned) this->last_sample_L_[1],
           (unsigned) this->last_sample_L_[2], (unsigned) this->last_sample_L_[3], (unsigned) this->last_sample_L_[4],
           (unsigned) this->last_sample_L_[5]);
  ESP_LOGD(TAG, "  sample R: %u %u %u %u %u %u", (unsigned) this->last_sample_R_[0], (unsigned) this->last_sample_R_[1],
           (unsigned) this->last_sample_R_[2], (unsigned) this->last_sample_R_[3], (unsigned) this->last_sample_R_[4],
           (unsigned) this->last_sample_R_[5]);
  if (this->last_fail_bit_ != 255) {
    ESP_LOGD(TAG, "  fail@bit=%u L=%u R=%u adj_L=%u adj_R=%u align_delta=%d", (unsigned) this->last_fail_bit_,
             (unsigned) this->last_fail_L_, (unsigned) this->last_fail_R_, (unsigned) this->last_fail_L_adj_,
             (unsigned) this->last_fail_R_adj_, (int) this->last_align_delta_);
  }
  ESP_LOGD(TAG, "  halves_count=%u first16=%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
           (unsigned) this->last_halves_count_, (unsigned) this->last_halves_[0], (unsigned) this->last_halves_[1],
           (unsigned) this->last_halves_[2], (unsigned) this->last_halves_[3], (unsigned) this->last_halves_[4],
           (unsigned) this->last_halves_[5], (unsigned) this->last_halves_[6], (unsigned) this->last_halves_[7],
           (unsigned) this->last_halves_[8], (unsigned) this->last_halves_[9], (unsigned) this->last_halves_[10],
           (unsigned) this->last_halves_[11], (unsigned) this->last_halves_[12], (unsigned) this->last_halves_[13],
           (unsigned) this->last_halves_[14], (unsigned) this->last_halves_[15]);
  if (this->last_scan_info_count_ > 0) {
    ESP_LOGD(TAG, "  scan candidates (phase,start,pol,failbit): count=%u", (unsigned) this->last_scan_info_count_);
    for (uint8_t i = 0; i < this->last_scan_info_count_; i++) {
      auto s = this->last_scan_info_[i];
      ESP_LOGD(TAG, "    (%u,%u,%u,%u)", (unsigned) s.phase_idx, (unsigned) s.start_half, (unsigned) s.pol_low_high,
               (unsigned) s.fail_bit);
    }
  }
}

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
