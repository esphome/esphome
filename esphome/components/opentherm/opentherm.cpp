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

// --- RMT RX decoding helpers (IRAM) ---
constexpr uint32_t HALF_US = 500;          // half-bit period
constexpr uint32_t JIT_MINUS = 100;        // short: early edge tolerance
constexpr uint32_t JIT_PLUS = 200;         // short: late  edge tolerance (accept up to ~700us)
constexpr uint32_t LONG_JIT_MINUS = 200;   // long: early tolerance (accept down to ~800us)
constexpr uint32_t LONG_JIT_PLUS = 250;    // long: late tolerance  (accept up to ~1250us)
constexpr uint32_t SUM_TOL = 120;          // tolerance for sum of two half periods
constexpr uint32_t SHORT_MIN_FLOOR = 300;  // guard for extremely short glitches
constexpr uint32_t GLITCH_MIN_US = 200;    // ignore edges shorter than this

static inline IRAM_ATTR uint8_t classify_dt(uint32_t dt) {
  // Short half bit (about 500us)
  if (dt >= (HALF_US - JIT_MINUS) && dt <= (HALF_US + JIT_PLUS))
    return 0;  // short
  // Long half bit (about 1000us) — wider tolerance to account for line/boiler jitter
  if (dt >= (2 * HALF_US - LONG_JIT_MINUS) && dt <= (2 * HALF_US + LONG_JIT_PLUS))
    return 1;  // long
  return 2;    // invalid
}

static inline IRAM_ATTR int build_edges(const rmt_symbol_word_t *syms, size_t count, uint32_t *edge_t,
                                        uint8_t *edge_is_rise, int max_edges, uint32_t glitch_min_us,
                                        int *out_suppressed) {
  uint32_t t_acc = 0;
  int n = 0;
  int suppressed = 0;
  for (size_t i = 0; i < count && n < max_edges; i++) {
    uint32_t d0 = syms[i].duration0;
    uint32_t d1 = syms[i].duration1;
    if (d0 == 0)
      break;
    t_acc += d0;
    // mid-bit edge: add only if duration is not an ultra-short glitch
    if (d0 >= glitch_min_us) {
      edge_t[n] = t_acc;
      edge_is_rise[n] = (syms[i].level0 == 0 && syms[i].level1 == 1) ? 1 : 0;
      n++;
    } else {
      suppressed++;
    }
    if (d1 == 0)
      break;
    t_acc += d1;
    // boundary edge (only if level toggles to next symbol)
    if ((i + 1) < count && n < max_edges) {
      bool next_l0 = syms[i + 1].level0;
      bool cur_l1 = syms[i].level1;
      if (next_l0 != cur_l1) {
        if (d1 >= glitch_min_us) {
          edge_t[n] = t_acc;
          edge_is_rise[n] = (cur_l1 == 0 && next_l0 == 1) ? 1 : 0;
          n++;
        } else {
          suppressed++;
        }
      }
    }
  }
  if (out_suppressed)
    *out_suppressed = suppressed;
  return n;
}

static inline IRAM_ATTR bool collapse_decode(const uint32_t *edge_t, const uint8_t *edge_is_rise, int edge_count,
                                             int start_idx, bool rise_is_one, uint8_t out_bits[34]) {
  int ei = start_idx;
  int blen = 0;
  // Start bit must be '1'
  uint8_t start_bit = rise_is_one ? (edge_is_rise[ei] ? 1 : 0) : (edge_is_rise[ei] ? 0 : 1);
  if (start_bit != 1)
    return false;
  out_bits[blen++] = 1;
  while (blen < 34) {
    if (ei + 1 >= edge_count)
      return false;
    uint32_t dt1 = edge_t[ei + 1] - edge_t[ei];
    uint8_t c1 = classify_dt(dt1);
    if (c1 == 1) {
      ei = ei + 1;
    } else if (c1 == 0) {
      if (ei + 2 >= edge_count)
        return false;
      uint32_t dt2 = edge_t[ei + 2] - edge_t[ei + 1];
      uint8_t c2 = classify_dt(dt2);
      if (c2 == 0) {
        ei = ei + 2;  // two shorts as expected
      } else if (c2 == 1) {
        // Accept short + long as valid progression (boundary present then next boundary omitted)
        ei = ei + 2;
      } else {
        // Try forgiving check on sum for slightly skewed halves
        uint32_t sum = dt1 + dt2;
        if (dt1 >= SHORT_MIN_FLOOR && dt2 >= SHORT_MIN_FLOOR && sum >= (2 * HALF_US - SUM_TOL) &&
            sum <= (2 * HALF_US + SUM_TOL)) {
          ei = ei + 2;
        } else {
          return false;
        }
      }
    } else {
      // Fallback: accept a pair that sums to one bit time, even if individual halves
      // slightly violate short thresholds (observed e.g. 661us + 365us ~ 1026us).
      if (ei + 2 >= edge_count)
        return false;
      uint32_t dt2 = edge_t[ei + 2] - edge_t[ei + 1];
      uint32_t sum = dt1 + dt2;
      if (dt1 >= SHORT_MIN_FLOOR && dt2 >= SHORT_MIN_FLOOR && sum >= (2 * HALF_US - SUM_TOL) &&
          sum <= (2 * HALF_US + SUM_TOL)) {
        ei = ei + 2;  // treat as two shorts
      } else {
        // Also accept pattern: (almost short) + long
        uint8_t c2 = classify_dt(dt2);
        bool dt1_near_short = (dt1 >= SHORT_MIN_FLOOR && dt1 <= (HALF_US + JIT_PLUS));
        if (dt1_near_short && c2 == 1) {
          ei = ei + 2;
        } else {
          return false;
        }
      }
    }
    uint8_t bit = rise_is_one ? (edge_is_rise[ei] ? 1 : 0) : (edge_is_rise[ei] ? 0 : 1);
    out_bits[blen++] = bit;
  }
  return true;
}

static inline IRAM_ATTR bool scan_decode(const uint32_t *edge_t, const uint8_t *edge_is_rise, int edge_count,
                                         uint8_t out_bits[34]) {
  for (int start = 0; start < edge_count; start++) {
    for (int pol = 0; pol < 2; pol++) {
      bool rise_is_one = (pol == 0);
      if (!collapse_decode(edge_t, edge_is_rise, edge_count, start, rise_is_one, out_bits))
        continue;
      if (out_bits[0] == 1 && out_bits[33] == 1)
        return true;
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
    // Spurious/empty capture: mark as timeout so outer loop doesn't spin-wait
    self->mode_ = OperationMode::ERROR_TIMEOUT;
    self->rx_receiving_ = false;
    return false;
  }

  // Build edge list (timestamps in us) and directions
  constexpr int kMaxEdges = 256;
  uint32_t edge_t[kMaxEdges];
  uint8_t edge_dir_rise[kMaxEdges];  // 1 for rising, 0 for falling
  int suppressed = 0;
  int edge_count = build_edges(syms, count, edge_t, edge_dir_rise, kMaxEdges, GLITCH_MIN_US, &suppressed);

  if (edge_count < 2) {
    self->mode_ = OperationMode::ERROR_TIMEOUT;
    self->rx_receiving_ = false;
    return false;
  }

  // Decode bits from edges (try all starts and both polarities)
  uint8_t bits[34];
  bool frame_ok = scan_decode(edge_t, edge_dir_rise, edge_count, bits);

  if (!frame_ok) {
    self->mode_ = OperationMode::ERROR_PROTOCOL;
    self->error_type_ = ProtocolErrorType::NO_TRANSITION;
    // Capture diagnostics for later logging at non-ISR context
    // Copy raw symbols (bounded by RX_SYMBOL_CAPACITY)
    size_t to_copy = count;
    if (to_copy > RX_SYMBOL_CAPACITY)
      to_copy = RX_SYMBOL_CAPACITY;
    for (size_t i = 0; i < to_copy; i++) {
      self->diag_syms_[i] = syms[i];
    }
    self->diag_sym_count_ = to_copy;
    // Copy edges and compute classifications
    int store_edges = edge_count;
    if (store_edges > MAX_EDGES_STORE)
      store_edges = MAX_EDGES_STORE;
    self->diag_edge_count_ = store_edges;
    self->diag_cnt_short_ = 0;
    self->diag_cnt_long_ = 0;
    self->diag_cnt_invalid_ = 0;
    self->diag_edges_suppressed_ = suppressed;
    for (int i = 0; i < store_edges; i++) {
      self->diag_edge_t_[i] = edge_t[i];
      self->diag_edge_rise_[i] = edge_dir_rise[i];
      if (i > 0) {
        uint32_t dt = edge_t[i] - edge_t[i - 1];
        uint8_t cls = classify_dt(dt);
        if (cls == 0)
          self->diag_cnt_short_++;
        else if (cls == 1)
          self->diag_cnt_long_++;
        else
          self->diag_cnt_invalid_++;
      }
    }
    self->diag_rmt_clk_freq_ = RMT_CLK_FREQ;
    self->diag_range_min_ns_ = self->rx_config_.signal_range_min_ns;
    self->diag_range_max_ns_ = self->rx_config_.signal_range_max_ns;
    self->diag_valid_ = true;
    self->rx_receiving_ = false;
    return false;
  }

  // Validate start/stop bits
  if (bits[0] != 1 || bits[33] != 1) {
    self->mode_ = OperationMode::ERROR_PROTOCOL;
    self->error_type_ = ProtocolErrorType::INVALID_STOP_BIT;
    self->rx_receiving_ = false;
    return false;
  }

  // Assemble 32-bit payload [b1..b32] (parity + fields)
  uint32_t payload = 0;
  for (int i = 1; i <= 32; i++) {
    payload = (payload << 1) | (uint32_t) bits[i];
  }
  self->data_ = payload;

  // Parity check across all 32 bits
  if (!self->check_parity_(self->data_)) {
    self->mode_ = OperationMode::ERROR_PROTOCOL;
    self->error_type_ = ProtocolErrorType::PARITY_ERROR;
    self->rx_receiving_ = false;
    return false;
  }

  self->mode_ = OperationMode::RECEIVED;
  self->rx_receiving_ = false;
  return false;
}

void OpenTherm::start_read_rmt_() {
  // Start single receive into internal buffer
  memset(this->rx_buffer_, 0, sizeof(this->rx_buffer_));
  this->diag_valid_ = false;  // reset previous diagnostics before a new capture
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

void OpenTherm::log_no_transition_diagnostics() const {
  if (!this->diag_valid_ || this->error_type_ != ProtocolErrorType::NO_TRANSITION) {
    return;
  }

  ESP_LOGI(TAG, "NO_TRANSITION diagnostics begin =====================================");
  ESP_LOGI(TAG, "RMT config: clk_freq=%u Hz, resolution=%u Hz, range_min=%u ns, range_max=%u ns",
           (unsigned) this->diag_rmt_clk_freq_, (unsigned) RMT_RESOLUTION_HZ, (unsigned) this->diag_range_min_ns_,
           (unsigned) this->diag_range_max_ns_);

  ESP_LOGI(TAG, "Capture: symbols=%u, edges(stored)=%u, classify: short=%u long=%u invalid=%u",
           (unsigned) this->diag_sym_count_, (unsigned) this->diag_edge_count_, (unsigned) this->diag_cnt_short_,
           (unsigned) this->diag_cnt_long_, (unsigned) this->diag_cnt_invalid_);
  ESP_LOGI(TAG, "Glitch filter: threshold=%u us, suppressed_edges=%u", (unsigned) GLITCH_MIN_US,
           (unsigned) this->diag_edges_suppressed_);

  // Dump raw symbols
  for (size_t i = 0; i < this->diag_sym_count_; i++) {
    const auto &s = this->diag_syms_[i];
    ESP_LOGI(TAG, "SYM[%03u]: L0=%u D0=%u us | L1=%u D1=%u us", (unsigned) i, (unsigned) s.level0,
             (unsigned) s.duration0, (unsigned) s.level1, (unsigned) s.duration1);
  }

  // Dump edges and delta times
  for (int i = 0; i < this->diag_edge_count_; i++) {
    if (i == 0) {
      ESP_LOGI(TAG, "EDGE[%03d]: t=%u us, dir=%s", i, (unsigned) this->diag_edge_t_[i],
               this->diag_edge_rise_[i] ? "rise" : "fall");
    } else {
      uint32_t dt = this->diag_edge_t_[i] - this->diag_edge_t_[i - 1];
      uint8_t c = classify_dt(dt);
      const char *cls = (c == 0) ? "short" : (c == 1) ? "long" : "invalid";
      ESP_LOGI(TAG, "EDGE[%03d]: t=%u us, dt=%u us, %s, dir=%s", i, (unsigned) this->diag_edge_t_[i], (unsigned) dt,
               cls, this->diag_edge_rise_[i] ? "rise" : "fall");
    }
  }

  ESP_LOGI(TAG, "NO_TRANSITION diagnostics end =======================================");
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
