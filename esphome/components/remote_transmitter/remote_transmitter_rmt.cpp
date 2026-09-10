#include "remote_transmitter.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32
#include <soc/soc_caps.h>
#if SOC_RMT_SUPPORTED
#include <driver/gpio.h>

namespace esphome::remote_transmitter {

static const char *const TAG = "remote_transmitter";

// Maximum RMT symbol duration (15-bit field)
static constexpr uint32_t RMT_SYMBOL_DURATION_MAX = 0x7FFF;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 1)
// How long a blocking wait sleeps between watchdog feeds
static constexpr int RMT_WAIT_SLICE_MS = 50;

static size_t IRAM_ATTR HOT encoder_callback(const void *data, size_t size, size_t written, size_t free,
                                             rmt_symbol_word_t *symbols, bool *done, void *arg) {
  auto *store = static_cast<RemoteTransmitterComponentStore *>(arg);
  const auto *encoded = static_cast<const rmt_symbol_half_t *>(data);
  size_t length = size / sizeof(rmt_symbol_half_t);
  size_t count = 0;

  // copy symbols
  for (size_t i = 0; i < free; i++) {
    uint16_t sym_0 = encoded[store->index++].val;
    if (store->index >= length) {
      store->index = 0;
      store->times--;
      if (store->times == 0) {
        *done = true;
        symbols[count++].val = sym_0;
        return count;
      }
    }
    uint16_t sym_1 = encoded[store->index++].val;
    if (store->index >= length) {
      store->index = 0;
      store->times--;
      if (store->times == 0) {
        *done = true;
        symbols[count++].val = sym_0 | (sym_1 << 16);
        return count;
      }
    }
    symbols[count++].val = sym_0 | (sym_1 << 16);
  }
  *done = false;
  return count;
}

// Splits a duration into 15-bit symbols; with out == nullptr only counts them
static size_t write_symbols(rmt_symbol_half_t *out, size_t pos, uint32_t ticks, bool level) {
  size_t count = 0;
  while (ticks > 0) {
    uint32_t duration = std::min(ticks, RMT_SYMBOL_DURATION_MAX);
    if (out != nullptr) {
      out[pos + count] = {
          .duration = static_cast<uint16_t>(duration),
          .level = static_cast<uint16_t>(level),
      };
    }
    ticks -= duration;
    count++;
  }
  return count;
}

static RmtFrame *new_frame(uint32_t capacity) {
  // malloc rather than new: with C++ exceptions disabled, new aborts instead of returning null
  auto *frame = static_cast<RmtFrame *>(malloc(sizeof(RmtFrame) + capacity * sizeof(rmt_symbol_half_t)));  // NOLINT
  if (frame != nullptr)
    frame->capacity = capacity;
  return frame;
}

bool IRAM_ATTR HOT RemoteTransmitterComponent::tx_done_callback(rmt_channel_handle_t channel,
                                                                const rmt_tx_done_event_data_t *event, void *arg) {
  auto *self = static_cast<RemoteTransmitterComponent *>(arg);
  self->done_count_++;
  self->enable_loop_soon_any_context();
  return false;
}
#endif

void RemoteTransmitterComponent::setup() {
  this->inverted_ = this->pin_->is_inverted();
  this->configure_rmt_();
}

void RemoteTransmitterComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Remote Transmitter:");
  ESP_LOGCONFIG(TAG,
                "  Clock resolution: %" PRIu32 " hz\n"
                "  RMT symbols: %" PRIu32 "\n"
                "  Queue depth: %u\n"
                "  Max pending: %u",
                this->clock_resolution_, this->rmt_symbols_, this->queue_depth_, this->max_pending_);
  LOG_PIN("  Pin: ", this->pin_);

  if (this->current_carrier_frequency_ != 0 && this->carrier_duty_percent_ != 100) {
    ESP_LOGCONFIG(TAG, "    Carrier Duty: %u%%", this->carrier_duty_percent_);
  }

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Configuring RMT driver failed: %s (%s)", esp_err_to_name(this->error_code_),
             this->error_string_.c_str());
  }
}

void RemoteTransmitterComponent::digital_write(bool value) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 1)
  rmt_symbol_half_t symbol = {
      .duration = 1,
      .level = value,
  };
  rmt_transmit_config_t config;
  memset(&config, 0, sizeof(config));
  config.flags.eot_level = value;
  config.flags.queue_nonblocking = 1;
  // everything queued must go out first; slot 0 is then free
  this->wait_for_rmt_();
  RmtTxSlot &slot = this->slots_[0];
  slot.store.times = 1;
  slot.store.index = 0;
  rmt_encoder_handle_t encoder = slot.encoder;
#else
  rmt_symbol_word_t symbol = {
      .duration0 = 1,
      .level0 = value,
      .duration1 = 0,
      .level1 = value,
  };
  rmt_transmit_config_t config;
  memset(&config, 0, sizeof(config));
  config.flags.eot_level = value;
  rmt_encoder_handle_t encoder = this->encoder_;
#endif
  esp_err_t error = rmt_transmit(this->channel_, encoder, &symbol, sizeof(symbol), &config);
  if (error != ESP_OK) {
    ESP_LOGW(TAG, "rmt_transmit failed: %s", esp_err_to_name(error));
    this->status_set_warning();
  }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 1)
  if (error == ESP_OK)
    this->submitted_++;
  this->wait_all_done_();
  // a level write is not a frame, so its completion is not reported
  this->delivered_ = this->submitted_;
#else
  error = rmt_tx_wait_all_done(this->channel_, -1);
  if (error != ESP_OK) {
    ESP_LOGW(TAG, "rmt_tx_wait_all_done failed: %s", esp_err_to_name(error));
    this->status_set_warning();
  }
#endif
}

void RemoteTransmitterComponent::configure_rmt_() {
  esp_err_t error;

  if (!this->initialized_) {
    bool open_drain = (this->pin_->get_flags() & gpio::FLAG_OPEN_DRAIN) != 0;
    rmt_tx_channel_config_t channel;
    memset(&channel, 0, sizeof(channel));
    channel.clk_src = RMT_CLK_SRC_DEFAULT;
    channel.resolution_hz = this->clock_resolution_;
    channel.gpio_num = gpio_num_t(this->pin_->get_pin());
    channel.mem_block_symbols = this->rmt_symbols_;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 1)
    channel.trans_queue_depth = this->queue_depth_;
#else
    channel.trans_queue_depth = 1;
#endif
    channel.flags.invert_out = 0;
    channel.flags.with_dma = this->with_dma_;
    channel.intr_priority = 0;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
    channel.flags.io_loop_back = open_drain;
    channel.flags.io_od_mode = open_drain;
#endif
    error = rmt_new_tx_channel(&channel, &this->channel_);
    if (error != ESP_OK) {
      this->error_code_ = error;
      if (error == ESP_ERR_NOT_FOUND) {
        this->error_string_ = "out of RMT symbol memory";
      } else {
        this->error_string_ = "in rmt_new_tx_channel";
      }
      this->mark_failed();
      return;
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    if (open_drain) {
      gpio_num_t gpio = gpio_num_t(this->pin_->get_pin());
      gpio_od_enable(gpio);
      gpio_input_enable(gpio);
    }
#endif
    if (this->pin_->get_flags() & gpio::FLAG_PULLUP) {
      gpio_pullup_en(gpio_num_t(this->pin_->get_pin()));
    } else {
      gpio_pullup_dis(gpio_num_t(this->pin_->get_pin()));
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 1)
    rmt_tx_event_callbacks_t callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.on_trans_done = tx_done_callback;
    error = rmt_tx_register_event_callbacks(this->channel_, &callbacks, this);
    if (error != ESP_OK) {
      this->error_code_ = error;
      this->error_string_ = "in rmt_tx_register_event_callbacks";
      this->mark_failed();
      return;
    }

    // one encoder per slot: its state is read from the interrupt while the slot's frame is in flight
    this->slots_.init(this->queue_depth_);
    for (uint8_t i = 0; i < this->queue_depth_; i++) {
      RmtTxSlot &slot = this->slots_.emplace_back();
      rmt_simple_encoder_config_t encoder;
      memset(&encoder, 0, sizeof(encoder));
      encoder.callback = encoder_callback;
      encoder.arg = &slot.store;
      encoder.min_chunk_size = 1;
      error = rmt_new_simple_encoder(&encoder, &slot.encoder);
      if (error != ESP_OK) {
        this->error_code_ = error;
        this->error_string_ = "in rmt_new_simple_encoder";
        this->mark_failed();
        return;
      }
    }
    this->backlog_.set_capacity(this->max_pending_ - this->queue_depth_);
#else
    rmt_copy_encoder_config_t encoder;
    memset(&encoder, 0, sizeof(encoder));
    error = rmt_new_copy_encoder(&encoder, &this->encoder_);
    if (error != ESP_OK) {
      this->error_code_ = error;
      this->error_string_ = "in rmt_new_copy_encoder";
      this->mark_failed();
      return;
    }
#endif

    error = rmt_enable(this->channel_);
    if (error != ESP_OK) {
      this->error_code_ = error;
      this->error_string_ = "in rmt_enable";
      this->mark_failed();
      return;
    }
    this->digital_write(open_drain || this->inverted_);
    this->initialized_ = true;
  }

  if (this->current_carrier_frequency_ == 0 || this->carrier_duty_percent_ == 100) {
    error = rmt_apply_carrier(this->channel_, nullptr);
  } else {
    rmt_carrier_config_t carrier;
    memset(&carrier, 0, sizeof(carrier));
    carrier.frequency_hz = this->current_carrier_frequency_;
    carrier.duty_cycle = (float) this->carrier_duty_percent_ / 100.0f;
    carrier.flags.polarity_active_low = this->inverted_;
    carrier.flags.always_on = 1;
    error = rmt_apply_carrier(this->channel_, &carrier);
  }
  if (error != ESP_OK) {
    this->error_code_ = error;
    this->error_string_ = "in rmt_apply_carrier";
    this->mark_failed();
    return;
  }
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 1)
// Blocks until the hardware queue is empty, feeding the watchdog while waiting
void RemoteTransmitterComponent::wait_all_done_() {
  esp_err_t error;
  while ((error = rmt_tx_wait_all_done(this->channel_, RMT_WAIT_SLICE_MS)) == ESP_ERR_TIMEOUT) {
    App.feed_wdt();
  }
  if (error != ESP_OK) {
    ESP_LOGW(TAG, "rmt_tx_wait_all_done failed: %s", esp_err_to_name(error));
    this->status_set_warning();
  }
}

// Blocks until every queued and backlogged frame has gone out, reporting completions in order
void RemoteTransmitterComponent::wait_for_rmt_() {
  while (true) {
    this->wait_all_done_();
    this->deliver_completions_();
    if (this->backlog_.empty())
      return;
    this->pump_backlog_();
  }
}

void RemoteTransmitterComponent::deliver_completions_() {
  uint32_t done = this->done_count_;
  while (this->delivered_ != done) {
    this->delivered_++;
    this->complete_trigger_.trigger();
  }
}

void RemoteTransmitterComponent::submit_(RmtTxSlot &slot) {
  slot.store.times = slot.frame->times;
  slot.store.index = slot.frame->offset;
  rmt_transmit_config_t config;
  memset(&config, 0, sizeof(config));
  config.flags.eot_level = this->eot_level_;
  config.flags.queue_nonblocking = 1;
  esp_err_t error = rmt_transmit(this->channel_, slot.encoder, slot.frame->symbols,
                                 slot.frame->count * sizeof(rmt_symbol_half_t), &config);
  if (error != ESP_OK) {
    ESP_LOGW(TAG, "rmt_transmit failed: %s", esp_err_to_name(error));
    this->status_set_warning();
    // nothing will complete, so report it now
    this->complete_trigger_.trigger();
    return;
  }
  this->status_clear_warning();
  this->submitted_++;
}

// Moves backlogged frames into hardware queue slots as transmissions complete
void RemoteTransmitterComponent::pump_backlog_() {
  while (!this->backlog_.empty() && this->submitted_ - this->done_count_ < this->queue_depth_) {
    uint32_t carrier_frequency = this->backlog_.front().carrier_frequency;
    if (carrier_frequency != this->current_carrier_frequency_) {
      // the carrier applies to the whole channel, so it can only change once idle
      if (this->submitted_ != this->done_count_)
        return;
      this->current_carrier_frequency_ = carrier_frequency;
      this->configure_rmt_();
    }
    RmtTxSlot &slot = this->slots_[this->submitted_ % this->queue_depth_];
    slot.frame = this->backlog_.pop();
    this->submit_(slot);
  }
}

void RemoteTransmitterComponent::loop() {
  this->deliver_completions_();
  this->pump_backlog_();
  // the transmit done interrupt re-enables the loop for the next completion
  if (this->delivered_ == this->done_count_)
    this->disable_loop();
}

// Encodes the repeat gap followed by the frame; with out == nullptr only counts symbols.
// The gap leads the buffer so the encoder skips it on the first pass and replays it
// before every repeat; offset receives the index of the first frame symbol.
size_t RemoteTransmitterComponent::encode_symbols_(rmt_symbol_half_t *out, uint32_t send_wait, uint32_t *offset) {
  size_t count = write_symbols(out, 0, this->from_microseconds_(send_wait), this->eot_level_);
  *offset = count;
  for (int32_t value : this->temp_.get_data()) {
    bool level = value >= 0;
    if (!level) {
      value = -value;
    }
    count += write_symbols(out, count, this->from_microseconds_(static_cast<uint32_t>(value)), level ^ this->inverted_);
  }
  return count;
}

// Encodes temp_ into frame, growing it when needed; returns false when out of memory
bool RemoteTransmitterComponent::encode_frame_(RmtFramePtr &frame, size_t count, uint32_t send_times,
                                               uint32_t send_wait) {
  if (frame == nullptr || frame->capacity < count) {
    frame.reset(new_frame(count));
    if (frame == nullptr)
      return false;
  }
  frame->count = count;
  frame->times = send_times;
  frame->carrier_frequency = this->temp_.get_carrier_frequency();
  this->encode_symbols_(frame->symbols, send_wait, &frame->offset);
  return true;
}

void RemoteTransmitterComponent::send_internal(uint32_t send_times, uint32_t send_wait) {
  if (this->is_failed()) {
    return;
  }

  if (send_times == 0) {
    // transmit nothing, but both triggers still fire so an on_complete-sequenced
    // automation does not stall; a zero repeat count would never finish in the encoder
    this->transmit_trigger_.trigger();
    this->complete_trigger_.trigger();
    return;
  }

  uint32_t offset;
  size_t count = this->encode_symbols_(nullptr, send_wait, &offset);
  if (count <= offset) {
    ESP_LOGE(TAG, "Empty data");
    return;
  }

  uint32_t carrier_frequency = this->temp_.get_carrier_frequency();
  if (carrier_frequency != this->current_carrier_frequency_ && this->submitted_ == this->done_count_ &&
      this->backlog_.empty()) {
    this->current_carrier_frequency_ = carrier_frequency;
    this->configure_rmt_();
  }

  // a frame goes straight to a free hardware slot unless it must wait behind the
  // backlog or for a carrier change; beyond max_pending it is dropped
  RmtTxSlot *slot = nullptr;
  RmtFramePtr pending;
  RmtFramePtr *frame = &pending;
  if (carrier_frequency == this->current_carrier_frequency_ && this->backlog_.empty() &&
      this->submitted_ - this->done_count_ < this->queue_depth_) {
    slot = &this->slots_[this->submitted_ % this->queue_depth_];
    frame = &slot->frame;
  } else if (this->backlog_.full()) {
    frame = nullptr;
  }
  if (frame == nullptr || !this->encode_frame_(*frame, count, send_times, send_wait) ||
      (slot == nullptr && !this->backlog_.push(std::move(pending)))) {
    ESP_LOGW(TAG, "Transmit queue full, dropping");
    this->status_set_warning();
    this->transmit_trigger_.trigger();
    this->complete_trigger_.trigger();
    return;
  }

  this->transmit_trigger_.trigger();
  if (slot != nullptr) {
    this->submit_(*slot);
  }
  if (this->non_blocking_) {
    this->enable_loop();
  } else {
    this->wait_for_rmt_();
  }
}
#else
void RemoteTransmitterComponent::wait_for_rmt_() {
  esp_err_t error = rmt_tx_wait_all_done(this->channel_, -1);
  if (error != ESP_OK) {
    ESP_LOGW(TAG, "rmt_tx_wait_all_done failed: %s", esp_err_to_name(error));
    this->status_set_warning();
  }

  this->complete_trigger_.trigger();
}

void RemoteTransmitterComponent::send_internal(uint32_t send_times, uint32_t send_wait) {
  if (this->is_failed())
    return;

  if (this->current_carrier_frequency_ != this->temp_.get_carrier_frequency()) {
    this->current_carrier_frequency_ = this->temp_.get_carrier_frequency();
    this->configure_rmt_();
  }

  this->rmt_temp_.clear();
  this->rmt_temp_.reserve((this->temp_.get_data().size() + 1) / 2);
  uint32_t rmt_i = 0;
  rmt_symbol_word_t rmt_item;

  for (int32_t val : this->temp_.get_data()) {
    bool level = val >= 0;
    if (!level)
      val = -val;
    val = this->from_microseconds_(static_cast<uint32_t>(val));

    do {
      int32_t item = std::min(val, int32_t(RMT_SYMBOL_DURATION_MAX));
      val -= item;

      if (rmt_i % 2 == 0) {
        rmt_item.level0 = static_cast<uint32_t>(level ^ this->inverted_);
        rmt_item.duration0 = static_cast<uint32_t>(item);
      } else {
        rmt_item.level1 = static_cast<uint32_t>(level ^ this->inverted_);
        rmt_item.duration1 = static_cast<uint32_t>(item);
        this->rmt_temp_.push_back(rmt_item);
      }
      rmt_i++;
    } while (val != 0);
  }

  if (rmt_i % 2 == 1) {
    rmt_item.level1 = 0;
    rmt_item.duration1 = 0;
    this->rmt_temp_.push_back(rmt_item);
  }

  if ((this->rmt_temp_.data() == nullptr) || this->rmt_temp_.empty()) {
    ESP_LOGE(TAG, "Empty data");
    return;
  }
  this->transmit_trigger_.trigger();
  for (uint32_t i = 0; i < send_times; i++) {
    rmt_transmit_config_t config;
    memset(&config, 0, sizeof(config));
    config.flags.eot_level = this->eot_level_;
    esp_err_t error = rmt_transmit(this->channel_, this->encoder_, this->rmt_temp_.data(),
                                   this->rmt_temp_.size() * sizeof(rmt_symbol_word_t), &config);
    if (error != ESP_OK) {
      ESP_LOGW(TAG, "rmt_transmit failed: %s", esp_err_to_name(error));
      this->status_set_warning();
    } else {
      this->status_clear_warning();
    }
    error = rmt_tx_wait_all_done(this->channel_, -1);
    if (error != ESP_OK) {
      ESP_LOGW(TAG, "rmt_tx_wait_all_done failed: %s", esp_err_to_name(error));
      this->status_set_warning();
    }
    if (i + 1 < send_times)
      delayMicroseconds(send_wait);
  }
  this->complete_trigger_.trigger();
}
#endif

}  // namespace esphome::remote_transmitter

#endif  // SOC_RMT_SUPPORTED
#endif  // USE_ESP32
