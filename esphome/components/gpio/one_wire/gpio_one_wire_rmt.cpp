#include "gpio_one_wire.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ONE_WIRE_RMT

#include <cstring>
#include <driver/gpio.h>

namespace esphome::gpio {

static const char *const TAG = "gpio.one_wire";

// ---------------------------------------------------------------------------
// RMT resolution and memory sizing
// ---------------------------------------------------------------------------

// 1 MHz resolution → 1 tick = 1 µs.  All timing constants below are in µs.
static const uint32_t RMT_RESOLUTION_HZ = 1000000;

// Maximum number of RMT symbols we ever need to receive in one transaction.
// read64() is the largest user at 8 bytes × 8 bits = 64 symbols.
static const size_t MAX_RX_SYMBOLS = 64;

// ---------------------------------------------------------------------------
// 1-wire timing constants (µs = ticks at 1 MHz)
// See https://www.maximintegrated.com/en/design/technical-documents/app-notes/3/3829.html
// ---------------------------------------------------------------------------
static const uint32_t RESET_PULSE_DURATION = 500;
static const uint32_t RESET_WAIT_DURATION = 200;
static const uint32_t RESET_PRESENCE_WAIT_MIN = 15;
static const uint32_t RESET_PRESENCE_DURATION_MIN = 60;
static const uint32_t SLOT_START = 2;
static const uint32_t SLOT_BIT = 60;
static const uint32_t SLOT_RECOVERY = 5;
static const uint32_t SLOT_SAMPLE_TIME = 15;

// ---------------------------------------------------------------------------
// Helper: build an rmt_symbol_word_t from four fields
// ---------------------------------------------------------------------------
static inline rmt_symbol_word_t make_symbol(uint16_t dur0, uint8_t lvl0, uint16_t dur1, uint8_t lvl1) {
  rmt_symbol_word_t s = {};
  s.duration0 = dur0;
  s.level0 = lvl0;
  s.duration1 = dur1;
  s.level1 = lvl1;
  return s;
}

// ---------------------------------------------------------------------------
// RMT TX/RX configs (static, shared across all instances)
// ---------------------------------------------------------------------------

// eot_level = 1: release the bus (HIGH) when TX goes idle
static const rmt_transmit_config_t TX_CONFIG = {
    .loop_count = 0,
    .flags = {.eot_level = 1},
};

// Accept any pulse between 1 µs and the full reset window
static const rmt_receive_config_t RX_CONFIG = {
    // signal_range_min_ns = 1e9 / resolution = 1 µs minimum accepted pulse width
    .signal_range_min_ns = 1000000000 / RMT_RESOLUTION_HZ,
    .signal_range_max_ns = (RESET_PULSE_DURATION + RESET_WAIT_DURATION) * 1000,
};

// ---------------------------------------------------------------------------
// ISR callback: notifies the task waiting for RX completion
// ---------------------------------------------------------------------------
static bool IRAM_ATTR rx_done_cb(rmt_channel_handle_t /*channel*/, const rmt_rx_done_event_data_t *edata,
                                 void *user_data) {
  BaseType_t task_woken = pdFALSE;
  xQueueSendFromISR(static_cast<QueueHandle_t>(user_data), edata, &task_woken);
  return task_woken;
}

// ---------------------------------------------------------------------------
// destroy_(): release all RMT resources.  Safe to call at any point during
// or after setup() — each resource is only freed if it was created.
// ---------------------------------------------------------------------------
void GPIOOneWireBus::destroy_() {
  if (this->tx_bytes_encoder_) {
    rmt_del_encoder(this->tx_bytes_encoder_);
    this->tx_bytes_encoder_ = nullptr;
  }
  if (this->tx_copy_encoder_) {
    rmt_del_encoder(this->tx_copy_encoder_);
    this->tx_copy_encoder_ = nullptr;
  }
  if (this->rx_channel_) {
    rmt_disable(this->rx_channel_);
    rmt_del_channel(this->rx_channel_);
    this->rx_channel_ = nullptr;
  }
  if (this->tx_channel_) {
    rmt_disable(this->tx_channel_);
    rmt_del_channel(this->tx_channel_);
    this->tx_channel_ = nullptr;
  }
  if (this->receive_queue_) {
    vQueueDelete(this->receive_queue_);
    this->receive_queue_ = nullptr;
  }
  if (this->rx_symbols_buf_) {
    RAMAllocator<rmt_symbol_word_t>().deallocate(this->rx_symbols_buf_, MAX_RX_SYMBOLS);
    this->rx_symbols_buf_ = nullptr;
  }
}

// ---------------------------------------------------------------------------
// setup() — RMT path
// ---------------------------------------------------------------------------
void GPIOOneWireBus::setup() {
  // Bytes encoder — LSB-first; used for multi-byte write and read-clock TX
  rmt_bytes_encoder_config_t bytes_enc_cfg = {};
  bytes_enc_cfg.bit0 = make_symbol(SLOT_START + SLOT_BIT, 0, SLOT_RECOVERY, 1);
  bytes_enc_cfg.bit1 = make_symbol(SLOT_START, 0, SLOT_BIT + SLOT_RECOVERY, 1);
  bytes_enc_cfg.flags.msb_first = 0;
  if (rmt_new_bytes_encoder(&bytes_enc_cfg, &this->tx_bytes_encoder_) != ESP_OK) {
    this->destroy_();
    this->mark_failed(LOG_STR("Failed to create bytes encoder"));
    return;
  }

  // Copy encoder — used for reset pulse and single-bit operations
  rmt_copy_encoder_config_t copy_enc_cfg = {};
  if (rmt_new_copy_encoder(&copy_enc_cfg, &this->tx_copy_encoder_) != ESP_OK) {
    this->destroy_();
    this->mark_failed(LOG_STR("Failed to create copy encoder"));
    return;
  }

  // Receive-done queue (depth 1 — we always drain before the next operation)
  this->receive_queue_ = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
  if (this->receive_queue_ == nullptr) {
    this->destroy_();
    this->mark_failed(LOG_STR("Failed to create receive queue"));
    return;
  }

  // RX symbol buffer — must hold up to MAX_RX_SYMBOLS symbols
  this->rx_symbols_buf_ =
      RAMAllocator<rmt_symbol_word_t>(RAMAllocator<rmt_symbol_word_t>::ALLOC_INTERNAL).allocate(MAX_RX_SYMBOLS);
  if (this->rx_symbols_buf_ == nullptr) {
    this->destroy_();
    this->mark_failed(LOG_STR("Failed to allocate RX symbol buffer"));
    return;
  }

  auto gpio_num = static_cast<gpio_num_t>(this->t_pin_->get_pin());

  // RX channel must be created BEFORE TX channel (ESP-IDF requirement for
  // loop-back: RX claims the GPIO first, TX piggy-backs on it)
  rmt_rx_channel_config_t rx_cfg = {};
  rx_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
  rx_cfg.resolution_hz = RMT_RESOLUTION_HZ;
  rx_cfg.gpio_num = gpio_num;
  rx_cfg.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL;
  if (rmt_new_rx_channel(&rx_cfg, &this->rx_channel_) != ESP_OK) {
    this->destroy_();
    this->mark_failed(LOG_STR("Failed to create RMT RX channel"));
    return;
  }

  rmt_rx_event_callbacks_t cbs = {.on_recv_done = rx_done_cb};
  if (rmt_rx_register_event_callbacks(this->rx_channel_, &cbs, this->receive_queue_) != ESP_OK) {
    this->destroy_();
    this->mark_failed(LOG_STR("Failed to register RMT RX callback"));
    return;
  }

  // TX channel — open-drain + loop-back so TX and RX share the same GPIO
  rmt_tx_channel_config_t tx_cfg = {};
  tx_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
  tx_cfg.resolution_hz = RMT_RESOLUTION_HZ;
  tx_cfg.gpio_num = gpio_num;
  tx_cfg.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL;
  tx_cfg.trans_queue_depth = 4;
  tx_cfg.flags.io_loop_back = true;  // TX output feeds back into RX input
  tx_cfg.flags.io_od_mode = true;    // Open-drain required for 1-wire
  if (rmt_new_tx_channel(&tx_cfg, &this->tx_channel_) != ESP_OK) {
    this->destroy_();
    this->mark_failed(LOG_STR("Failed to create RMT TX channel"));
    return;
  }

  // Enable the internal pull-up (useful for short wire runs; for longer buses
  // an external 4.7 kΩ resistor to VCC is recommended by the datasheet)
  gpio_set_pull_mode(gpio_num, GPIO_PULLUP_ONLY);

  if (rmt_enable(this->rx_channel_) != ESP_OK) {
    this->destroy_();
    this->mark_failed(LOG_STR("Failed to enable RMT RX channel"));
    return;
  }
  if (rmt_enable(this->tx_channel_) != ESP_OK) {
    this->destroy_();
    this->mark_failed(LOG_STR("Failed to enable RMT TX channel"));
    return;
  }

  // Release the bus so it is in a known HIGH (idle) state before the first reset.
  // Wait for the transmission to complete before starting the device search.
  rmt_symbol_word_t release = make_symbol(1, 1, 0, 1);
  if (rmt_transmit(this->tx_channel_, this->tx_copy_encoder_, &release, sizeof(release), &TX_CONFIG) != ESP_OK ||
      rmt_tx_wait_all_done(this->tx_channel_, 1000) != ESP_OK) {
    this->destroy_();
    this->mark_failed(LOG_STR("Failed to release bus"));
    return;
  }

  this->search();
}

// ---------------------------------------------------------------------------
// reset_int() — RMT path
// Returns: 1 = device(s) present, 0 = no device, -1 = signal error
// ---------------------------------------------------------------------------
int GPIOOneWireBus::reset_int() {
  rmt_symbol_word_t reset_sym = make_symbol(RESET_PULSE_DURATION, 0, RESET_WAIT_DURATION, 1);

  // Drain any stale event left by a previous failed/timed-out operation.
  // If rmt_transmit() ever fails after rmt_receive() is armed, the idle-HIGH
  // bus (>700 µs silence) auto-triggers the done callback within one
  // signal_range_max window, leaving a stale item in the queue.
  xQueueReset(this->receive_queue_);

  if (rmt_receive(this->rx_channel_, this->rx_symbols_buf_, 2 * sizeof(rmt_symbol_word_t), &RX_CONFIG) != ESP_OK)
    return -1;
  if (rmt_transmit(this->tx_channel_, this->tx_copy_encoder_, &reset_sym, sizeof(reset_sym), &TX_CONFIG) != ESP_OK)
    return -1;

  rmt_rx_done_event_data_t rx_data;
  if (xQueueReceive(this->receive_queue_, &rx_data, pdMS_TO_TICKS(1000)) != pdPASS)
    return -1;

  if (rx_data.num_symbols < 2)
    return 0;

  const rmt_symbol_word_t *s = rx_data.received_symbols;
  bool present;
  if (s[0].level1 == 1) {
    // Normal case: bus was HIGH before reset
    present = s[0].duration1 > RESET_PRESENCE_WAIT_MIN && s[1].duration0 > RESET_PRESENCE_DURATION_MIN;
  } else {
    // First reset after channel init: bus was LOW before reset
    present = s[0].duration0 > RESET_PRESENCE_WAIT_MIN && s[1].duration1 > RESET_PRESENCE_DURATION_MIN;
  }
  return present ? 1 : 0;
}

// ---------------------------------------------------------------------------
// write8() / write64() — RMT path
// ---------------------------------------------------------------------------
void GPIOOneWireBus::write8(uint8_t val) {
  if (rmt_transmit(this->tx_channel_, this->tx_bytes_encoder_, &val, 1, &TX_CONFIG) != ESP_OK ||
      rmt_tx_wait_all_done(this->tx_channel_, 50) != ESP_OK) {
    ESP_LOGE(TAG, "write8 failed");
  }
}

void GPIOOneWireBus::write64(uint64_t val) {
  if (rmt_transmit(this->tx_channel_, this->tx_bytes_encoder_, &val, sizeof(val), &TX_CONFIG) != ESP_OK ||
      rmt_tx_wait_all_done(this->tx_channel_, 100) != ESP_OK) {
    ESP_LOGE(TAG, "write64 failed");
  }
}

// ---------------------------------------------------------------------------
// read8() / read64() — RMT path
// The master sends 0xFF bytes (write-1 slots) to generate read clock pulses;
// the RX channel (loop-back) captures whether each slot stays HIGH (bit = 1)
// or is pulled LOW by the device (bit = 0), decoded from the pulse duration.
// ---------------------------------------------------------------------------
uint8_t GPIOOneWireBus::read8() {
  uint8_t tx_buf = 0xFF;
  uint8_t result = 0;

  xQueueReset(this->receive_queue_);
  if (rmt_receive(this->rx_channel_, this->rx_symbols_buf_, 8 * sizeof(rmt_symbol_word_t), &RX_CONFIG) != ESP_OK)
    return 0;
  if (rmt_transmit(this->tx_channel_, this->tx_bytes_encoder_, &tx_buf, 1, &TX_CONFIG) != ESP_OK)
    return 0;

  rmt_rx_done_event_data_t rx_data;
  if (xQueueReceive(this->receive_queue_, &rx_data, pdMS_TO_TICKS(1000)) != pdPASS) {
    ESP_LOGE(TAG, "read8 timeout");
    return 0;
  }

  for (size_t i = 0; i < rx_data.num_symbols && i < 8; i++) {
    // duration0 ≤ SLOT_SAMPLE_TIME µs → bus went HIGH quickly → device sent 1
    if (rx_data.received_symbols[i].duration0 <= SLOT_SAMPLE_TIME)
      result |= (1u << i);
  }
  return result;
}

uint64_t GPIOOneWireBus::read64() {
  uint8_t tx_buf[8];
  memset(tx_buf, 0xFF, sizeof(tx_buf));
  uint64_t result = 0;

  xQueueReset(this->receive_queue_);
  if (rmt_receive(this->rx_channel_, this->rx_symbols_buf_, MAX_RX_SYMBOLS * sizeof(rmt_symbol_word_t), &RX_CONFIG) !=
      ESP_OK)
    return 0;
  if (rmt_transmit(this->tx_channel_, this->tx_bytes_encoder_, tx_buf, sizeof(tx_buf), &TX_CONFIG) != ESP_OK)
    return 0;

  rmt_rx_done_event_data_t rx_data;
  if (xQueueReceive(this->receive_queue_, &rx_data, pdMS_TO_TICKS(1000)) != pdPASS) {
    ESP_LOGE(TAG, "read64 timeout");
    return 0;
  }

  for (size_t i = 0; i < rx_data.num_symbols && i < 64; i++) {
    if (rx_data.received_symbols[i].duration0 <= SLOT_SAMPLE_TIME)
      result |= (uint64_t(1) << i);
  }
  return result;
}

// ---------------------------------------------------------------------------
// read_bit_() / write_bit_() — RMT path (used by search_int)
// ---------------------------------------------------------------------------
bool GPIOOneWireBus::read_bit_() {
  rmt_symbol_word_t bit1_sym = make_symbol(SLOT_START, 0, SLOT_BIT + SLOT_RECOVERY, 1);

  xQueueReset(this->receive_queue_);
  if (rmt_receive(this->rx_channel_, this->rx_symbols_buf_, sizeof(rmt_symbol_word_t), &RX_CONFIG) != ESP_OK)
    return false;
  if (rmt_transmit(this->tx_channel_, this->tx_copy_encoder_, &bit1_sym, sizeof(bit1_sym), &TX_CONFIG) != ESP_OK)
    return false;

  rmt_rx_done_event_data_t rx_data;
  if (xQueueReceive(this->receive_queue_, &rx_data, pdMS_TO_TICKS(1000)) != pdPASS)
    return false;

  if (rx_data.num_symbols == 0)
    return false;
  return rx_data.received_symbols[0].duration0 <= SLOT_SAMPLE_TIME;
}

void GPIOOneWireBus::write_bit_(bool bit) {
  rmt_symbol_word_t sym = bit ? make_symbol(SLOT_START, 0, SLOT_BIT + SLOT_RECOVERY, 1)
                              : make_symbol(SLOT_START + SLOT_BIT, 0, SLOT_RECOVERY, 1);
  if (rmt_transmit(this->tx_channel_, this->tx_copy_encoder_, &sym, sizeof(sym), &TX_CONFIG) != ESP_OK ||
      rmt_tx_wait_all_done(this->tx_channel_, 50) != ESP_OK) {
    ESP_LOGE(TAG, "write bit failed");
  }
}

// ---------------------------------------------------------------------------
// search_int() — RMT path (ROM search algorithm)
// ---------------------------------------------------------------------------
uint64_t GPIOOneWireBus::search_int() {
  if (this->last_device_flag_)
    return 0u;

  uint8_t last_zero = 0;
  uint64_t bit_mask = 1;
  uint64_t address = this->address_;

  for (int bit_number = 1; bit_number <= 64; bit_number++, bit_mask <<= 1) {
    bool id_bit = this->read_bit_();
    bool cmp_id_bit = this->read_bit_();

    if (id_bit && cmp_id_bit)
      return 0;  // No devices participating

    bool branch;
    if (id_bit != cmp_id_bit) {
      branch = id_bit;
    } else {
      if (bit_number < this->last_discrepancy_) {
        branch = (address & bit_mask) > 0;
      } else {
        branch = (bit_number == this->last_discrepancy_);
      }
      if (!branch)
        last_zero = bit_number;
    }

    if (branch) {
      address |= bit_mask;
    } else {
      address &= ~bit_mask;
    }

    this->write_bit_(branch);
  }

  this->last_discrepancy_ = last_zero;
  if (this->last_discrepancy_ == 0)
    this->last_device_flag_ = true;

  this->address_ = address;
  return address;
}

}  // namespace esphome::gpio

#endif  // USE_ONE_WIRE_RMT
