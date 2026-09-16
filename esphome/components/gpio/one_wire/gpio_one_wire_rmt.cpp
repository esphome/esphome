#include "gpio_one_wire.h"

#ifdef USE_ONE_WIRE_RMT

#include <cstring>
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>

#include "esphome/core/log.h"

namespace esphome::gpio {

static const char *const TAG = "gpio.one_wire";

// 1 MHz resolution gives 1 us per RMT tick.
static constexpr uint32_t RMT_RESOLUTION_HZ = 1000000;
static constexpr size_t MAX_RX_SYMBOLS = 64;
// OneWireBus operations are synchronous, so unlike output-only RMT users the
// caller cannot defer completion to a later loop iteration. Normal 64-bit
// transactions finish in under 5 ms; this is only a fault timeout ceiling.
static constexpr uint32_t RMT_OPERATION_TIMEOUT_MS = 20;

// 1-wire timing constants, in microseconds.
static constexpr uint32_t RESET_PULSE_DURATION = 500;
// Keep the recovery/presence window at the 1-wire minimum tRSTH. The current
// Espressif RMT 1-wire implementation also uses 480 us here.
static constexpr uint32_t RESET_WAIT_DURATION = 480;
static constexpr uint32_t RESET_PRESENCE_WAIT_MIN = 15;
static constexpr uint32_t RESET_PRESENCE_DURATION_MIN = 60;
static constexpr uint32_t SLOT_START = 2;
static constexpr uint32_t SLOT_BIT = 60;
static constexpr uint32_t SLOT_RECOVERY = 5;
static constexpr uint32_t SLOT_SAMPLE_TIME = 15;

static inline rmt_symbol_word_t make_symbol(uint16_t dur0, uint8_t lvl0, uint16_t dur1, uint8_t lvl1) {
  rmt_symbol_word_t symbol{};
  symbol.duration0 = dur0;
  symbol.level0 = lvl0;
  symbol.duration1 = dur1;
  symbol.level1 = lvl1;
  return symbol;
}

// These are immutable configuration values and can be shared by all bus
// instances. All mutable RMT resources are members of GPIOOneWireBus.
static const rmt_transmit_config_t TX_CONFIG = {
    .loop_count = 0,
    .flags = {.eot_level = 1},
};

static const rmt_receive_config_t RX_CONFIG = {
    .signal_range_min_ns = 1000000000 / RMT_RESOLUTION_HZ,
    .signal_range_max_ns = (RESET_PULSE_DURATION + RESET_WAIT_DURATION) * 1000,
};

static bool IRAM_ATTR rx_done_cb(rmt_channel_handle_t /*channel*/, const rmt_rx_done_event_data_t *edata,
                                 void *user_data) {
  BaseType_t task_woken = pdFALSE;
  xQueueSendFromISR(static_cast<QueueHandle_t>(user_data), edata, &task_woken);
  return task_woken;
}

void GPIOOneWireBus::fail_rmt_(esp_err_t error, const LogString *reason) {
  ESP_LOGE(TAG, "RMT driver failed: %s", esp_err_to_name(error));
  this->destroy_rmt_();
  this->mark_failed(reason);
}

void GPIOOneWireBus::destroy_rmt_() {
  if (this->tx_bytes_encoder_ != nullptr) {
    rmt_del_encoder(this->tx_bytes_encoder_);
    this->tx_bytes_encoder_ = nullptr;
  }
  if (this->tx_copy_encoder_ != nullptr) {
    rmt_del_encoder(this->tx_copy_encoder_);
    this->tx_copy_encoder_ = nullptr;
  }
  if (this->rx_channel_ != nullptr) {
    rmt_disable(this->rx_channel_);
    rmt_del_channel(this->rx_channel_);
    this->rx_channel_ = nullptr;
  }
  if (this->tx_channel_ != nullptr) {
    rmt_disable(this->tx_channel_);
    rmt_del_channel(this->tx_channel_);
    this->tx_channel_ = nullptr;
  }
  if (this->receive_queue_ != nullptr) {
    vQueueDelete(this->receive_queue_);
    this->receive_queue_ = nullptr;
  }
  if (this->rx_symbols_buf_ != nullptr) {
    heap_caps_free(this->rx_symbols_buf_);
    this->rx_symbols_buf_ = nullptr;
  }
}

void GPIOOneWireBus::setup_rmt_() {
  rmt_bytes_encoder_config_t bytes_enc_cfg{};
  bytes_enc_cfg.bit0 = make_symbol(SLOT_START + SLOT_BIT, 0, SLOT_RECOVERY, 1);
  bytes_enc_cfg.bit1 = make_symbol(SLOT_START, 0, SLOT_BIT + SLOT_RECOVERY, 1);
  bytes_enc_cfg.flags.msb_first = 0;
  esp_err_t error = rmt_new_bytes_encoder(&bytes_enc_cfg, &this->tx_bytes_encoder_);
  if (error != ESP_OK) {
    this->fail_rmt_(error, LOG_STR("Failed to create RMT bytes encoder"));
    return;
  }

  rmt_copy_encoder_config_t copy_enc_cfg{};
  error = rmt_new_copy_encoder(&copy_enc_cfg, &this->tx_copy_encoder_);
  if (error != ESP_OK) {
    this->fail_rmt_(error, LOG_STR("Failed to create RMT copy encoder"));
    return;
  }

  this->receive_queue_ = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
  if (this->receive_queue_ == nullptr) {
    this->destroy_rmt_();
    this->mark_failed(LOG_STR("Failed to create RMT receive queue"));
    return;
  }

  this->rx_symbols_buf_ = static_cast<rmt_symbol_word_t *>(
      heap_caps_malloc(MAX_RX_SYMBOLS * sizeof(rmt_symbol_word_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (this->rx_symbols_buf_ == nullptr) {
    this->destroy_rmt_();
    this->mark_failed(LOG_STR("Failed to allocate RMT receive buffer"));
    return;
  }

  auto gpio_num = static_cast<gpio_num_t>(this->t_pin_->get_pin());

  // Create RX before TX. The TX channel is then attached to the same GPIO and
  // configured as open-drain so the device can pull the 1-wire bus low.
  rmt_rx_channel_config_t rx_cfg{};
  rx_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
  rx_cfg.resolution_hz = RMT_RESOLUTION_HZ;
  rx_cfg.gpio_num = gpio_num;
  rx_cfg.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL;
  error = rmt_new_rx_channel(&rx_cfg, &this->rx_channel_);
  if (error != ESP_OK) {
    this->fail_rmt_(error, LOG_STR("Failed to create RMT RX channel"));
    return;
  }

  rmt_rx_event_callbacks_t callbacks{.on_recv_done = rx_done_cb};
  error = rmt_rx_register_event_callbacks(this->rx_channel_, &callbacks, this->receive_queue_);
  if (error != ESP_OK) {
    this->fail_rmt_(error, LOG_STR("Failed to register RMT RX callback"));
    return;
  }

  rmt_tx_channel_config_t tx_cfg{};
  tx_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
  tx_cfg.resolution_hz = RMT_RESOLUTION_HZ;
  tx_cfg.gpio_num = gpio_num;
  tx_cfg.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL;
  // A read completes from the RX side, which can happen just before the TX
  // channel has released its final recovery-high symbol. Keep the same queue
  // depth as Espressif's current RMT 1-wire driver so back-to-back search/read
  // slots on a multidrop bus can be accepted without a transient busy error.
  tx_cfg.trans_queue_depth = 4;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
  tx_cfg.flags.io_loop_back = true;
  tx_cfg.flags.io_od_mode = true;
#endif
  error = rmt_new_tx_channel(&tx_cfg, &this->tx_channel_);
  if (error != ESP_OK) {
    this->fail_rmt_(error, LOG_STR("Failed to create RMT TX channel"));
    return;
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
  // IDF 6 removed the RMT loop-back/open-drain channel flags. Configure the
  // GPIO explicitly, matching the current ESPHome RMT transmitter path.
  gpio_od_enable(gpio_num);
  gpio_input_enable(gpio_num);
#endif

  gpio_set_pull_mode(gpio_num, GPIO_PULLUP_ONLY);

  error = rmt_enable(this->rx_channel_);
  if (error != ESP_OK) {
    this->fail_rmt_(error, LOG_STR("Failed to enable RMT RX channel"));
    return;
  }
  error = rmt_enable(this->tx_channel_);
  if (error != ESP_OK) {
    this->fail_rmt_(error, LOG_STR("Failed to enable RMT TX channel"));
    return;
  }

  // Leave the bus released (high) before the first reset/search cycle.
  rmt_symbol_word_t release = make_symbol(1, 1, 0, 1);
  error = rmt_transmit(this->tx_channel_, this->tx_copy_encoder_, &release, sizeof(release), &TX_CONFIG);
  if (error != ESP_OK) {
    this->fail_rmt_(error, LOG_STR("Failed to release 1-wire bus"));
    return;
  }
  error = rmt_tx_wait_all_done(this->tx_channel_, RMT_OPERATION_TIMEOUT_MS);
  if (error != ESP_OK) {
    this->fail_rmt_(error, LOG_STR("Failed to release 1-wire bus"));
    return;
  }

  this->search();
}

int GPIOOneWireBus::reset_rmt_() {
  rmt_symbol_word_t reset_symbol = make_symbol(RESET_PULSE_DURATION, 0, RESET_WAIT_DURATION, 1);

  xQueueReset(this->receive_queue_);
  if (rmt_receive(this->rx_channel_, this->rx_symbols_buf_, 2 * sizeof(rmt_symbol_word_t), &RX_CONFIG) != ESP_OK)
    return -1;
  if (rmt_transmit(this->tx_channel_, this->tx_copy_encoder_, &reset_symbol, sizeof(reset_symbol), &TX_CONFIG) !=
      ESP_OK)
    return -1;

  rmt_rx_done_event_data_t rx_data;
  if (xQueueReceive(this->receive_queue_, &rx_data, pdMS_TO_TICKS(RMT_OPERATION_TIMEOUT_MS)) != pdPASS)
    return -1;

  if (rx_data.num_symbols < 2)
    return 0;

  const rmt_symbol_word_t *symbols = rx_data.received_symbols;
  bool present;
  if (symbols[0].level1 == 1) {
    present = symbols[0].duration1 > RESET_PRESENCE_WAIT_MIN && symbols[1].duration0 > RESET_PRESENCE_DURATION_MIN;
  } else {
    present = symbols[0].duration0 > RESET_PRESENCE_WAIT_MIN && symbols[1].duration1 > RESET_PRESENCE_DURATION_MIN;
  }
  return present ? 1 : 0;
}

void GPIOOneWireBus::write8_rmt_(uint8_t val) {
  if (rmt_transmit(this->tx_channel_, this->tx_bytes_encoder_, &val, sizeof(val), &TX_CONFIG) != ESP_OK ||
      rmt_tx_wait_all_done(this->tx_channel_, RMT_OPERATION_TIMEOUT_MS) != ESP_OK) {
    ESP_LOGE(TAG, "RMT write8 failed");
  }
}

void GPIOOneWireBus::write64_rmt_(uint64_t val) {
  if (rmt_transmit(this->tx_channel_, this->tx_bytes_encoder_, &val, sizeof(val), &TX_CONFIG) != ESP_OK ||
      rmt_tx_wait_all_done(this->tx_channel_, RMT_OPERATION_TIMEOUT_MS) != ESP_OK) {
    ESP_LOGE(TAG, "RMT write64 failed");
  }
}

uint8_t GPIOOneWireBus::read8_rmt_() {
  uint8_t tx_buf = 0xFF;
  uint8_t result = 0;

  xQueueReset(this->receive_queue_);
  if (rmt_receive(this->rx_channel_, this->rx_symbols_buf_, 8 * sizeof(rmt_symbol_word_t), &RX_CONFIG) != ESP_OK)
    return 0;
  if (rmt_transmit(this->tx_channel_, this->tx_bytes_encoder_, &tx_buf, sizeof(tx_buf), &TX_CONFIG) != ESP_OK)
    return 0;

  rmt_rx_done_event_data_t rx_data;
  if (xQueueReceive(this->receive_queue_, &rx_data, pdMS_TO_TICKS(RMT_OPERATION_TIMEOUT_MS)) != pdPASS) {
    ESP_LOGE(TAG, "RMT read8 timeout");
    return 0;
  }

  for (size_t i = 0; i < rx_data.num_symbols && i < 8; i++) {
    if (rx_data.received_symbols[i].duration0 <= SLOT_SAMPLE_TIME)
      result |= (1u << i);
  }
  return result;
}

uint64_t GPIOOneWireBus::read64_rmt_() {
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
  if (xQueueReceive(this->receive_queue_, &rx_data, pdMS_TO_TICKS(RMT_OPERATION_TIMEOUT_MS)) != pdPASS) {
    ESP_LOGE(TAG, "RMT read64 timeout");
    return 0;
  }

  for (size_t i = 0; i < rx_data.num_symbols && i < 64; i++) {
    if (rx_data.received_symbols[i].duration0 <= SLOT_SAMPLE_TIME)
      result |= (uint64_t(1) << i);
  }
  return result;
}

bool GPIOOneWireBus::read_bit_rmt_() {
  rmt_symbol_word_t read_symbol = make_symbol(SLOT_START, 0, SLOT_BIT + SLOT_RECOVERY, 1);

  xQueueReset(this->receive_queue_);
  if (rmt_receive(this->rx_channel_, this->rx_symbols_buf_, sizeof(rmt_symbol_word_t), &RX_CONFIG) != ESP_OK)
    return false;
  if (rmt_transmit(this->tx_channel_, this->tx_copy_encoder_, &read_symbol, sizeof(read_symbol), &TX_CONFIG) != ESP_OK)
    return false;

  rmt_rx_done_event_data_t rx_data;
  if (xQueueReceive(this->receive_queue_, &rx_data, pdMS_TO_TICKS(RMT_OPERATION_TIMEOUT_MS)) != pdPASS)
    return false;
  if (rx_data.num_symbols == 0)
    return false;

  return rx_data.received_symbols[0].duration0 <= SLOT_SAMPLE_TIME;
}

void GPIOOneWireBus::write_bit_rmt_(bool bit) {
  rmt_symbol_word_t symbol = bit ? make_symbol(SLOT_START, 0, SLOT_BIT + SLOT_RECOVERY, 1)
                                 : make_symbol(SLOT_START + SLOT_BIT, 0, SLOT_RECOVERY, 1);
  if (rmt_transmit(this->tx_channel_, this->tx_copy_encoder_, &symbol, sizeof(symbol), &TX_CONFIG) != ESP_OK ||
      rmt_tx_wait_all_done(this->tx_channel_, RMT_OPERATION_TIMEOUT_MS) != ESP_OK) {
    ESP_LOGE(TAG, "RMT write bit failed");
  }
}

uint64_t GPIOOneWireBus::search_rmt_() {
  if (this->last_device_flag_)
    return 0u;

  uint8_t last_zero = 0;
  uint64_t bit_mask = 1;
  uint64_t address = this->address_;

  for (int bit_number = 1; bit_number <= 64; bit_number++, bit_mask <<= 1) {
    bool id_bit = this->read_bit_rmt_();
    bool cmp_id_bit = this->read_bit_rmt_();

    if (id_bit && cmp_id_bit)
      return 0;

    bool branch;
    if (id_bit != cmp_id_bit) {
      branch = id_bit;
    } else {
      if (bit_number < this->last_discrepancy_) {
        branch = (address & bit_mask) > 0;
      } else {
        branch = bit_number == this->last_discrepancy_;
      }
      if (!branch)
        last_zero = bit_number;
    }

    if (branch) {
      address |= bit_mask;
    } else {
      address &= ~bit_mask;
    }

    this->write_bit_rmt_(branch);
  }

  this->last_discrepancy_ = last_zero;
  if (this->last_discrepancy_ == 0)
    this->last_device_flag_ = true;

  this->address_ = address;
  return address;
}

}  // namespace esphome::gpio

#endif  // USE_ONE_WIRE_RMT
