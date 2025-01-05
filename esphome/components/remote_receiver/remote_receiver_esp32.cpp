#include "remote_receiver.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace remote_receiver {

static const char *const TAG = "remote_receiver.esp32";
#ifdef USE_ESP32_VARIANT_ESP32H2
static const uint32_t RMT_CLK_FREQ = 32000000;
#else
static const uint32_t RMT_CLK_FREQ = 80000000;
#endif

#if ESP_IDF_VERSION_MAJOR >= 5
static bool IRAM_ATTR HOT rmt_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *event, void *arg) {
  RemoteReceiverComponentStore *store = (RemoteReceiverComponentStore *) arg;
  rmt_rx_done_event_data_t *event_buffer = (rmt_rx_done_event_data_t *) (store->buffer + store->buffer_write);
  uint32_t event_size = sizeof(rmt_rx_done_event_data_t);
  uint32_t next_write = store->buffer_write + event_size + event->num_symbols * sizeof(rmt_symbol_word_t);
  if (next_write + event_size + store->receive_size > store->buffer_size) {
    next_write = 0;
  }
  if (store->buffer_read - next_write < event_size + store->receive_size) {
    next_write = store->buffer_write;
    store->overflow = true;
  }
  if (event->num_symbols <= store->filter_symbols) {
    next_write = store->buffer_write;
  }
  store->error =
      rmt_receive(channel, (uint8_t *) store->buffer + next_write + event_size, store->receive_size, &store->config);
  event_buffer->num_symbols = event->num_symbols;
  event_buffer->received_symbols = event->received_symbols;
  store->buffer_write = next_write;
  return false;
}
#endif

void RemoteReceiverComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Remote Receiver...");
#if ESP_IDF_VERSION_MAJOR >= 5
  rmt_rx_channel_config_t channel;
  memset(&channel, 0, sizeof(channel));
  channel.clk_src = RMT_CLK_SRC_DEFAULT;
  channel.resolution_hz = this->clock_resolution_;
  channel.mem_block_symbols = rmt_symbols_;
  channel.gpio_num = gpio_num_t(this->pin_->get_pin());
  channel.intr_priority = 0;
  channel.flags.invert_in = 0;
  channel.flags.with_dma = this->with_dma_;
  channel.flags.io_loop_back = 0;
  esp_err_t error = rmt_new_rx_channel(&channel, &this->channel_);
  if (error != ESP_OK) {
    this->error_code_ = error;
    if (error == ESP_ERR_NOT_FOUND) {
      this->error_string_ = "out of RMT symbol memory";
    } else {
      this->error_string_ = "in rmt_new_rx_channel";
    }
    this->mark_failed();
    return;
  }
  error = rmt_enable(this->channel_);
  if (error != ESP_OK) {
    this->error_code_ = error;
    this->error_string_ = "in rmt_enable";
    this->mark_failed();
    return;
  }

  rmt_rx_event_callbacks_t callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.on_recv_done = rmt_callback;
  error = rmt_rx_register_event_callbacks(this->channel_, &callbacks, &this->store_);
  if (error != ESP_OK) {
    this->error_code_ = error;
    this->error_string_ = "in rmt_rx_register_event_callbacks";
    this->mark_failed();
    return;
  }

  uint32_t event_size = sizeof(rmt_rx_done_event_data_t);
  uint32_t max_filter_ns = 255u * 1000 / (RMT_CLK_FREQ / 1000000);
  uint32_t max_idle_ns = 65535u * 1000;
  memset(&this->store_.config, 0, sizeof(this->store_.config));
  this->store_.config.signal_range_min_ns = std::min(this->filter_us_ * 1000, max_filter_ns);
  this->store_.config.signal_range_max_ns = std::min(this->idle_us_ * 1000, max_idle_ns);
  this->store_.filter_symbols = this->filter_symbols_;
  this->store_.receive_size = this->receive_symbols_ * sizeof(rmt_symbol_word_t);
  this->store_.buffer_size = std::max((event_size + this->store_.receive_size) * 2, this->buffer_size_);
  this->store_.buffer = new uint8_t[this->buffer_size_];
  error = rmt_receive(this->channel_, (uint8_t *) this->store_.buffer + event_size, this->store_.receive_size,
                      &this->store_.config);
  if (error != ESP_OK) {
    this->error_code_ = error;
    this->error_string_ = "in rmt_receive";
    this->mark_failed();
    return;
  }
#else
  this->pin_->setup();
  rmt_config_t rmt{};
  this->config_rmt(rmt);
  rmt.gpio_num = gpio_num_t(this->pin_->get_pin());
  rmt.rmt_mode = RMT_MODE_RX;
  if (this->filter_us_ == 0) {
    rmt.rx_config.filter_en = false;
  } else {
    rmt.rx_config.filter_en = true;
    rmt.rx_config.filter_ticks_thresh = static_cast<uint8_t>(
        std::min(this->from_microseconds_(this->filter_us_) * this->clock_divider_, (uint32_t) 255));
  }
  rmt.rx_config.idle_threshold =
      static_cast<uint16_t>(std::min(this->from_microseconds_(this->idle_us_), (uint32_t) 65535));

  esp_err_t error = rmt_config(&rmt);
  if (error != ESP_OK) {
    this->error_code_ = error;
    this->error_string_ = "in rmt_config";
    this->mark_failed();
    return;
  }

  error = rmt_driver_install(this->channel_, this->buffer_size_, 0);
  if (error != ESP_OK) {
    this->error_code_ = error;
    if (error == ESP_ERR_INVALID_STATE) {
      this->error_string_ = str_sprintf("RMT channel %i is already in use by another component", this->channel_);
    } else {
      this->error_string_ = "in rmt_driver_install";
    }
    this->mark_failed();
    return;
  }
  error = rmt_get_ringbuf_handle(this->channel_, &this->ringbuf_);
  if (error != ESP_OK) {
    this->error_code_ = error;
    this->error_string_ = "in rmt_get_ringbuf_handle";
    this->mark_failed();
    return;
  }
  error = rmt_rx_start(this->channel_, true);
  if (error != ESP_OK) {
    this->error_code_ = error;
    this->error_string_ = "in rmt_rx_start";
    this->mark_failed();
    return;
  }
#endif
}

void RemoteReceiverComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Remote Receiver:");
  LOG_PIN("  Pin: ", this->pin_);
#if ESP_IDF_VERSION_MAJOR >= 5
  ESP_LOGCONFIG(TAG, "  Clock resolution: %" PRIu32 " hz", this->clock_resolution_);
  ESP_LOGCONFIG(TAG, "  RMT symbols: %" PRIu32, this->rmt_symbols_);
  ESP_LOGCONFIG(TAG, "  Filter symbols: %" PRIu32, this->filter_symbols_);
  ESP_LOGCONFIG(TAG, "  Receive symbols: %" PRIu32, this->receive_symbols_);
#else
  if (this->pin_->digital_read()) {
    ESP_LOGW(TAG, "Remote Receiver Signal starts with a HIGH value. Usually this means you have to "
                  "invert the signal using 'inverted: True' in the pin schema!");
  }
  ESP_LOGCONFIG(TAG, "  Channel: %d", this->channel_);
  ESP_LOGCONFIG(TAG, "  RMT memory blocks: %d", this->mem_block_num_);
  ESP_LOGCONFIG(TAG, "  Clock divider: %u", this->clock_divider_);
#endif
  ESP_LOGCONFIG(TAG, "  Tolerance: %" PRIu32 "%s", this->tolerance_,
                (this->tolerance_mode_ == remote_base::TOLERANCE_MODE_TIME) ? " us" : "%");
  ESP_LOGCONFIG(TAG, "  Filter out pulses shorter than: %" PRIu32 " us", this->filter_us_);
  ESP_LOGCONFIG(TAG, "  Signal is done after %" PRIu32 " us of no changes", this->idle_us_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Configuring RMT driver failed: %s (%s)", esp_err_to_name(this->error_code_),
             this->error_string_.c_str());
  }
}

void RemoteReceiverComponent::loop() {
#if ESP_IDF_VERSION_MAJOR >= 5
  if (this->store_.error != ESP_OK) {
    ESP_LOGE(TAG, "Receive error");
    this->error_code_ = this->store_.error;
    this->error_string_ = "in rmt_callback";
    this->mark_failed();
  }
  if (this->store_.overflow) {
    ESP_LOGW(TAG, "Buffer overflow");
    this->store_.overflow = false;
  }
  uint32_t buffer_write = this->store_.buffer_write;
  while (this->store_.buffer_read != buffer_write) {
    rmt_rx_done_event_data_t *event = (rmt_rx_done_event_data_t *) (this->store_.buffer + this->store_.buffer_read);
    uint32_t event_size = sizeof(rmt_rx_done_event_data_t);
    uint32_t next_read = this->store_.buffer_read + event_size + event->num_symbols * sizeof(rmt_symbol_word_t);
    if (next_read + event_size + this->store_.receive_size > this->store_.buffer_size) {
      next_read = 0;
    }
    this->decode_rmt_(event->received_symbols, event->num_symbols);
    this->store_.buffer_read = next_read;

    if (!this->temp_.empty()) {
      this->temp_.push_back(-this->idle_us_);
      this->call_listeners_dumpers_();
    }
  }
#else
  size_t len = 0;
  auto *item = (rmt_item32_t *) xRingbufferReceive(this->ringbuf_, &len, 0);
  if (item != nullptr) {
    this->decode_rmt_(item, len / sizeof(rmt_item32_t));
    vRingbufferReturnItem(this->ringbuf_, item);

    if (this->temp_.empty())
      return;

    this->temp_.push_back(-this->idle_us_);
    this->call_listeners_dumpers_();
  }
#endif
}

#if ESP_IDF_VERSION_MAJOR >= 5
void RemoteReceiverComponent::decode_rmt_(rmt_symbol_word_t *item, size_t item_count) {
#else
void RemoteReceiverComponent::decode_rmt_(rmt_item32_t *item, size_t item_count) {
#endif
  bool prev_level = false;
  uint32_t prev_length = 0;
  this->temp_.clear();
  int32_t multiplier = this->pin_->is_inverted() ? -1 : 1;
  uint32_t filter_ticks = this->from_microseconds_(this->filter_us_);

  ESP_LOGVV(TAG, "START:");
  for (size_t i = 0; i < item_count; i++) {
    if (item[i].level0) {
      ESP_LOGVV(TAG, "%zu A: ON %" PRIu32 "us (%u ticks)", i, this->to_microseconds_(item[i].duration0),
                item[i].duration0);
    } else {
      ESP_LOGVV(TAG, "%zu A: OFF %" PRIu32 "us (%u ticks)", i, this->to_microseconds_(item[i].duration0),
                item[i].duration0);
    }
    if (item[i].level1) {
      ESP_LOGVV(TAG, "%zu B: ON %" PRIu32 "us (%u ticks)", i, this->to_microseconds_(item[i].duration1),
                item[i].duration1);
    } else {
      ESP_LOGVV(TAG, "%zu B: OFF %" PRIu32 "us (%u ticks)", i, this->to_microseconds_(item[i].duration1),
                item[i].duration1);
    }
  }
  ESP_LOGVV(TAG, "\n");

  this->temp_.reserve(item_count * 2);  // each RMT item has 2 pulses
  for (size_t i = 0; i < item_count; i++) {
    if (item[i].duration0 == 0u) {
      // EOF, sometimes garbage follows, break early
      break;
    } else if ((bool(item[i].level0) == prev_level) || (item[i].duration0 < filter_ticks)) {
      prev_length += item[i].duration0;
    } else {
      if (prev_length > 0) {
        if (prev_level) {
          this->temp_.push_back(this->to_microseconds_(prev_length) * multiplier);
        } else {
          this->temp_.push_back(-int32_t(this->to_microseconds_(prev_length)) * multiplier);
        }
      }
      prev_level = bool(item[i].level0);
      prev_length = item[i].duration0;
    }

    if (item[i].duration1 == 0u) {
      // EOF, sometimes garbage follows, break early
      break;
    } else if ((bool(item[i].level1) == prev_level) || (item[i].duration1 < filter_ticks)) {
      prev_length += item[i].duration1;
    } else {
      if (prev_length > 0) {
        if (prev_level) {
          this->temp_.push_back(this->to_microseconds_(prev_length) * multiplier);
        } else {
          this->temp_.push_back(-int32_t(this->to_microseconds_(prev_length)) * multiplier);
        }
      }
      prev_level = bool(item[i].level1);
      prev_length = item[i].duration1;
    }
  }
  if (prev_length > 0) {
    if (prev_level) {
      this->temp_.push_back(this->to_microseconds_(prev_length) * multiplier);
    } else {
      this->temp_.push_back(-int32_t(this->to_microseconds_(prev_length)) * multiplier);
    }
  }
}

}  // namespace remote_receiver
}  // namespace esphome

#endif
