#include "gpio_one_wire_rmt.h"

#ifdef USE_ONE_WIRE_RMT

#include <driver/gpio.h>
#include <esp_idf_version.h>
#include <soc/soc_caps.h>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::gpio {

using rmt_one_wire::Engine;
using rmt_one_wire::Symbol;

ESPHOME_LOG_TAG(TAG, "gpio.one_wire");

static constexpr uint32_t RESOLUTION_HZ = 1000000;  // one tick per microsecond, the unit of the codec
static constexpr uint32_t GLITCH_FILTER_NS = 1000;
static constexpr int LINE_RELEASE_TIMEOUT_MS = 5;

static const rmt_transmit_config_t TRANSMIT_CONFIG = {.flags = {.eot_level = 1, .queue_nonblocking = 1}};

static_assert(sizeof(Symbol) == sizeof(rmt_symbol_word_t), "Symbol must have the layout of rmt_symbol_word_t");
static_assert(alignof(Symbol) == alignof(rmt_symbol_word_t), "the copy encoder reads Symbol as rmt_symbol_word_t");
static_assert(SOC_RMT_MEM_WORDS_PER_CHANNEL <= Engine::MAX_MEM_WORDS, "the buffers of Engine are too small");

GPIORMTOneWireBus::GPIORMTOneWireBus(const rmt_one_wire::Timing &timing)
    : timing_(timing), engine_(this, SOC_RMT_MEM_WORDS_PER_CHANNEL, timing) {}

void GPIORMTOneWireBus::setup() {
  const auto gpio = static_cast<gpio_num_t>(this->pin_->get_pin());

  rmt_copy_encoder_config_t encoder_config{};

  // Also sets the input path and the pullup of the pin
  rmt_rx_channel_config_t rx_config{};
  rx_config.gpio_num = gpio;
  rx_config.clk_src = RMT_CLK_SRC_DEFAULT;
  rx_config.resolution_hz = RESOLUTION_HZ;
  rx_config.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL;
  rmt_rx_event_callbacks_t rx_callbacks{};
  rx_callbacks.on_recv_done = rx_done_isr;

  rmt_tx_channel_config_t tx_config{};
  tx_config.gpio_num = gpio;
  tx_config.clk_src = RMT_CLK_SRC_DEFAULT;
  tx_config.resolution_hz = RESOLUTION_HZ;
  tx_config.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL;
  tx_config.trans_queue_depth = 1;
  tx_config.flags.init_level = 1;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
  tx_config.flags.io_od_mode = 1;
#endif
  rmt_tx_event_callbacks_t tx_callbacks{};
  tx_callbacks.on_trans_done = tx_done_isr;

  // The line may stay Low from the TX channel to the first transmit, and a Low above 600 us can reset a device:
  // no delay and no log in this list, unless it fails. 500 us High also let a device finish answering such a Low.
  const Symbol release_line{250, 1, 250, 1};
  if (this->check_(rmt_new_copy_encoder(&encoder_config, &this->encoder_), LOG_STR("in rmt_new_copy_encoder")) &&
      this->check_(rmt_new_rx_channel(&rx_config, &this->rx_channel_), LOG_STR("in rmt_new_rx_channel")) &&
      this->check_(rmt_rx_register_event_callbacks(this->rx_channel_, &rx_callbacks, this),
                   LOG_STR("in rmt_rx_register_event_callbacks")) &&
      this->check_(rmt_new_tx_channel(&tx_config, &this->tx_channel_), LOG_STR("in rmt_new_tx_channel")) &&
      this->check_(rmt_tx_register_event_callbacks(this->tx_channel_, &tx_callbacks, this),
                   LOG_STR("in rmt_tx_register_event_callbacks")) &&
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
      this->check_(gpio_od_enable(gpio), LOG_STR("in gpio_od_enable")) &&
#endif
      this->check_(gpio_input_enable(gpio), LOG_STR("in gpio_input_enable")) &&
      this->check_(rmt_enable(this->rx_channel_), LOG_STR("in rmt_enable")) &&
      this->check_(rmt_enable(this->tx_channel_), LOG_STR("in rmt_enable")) &&
      this->check_(
          rmt_transmit(this->tx_channel_, this->encoder_, &release_line, sizeof(release_line), &TRANSMIT_CONFIG),
          LOG_STR("in rmt_transmit")) &&
      this->check_(rmt_tx_wait_all_done(this->tx_channel_, LINE_RELEASE_TIMEOUT_MS),
                   LOG_STR("in rmt_tx_wait_all_done"))) {
    this->search();
  }
}

void GPIORMTOneWireBus::dump_config() {
  ESP_LOGCONFIG(TAG, "GPIO 1-wire bus (RMT):");
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG, "  Timing: write 0 low %u us, slot %u us, read threshold %u us", this->timing_.write0_low_us,
                this->timing_.slot_us, this->timing_.read_threshold_us);
  if (this->timing_.read_threshold_us > 15) {
    ESP_LOGW(TAG, "  Read threshold above 15 us: a DS18B20 only keeps a 0 valid for that long");
  }
  this->dump_devices_(TAG);
}

void GPIORMTOneWireBus::loop() {
  // mark_failed() took the bus out of the loop: end all that is queued now. disable_loop() would undo the failed state.
  do {
    this->engine_.poll();
  } while (this->is_failed() && !this->engine_.is_idle());
  if (this->engine_.is_idle() && !this->is_failed())
    this->disable_loop();
}

void GPIORMTOneWireBus::on_shutdown() {
  // After a restart the pin stays on the RMT output and the line is Low until setup() sends the first High symbol
  gpio_set_direction(static_cast<gpio_num_t>(this->pin_->get_pin()), GPIO_MODE_INPUT);
}

bool GPIORMTOneWireBus::submit(one_wire::Transaction *transaction) {
  if (!this->engine_.submit(transaction))
    return false;
  this->enable_loop();  // Does nothing on a failed bus, which has to run loop() itself
  if (this->is_failed())
    this->loop();
  return true;
}

bool GPIORMTOneWireBus::check_(esp_err_t error, const LogString *step) {
  if (error == ESP_OK)
    return true;
  ESP_LOGE(TAG, "RMT driver failed: %s", esp_err_to_name(error));
  this->engine_.set_failed();
  const auto *no_memory = LOG_STR("no free RMT channel or symbol memory, lower rmt_symbols of other RMT components");
  this->mark_failed(error == ESP_ERR_NOT_FOUND ? no_memory : step);
  return false;
}

bool IRAM_ATTR GPIORMTOneWireBus::tx_done_isr(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *data,
                                              void *context) {
  auto *bus = static_cast<GPIORMTOneWireBus *>(context);
  if (bus->engine_.on_tx_done())
    bus->enable_loop_soon_any_context();
  return false;
}

bool IRAM_ATTR GPIORMTOneWireBus::rx_done_isr(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *data,
                                              void *context) {
  auto *bus = static_cast<GPIORMTOneWireBus *>(context);
  if (bus->engine_.on_rx_done(data->num_symbols))
    bus->enable_loop_soon_any_context();
  return false;
}

bool GPIORMTOneWireBus::arm_rx(Symbol *buffer, size_t capacity, uint32_t idle_us) {
  rmt_receive_config_t config{};
  config.signal_range_min_ns = GLITCH_FILTER_NS;
  config.signal_range_max_ns = idle_us * 1000;
  const esp_err_t error = rmt_receive(this->rx_channel_, buffer, capacity * sizeof(Symbol), &config);
  if (error != ESP_OK) {
    ESP_LOGW(TAG, "rmt_receive failed: %s", esp_err_to_name(error));
  }
  return error == ESP_OK;
}

bool GPIORMTOneWireBus::start_tx(const Symbol *symbols, size_t count) {
  const esp_err_t error =
      rmt_transmit(this->tx_channel_, this->encoder_, symbols, count * sizeof(Symbol), &TRANSMIT_CONFIG);
  if (error != ESP_OK) {
    ESP_LOGW(TAG, "rmt_transmit failed: %s", esp_err_to_name(error));
  }
  return error == ESP_OK;
}

bool GPIORMTOneWireBus::abort() {
  return this->check_(rmt_disable(this->rx_channel_), LOG_STR("in rmt_disable")) &&
         this->check_(rmt_disable(this->tx_channel_), LOG_STR("in rmt_disable")) &&
         this->check_(rmt_enable(this->rx_channel_), LOG_STR("in rmt_enable")) &&
         this->check_(rmt_enable(this->tx_channel_), LOG_STR("in rmt_enable"));
}

void GPIORMTOneWireBus::wait_for_event(uint32_t max_ms) {
  delay(max_ms);
  App.feed_wdt();
}

uint8_t GPIORMTOneWireBus::read8() {
  uint8_t value = 0;
  this->engine_.read_blocking(&value, 8);
  return value;
}

uint64_t GPIORMTOneWireBus::read64() {
  uint64_t value = 0;
  this->engine_.read_blocking(reinterpret_cast<uint8_t *>(&value), 64);
  return value;
}

}  // namespace esphome::gpio

#endif  // USE_ONE_WIRE_RMT
