#include "gpio_one_wire_rmt.h"

#ifdef USE_ONE_WIRE_RMT

#include <driver/gpio.h>
#include <esp_idf_version.h>
#include <soc/soc_caps.h>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::gpio {

using rmt_one_wire::Symbol;

ESPHOME_LOG_TAG(TAG, "gpio.one_wire");

static constexpr uint32_t RESOLUTION_HZ = 1000000;  // one tick per microsecond, the unit of the codec
static constexpr uint32_t GLITCH_FILTER_NS = 1000;
static constexpr int LINE_RELEASE_TIMEOUT_MS = 5;
static constexpr uint32_t POLL_TIMEOUT_ID = 0;

static constexpr rmt_transmit_config_t TRANSMIT_CONFIG = {.flags = {.eot_level = 1, .queue_nonblocking = 1}};

static_assert(sizeof(Symbol) == sizeof(rmt_symbol_word_t), "Symbol must have the layout of rmt_symbol_word_t");
static_assert(alignof(Symbol) == alignof(rmt_symbol_word_t), "the copy encoder reads Symbol as rmt_symbol_word_t");

GPIORMTOneWireBus::GPIORMTOneWireBus(const rmt_one_wire::Timing &timing)
    : engine_(this, SOC_RMT_MEM_WORDS_PER_CHANNEL, timing) {}

void GPIORMTOneWireBus::setup() {
  const auto gpio = static_cast<gpio_num_t>(this->pin_->get_pin());
  // pin_->setup() is left out, as it could drive the line Low before RMT takes over. So release a hold here
  // that an earlier boot left on the pin, or the pad keeps its old state. Not only with USE_GPIO_HOLD: the
  // firmware before an OTA update may have held this pin. Some output pads have no hold, such as GPIO20 of the
  // ESP32. On those, gpio_hold_dis() returns an error. It is ignored, because there is no hold to release.
  gpio_hold_dis(gpio);

  rmt_copy_encoder_config_t encoder_config{};

  // Also sets the input path, and before ESP-IDF 6.0 the internal pullup of the pin
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
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 2)
  // Older versions start the line Low, until the first transmit below releases it
  tx_config.flags.init_level = 1;
#endif
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
  tx_config.flags.io_od_mode = 1;
#endif
  rmt_tx_event_callbacks_t tx_callbacks{};
  tx_callbacks.on_trans_done = tx_done_isr;

  // The line may stay Low from the TX channel to the first transmit, and a Low above 600 us can reset a device:
  // no delay and no log in this list, unless it fails. 500 us High also let a device finish answering such a Low.
  const Symbol release_line{250, 1, 250, 1};
  const bool created =
      this->check_(rmt_new_copy_encoder(&encoder_config, &this->encoder_), LOG_STR("in rmt_new_copy_encoder")) &&
      this->check_(rmt_new_rx_channel(&rx_config, &this->rx_channel_), LOG_STR("in rmt_new_rx_channel")) &&
      this->check_(rmt_rx_register_event_callbacks(this->rx_channel_, &rx_callbacks, this),
                   LOG_STR("in rmt_rx_register_event_callbacks")) &&
      this->check_(rmt_new_tx_channel(&tx_config, &this->tx_channel_), LOG_STR("in rmt_new_tx_channel")) &&
      this->check_(rmt_tx_register_event_callbacks(this->tx_channel_, &tx_callbacks, this),
                   LOG_STR("in rmt_tx_register_event_callbacks")) &&
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
      this->check_(gpio_od_enable(gpio), LOG_STR("in gpio_od_enable")) &&
#endif
      this->check_(gpio_input_enable(gpio), LOG_STR("in gpio_input_enable"));
#ifdef CONFIG_FREERTOS_USE_TICKLESS_IDLE
  // ESP-IDF gives every pad a sleep setup that turns off its output, its input and its pullup. Like
  // ESP32InternalGPIOPin::setup(), keep this setup in light sleep instead: the open-drain output stays released by
  // the idle level of TX, and a pullup of the pad stays on.
  gpio_sleep_sel_dis(gpio);
#endif
  if (created && this->enable_channels_() &&
      this->check_(
          rmt_transmit(this->tx_channel_, this->encoder_, &release_line, sizeof(release_line), &TRANSMIT_CONFIG),
          LOG_STR("in rmt_transmit")) &&
      this->check_(rmt_tx_wait_all_done(this->tx_channel_, LINE_RELEASE_TIMEOUT_MS),
                   LOG_STR("in rmt_tx_wait_all_done"))) {
    this->search();
    if (!this->is_failed())
      return;
  }
  // check_() marked the bus failed, in the list above or in an abort() during the search. Free what was created, so
  // other RMT components can still get it. A failed engine makes no further port calls.
  this->release_();
}

bool GPIORMTOneWireBus::enable_channels_() {
  // RX first: TX may start right after, and its section needs RX
  if (!this->rx_enabled_) {
    if (!this->check_(rmt_enable(this->rx_channel_), LOG_STR("in rmt_enable")))
      return false;
    this->rx_enabled_ = true;
  }
  if (!this->tx_enabled_) {
    if (!this->check_(rmt_enable(this->tx_channel_), LOG_STR("in rmt_enable")))
      return false;
    this->tx_enabled_ = true;
  }
  return true;
}

void GPIORMTOneWireBus::release_() {
  // A channel must be disabled before it can be deleted. This also stops RX if it is still armed. Errors are
  // ignored, the bus has failed already.
  if (this->tx_enabled_) {
    rmt_disable(this->tx_channel_);
    this->tx_enabled_ = false;
  }
  if (this->rx_enabled_) {
    rmt_disable(this->rx_channel_);
    this->rx_enabled_ = false;
  }
  if (this->tx_channel_ != nullptr) {
    rmt_del_channel(this->tx_channel_);
    this->tx_channel_ = nullptr;
  }
  if (this->rx_channel_ != nullptr) {
    rmt_del_channel(this->rx_channel_);
    this->rx_channel_ = nullptr;
  }
  if (this->encoder_ != nullptr) {
    rmt_del_encoder(this->encoder_);
    this->encoder_ = nullptr;
  }
}

void GPIORMTOneWireBus::dump_config() {
  ESP_LOGCONFIG(TAG, "GPIO 1-wire bus (RMT):");
  LOG_PIN("  Pin: ", this->pin_);
  const rmt_one_wire::Timing &timing = this->engine_.get_timing();
  ESP_LOGCONFIG(TAG,
                "  Timing: write 0 low %u us, slot %u us, read threshold %u us\n"
                "  Parasite power: not supported, the bus never drives a strong pullup",
                timing.write0_low_us, timing.slot_us, timing.read_threshold_us);
  if (timing.read_threshold_us > 15) {
    ESP_LOGW(TAG, "  Read threshold above 15 us: a DS18B20 only keeps a 0 valid for that long");
  }
  this->dump_devices_(TAG);
}

void GPIORMTOneWireBus::loop() {
  // A submit() leaves the channels to this loop pass, which enables them before the engine starts the transaction
  if (!this->engine_.is_idle())
    this->wake_channels_();
  this->engine_.poll();
  this->log_dropped_calls_();
  // mark_failed() took the bus out of the loop, and disable_loop() would undo the failed state. A failed engine
  // ends all queued work itself.
  if (this->is_failed())
    return;
  if (!this->engine_.is_idle()) {
    // Only a completion or loop_interval runs loop() again, and a raised loop_interval can be seconds. So wake the
    // loop when the engine has work without a completion: a section that lost its completion times out, or a
    // transaction waits to start. A timer that fires alone does not run loop(), so it wakes the loop.
    // The deadline stays the same for the whole of a section, so the timeout is only set again when it changes.
    const uint32_t at_ms = this->engine_.next_poll_at_ms();
    if (!this->poll_timeout_set_ || at_ms != this->poll_timeout_at_ms_) {
      this->poll_timeout_set_ = true;
      this->poll_timeout_at_ms_ = at_ms;
      this->set_timeout(POLL_TIMEOUT_ID, this->engine_.next_poll_in_ms(), [this]() {
        this->poll_timeout_set_ = false;
        App.wake_loop_threadsafe();
      });
    }
    return;
  }
  if (this->poll_timeout_set_) {
    this->poll_timeout_set_ = false;
    this->cancel_timeout(POLL_TIMEOUT_ID);
  }
#ifdef CONFIG_PM_ENABLE
  // Not after each blocking call: the calls of one client follow each other in the same loop pass, so they share
  // one enable and one disable
  if (!this->sleep_channels_())
    return;
#endif
  this->disable_loop();
  // disable_loop() logs before it takes the bus out of the loop. A log handler that submits there calls
  // enable_loop() while the loop is still enabled, which does nothing. So enable it again here.
  if (!this->engine_.is_idle())
    this->enable_loop_soon_any_context();
}

void GPIORMTOneWireBus::on_shutdown() {
  // After a restart the pin stays on the RMT output and the line is Low until setup() sends the first High symbol
  gpio_set_direction(static_cast<gpio_num_t>(this->pin_->get_pin()), GPIO_MODE_INPUT);
}

bool GPIORMTOneWireBus::submit(one_wire::Transaction *transaction) {
  if (!this->engine_.submit(transaction))
    return false;
  // A failed engine ended the transaction already, or ends it when the engine call that runs now returns
  if (this->is_failed())
    return true;
  this->enable_loop();
  // From a scheduler callback, loop() would otherwise only run after loop_interval
  App.wake_loop_threadsafe();
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
    ESP_LOGW(TAG, "RMT section did not start, rmt_receive failed: %s", esp_err_to_name(error));
  }
  return error == ESP_OK;
}

bool GPIORMTOneWireBus::start_tx(const Symbol *symbols, size_t count) {
  const esp_err_t error =
      rmt_transmit(this->tx_channel_, this->encoder_, symbols, count * sizeof(Symbol), &TRANSMIT_CONFIG);
  if (error != ESP_OK) {
    ESP_LOGW(TAG, "RMT section did not start, rmt_transmit failed: %s", esp_err_to_name(error));
  }
  return error == ESP_OK;
}

bool GPIORMTOneWireBus::abort() {
  // Disabling a channel stops it, and enabling it again makes it ready for the next section
  if (this->rx_enabled_) {
    if (!this->check_(rmt_disable(this->rx_channel_), LOG_STR("in rmt_disable")))
      return false;
    this->rx_enabled_ = false;
  }
  if (this->tx_enabled_) {
    if (!this->check_(rmt_disable(this->tx_channel_), LOG_STR("in rmt_disable")))
      return false;
    this->tx_enabled_ = false;
  }
  return this->enable_channels_();
}

#ifdef CONFIG_PM_ENABLE
void GPIORMTOneWireBus::wake_channels_() {
  if (this->is_failed() || (this->rx_enabled_ && this->tx_enabled_))
    return;
  if (this->enable_channels_())
    this->enable_loop();
}

bool GPIORMTOneWireBus::sleep_channels_() {
  // The line stays released: rmt_disable() of an idle channel only gives back its lock. It does not change the
  // pin, and TX keeps sending its idle level, which is High from the last section.
  if (this->tx_enabled_) {
    const esp_err_t error = rmt_disable(this->tx_channel_);
    // The driver is still in the done interrupt of the last section. This can only happen when that interrupt runs
    // on the other core.
    if (error == ESP_ERR_INVALID_STATE)
      return false;
    if (!this->check_(error, LOG_STR("in rmt_disable")))
      return false;
    this->tx_enabled_ = false;
  }
  if (this->rx_enabled_) {
    const esp_err_t error = rmt_disable(this->rx_channel_);
    if (error == ESP_ERR_INVALID_STATE)
      return false;
    if (!this->check_(error, LOG_STR("in rmt_disable")))
      return false;
    this->rx_enabled_ = false;
  }
  return true;
}
#endif

void GPIORMTOneWireBus::wait_for_event(uint32_t max_ms) {
  delay(max_ms);
  App.feed_wdt();
}

// The blocking methods log a failed call only after the engine call has returned. A log handler may use the bus,
// and inside an engine call the engine would turn that away. Each failed call logs one warning here. The engine or
// the port may have logged its cause before.

int GPIORMTOneWireBus::reset_int() {
  // Not -1, which reads as "bus held low": mark_failed() already named the cause
  if (this->is_failed() || !this->may_call_engine_())
    return 0;
  this->wake_channels_();
  const int result = this->engine_.reset_blocking();
  this->log_dropped_calls_();
  return result;
}

void GPIORMTOneWireBus::write_(const uint8_t *data, uint8_t num_bits) {
  if (!this->may_call_engine_())
    return;
  this->wake_channels_();
  // On a failed bus, mark_failed() already named the cause
  if (!this->engine_.write_blocking(data, num_bits) && !this->is_failed()) {
    ESP_LOGW(TAG, "Writing %u bits to the bus failed", num_bits);
  }
  this->log_dropped_calls_();
}

uint8_t GPIORMTOneWireBus::read8() {
  uint8_t value = UINT8_MAX;
  this->read_(&value, 8);
  return value;
}

uint64_t GPIORMTOneWireBus::read64() {
  uint64_t value = UINT64_MAX;
  this->read_(reinterpret_cast<uint8_t *>(&value), 64);
  return value;
}

void GPIORMTOneWireBus::read_(uint8_t *data, uint8_t num_bits) {
  if (!this->may_call_engine_())
    return;
  this->wake_channels_();
  if (!this->engine_.read_blocking(data, num_bits) && !this->is_failed()) {
    ESP_LOGW(TAG, "Reading %u bits from the bus failed, they read as 1", num_bits);
  }
  this->log_dropped_calls_();
}

uint64_t GPIORMTOneWireBus::search_int() {
  if (!this->may_call_engine_())
    return 0;
  this->wake_channels_();
  const uint64_t address = this->engine_.search_next();
  // OneWireBus::search() takes the 0 as the end of the search
  if (this->engine_.get_search_missed_devices()) {
    ESP_LOGW(TAG, "Search stopped at bit %u: no device answered, devices may be missing",
             this->engine_.get_search_no_answer_bit());
  } else if (this->engine_.get_search_no_answer_bit() != 0) {
    // A presence pulse without a device in the search, for example from a device that was just unplugged
    ESP_LOGD(TAG, "Search: no device answered");
  }
  this->log_dropped_calls_();
  return address;
}

bool GPIORMTOneWireBus::may_call_engine_() {
  if (!this->engine_.is_busy())
    return true;
  if (this->dropped_calls_ < UINT16_MAX)
    this->dropped_calls_++;
  return false;
}

void GPIORMTOneWireBus::log_dropped_calls_() {
  if (this->dropped_calls_ == 0 || this->engine_.is_busy())
    return;
  const uint16_t count = this->dropped_calls_;
  this->dropped_calls_ = 0;
  ESP_LOGW(TAG, "%u bus calls from a log handler were dropped, the bus was in use", count);
}

}  // namespace esphome::gpio

#endif  // USE_ONE_WIRE_RMT
