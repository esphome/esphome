#include "uart_split.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cstdint>

namespace esphome::uart_split {

static const char *const TAG = "uart_split";

static constexpr size_t READ_CHUNK = 64;

void UartSplit::add_output(UartSplitOutput *output) {
  if (this->output_count_ >= MAX_OUTPUTS) {
    return;
  }
  this->outputs_[this->output_count_++] = output;
}

void UartSplit::setup() {
  // An output carries the parent's bytes, so it reports the parent's settings to the devices on it.
  for (uint8_t i = 0; i < this->output_count_; i++) {
    this->outputs_[i]->copy_settings();
  }
}

void UartSplit::loop() {
  size_t waiting = this->parent_->available();
  if (waiting == 0) {
    return;
  }
  // A full writer leaves new bytes in the parent's buffer until it is half full; then the writer drops.
  size_t hold_limit = this->parent_->get_rx_buffer_size() / 2;
  if (hold_limit == 0) {
    hold_limit = RX_BUFFER_SIZE / 2;
  }
  size_t room = RX_BUFFER_SIZE;
  for (uint8_t i = 0; i < this->output_count_; i++) {
    UartSplitOutput *output = this->outputs_[i];
    output->end_dropping_if_empty();
    if (output->holds_back()) {
      room = std::min(room, output->rx_free());
    }
  }
  // No output holds more than RX_BUFFER_SIZE, so reading more in one pass would only drop it.
  size_t take = std::min(waiting, RX_BUFFER_SIZE);
  if (take > room && waiting < hold_limit) {
    take = room;
  }
  uint8_t buf[READ_CHUNK];
  while (take > 0) {
    size_t len = std::min(take, READ_CHUNK);
    if (!this->parent_->read_array(buf, len)) {
      return;
    }
    for (uint8_t i = 0; i < this->output_count_; i++) {
      this->push_(i, buf, len);
    }
    take -= len;
  }
}

void UartSplit::mirror_tx(const UartSplitOutput *from, const uint8_t *data, size_t len) {
  for (uint8_t i = 0; i < this->output_count_; i++) {
    UartSplitOutput *output = this->outputs_[i];
    if (output != from && output->mirror_tx()) {
      this->push_(i, data, len);
    }
  }
}

void UartSplit::push_(uint8_t index, const uint8_t *data, size_t len) {
  UartSplitOutput *output = this->outputs_[index];
  if (!output->inject_rx(data, len) && output->start_dropping()) {
    ESP_LOGW(TAG, "Output %u is full; dropping bytes until it has been read", index);
  }
}

void UartSplit::dump_config() {
  ESP_LOGCONFIG(TAG,
                "UART Split:\n"
                "  Outputs: %u",
                this->output_count_);
  for (uint8_t i = 0; i < this->output_count_; i++) {
    UartSplitOutput *output = this->outputs_[i];
    ESP_LOGCONFIG(TAG,
                  "  Output %u:\n"
                  "    RX Only: %s\n"
                  "    Direction: %s",
                  i, YESNO(output->rx_only()), LOG_STR_ARG(output->mirror_tx() ? LOG_STR("BOTH") : LOG_STR("RX")));
  }
}

void UartSplitOutput::copy_settings() {
  uart::UARTComponent *parent = this->split_->parent();
  this->set_baud_rate(parent->get_baud_rate());
  this->set_data_bits(parent->get_data_bits());
  this->set_parity(parent->get_parity());
  this->set_stop_bits(parent->get_stop_bits());
}

#if defined(USE_ESP8266) || defined(USE_ESP32)
void UartSplitOutput::load_settings(bool dump_config) {
  if (!this->load_settings_warned_) {
    this->load_settings_warned_ = true;
    ESP_LOGW(TAG, "load_settings() ignored; change the settings of the UART that is split");
  }
  this->copy_settings();
}
#endif

void UartSplitOutput::write_array(const uint8_t *data, size_t len) {
  if (len == 0) {
    return;
  }
  if (this->rx_only_) {
    if (!this->write_drop_logged_) {
      ESP_LOGW(TAG, "An RX-only output drops the bytes written to it");
      this->write_drop_logged_ = true;
    }
    return;
  }
  this->split_->parent()->write_array(data, len);
  this->split_->mirror_tx(this, data, len);
}

size_t UartSplitOutput::available_for_write() {
  if (this->rx_only_) {
    // Takes and drops every byte; 0 would stall a writer that paces on it.
    return SIZE_MAX;
  }
  return this->split_->parent()->available_for_write();
}

uart::UARTFlushResult UartSplitOutput::flush() {
  if (this->rx_only_) {
    return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS;
  }
  return this->split_->parent()->flush();
}

bool UartSplitOutput::is_connected() { return this->split_->parent()->is_connected(); }

}  // namespace esphome::uart_split
