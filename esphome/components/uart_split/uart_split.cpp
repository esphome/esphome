#include "uart_split.h"

#include "esphome/core/log.h"

namespace esphome::uart_split {

static const char *const TAG = "uart_split";

static constexpr size_t READ_CHUNK = 64;

void UartSplit::add_output(UartSplitOutput *output) {
  if (this->output_count_ >= MAX_OUTPUTS) {
    return;
  }
  this->outputs_[this->output_count_++] = output;
}

void UartSplit::loop() {
  if (this->parent_ == nullptr || this->output_count_ == 0) {
    return;
  }
  // A writer must not lose bytes, so stop reading the pins while its buffer is full.
  // A receive-only output drops instead, or a slow listener would stall the bus.
  size_t room = READ_CHUNK;
  bool saw_writer = false;
  for (uint8_t i = 0; i < this->output_count_; i++) {
    UartSplitOutput *output = this->outputs_[i];
    if (output->rx_only()) {
      continue;
    }
    saw_writer = true;
    size_t free = output->rx_free();
    if (free < room) {
      room = free;
    }
  }
  if (saw_writer && room == 0) {
    return;
  }
  size_t waiting = this->parent_->available();
  if (waiting < room) {
    room = waiting;
  }
  if (room == 0) {
    return;
  }
  uint8_t buf[READ_CHUNK];
  if (!this->parent_->read_array(buf, room)) {
    return;
  }
  for (size_t n = 0; n < room; n++) {
    for (uint8_t i = 0; i < this->output_count_; i++) {
      UartSplitOutput *output = this->outputs_[i];
      if (output->push_rx(buf[n])) {
        continue;
      }
      if (!output->drop_logged()) {
        ESP_LOGW(TAG, "Output %u dropped a received byte", i);
        output->mark_drop_logged();
      }
    }
  }
}

void UartSplit::mirror_tx(const UartSplitOutput *from, const uint8_t *data, size_t len) {
  for (uint8_t i = 0; i < this->output_count_; i++) {
    UartSplitOutput *output = this->outputs_[i];
    if (output == from || !output->mirror_tx()) {
      continue;
    }
    for (size_t n = 0; n < len; n++) {
      if (output->push_rx(data[n])) {
        continue;
      }
      if (!output->drop_logged()) {
        ESP_LOGW(TAG, "Output %u dropped a sent byte", i);
        output->mark_drop_logged();
      }
      break;
    }
  }
}

void UartSplit::dump_config() {
  ESP_LOGCONFIG(TAG, "UART Split:");
  ESP_LOGCONFIG(TAG, "  Outputs: %u", this->output_count_);
  for (uint8_t i = 0; i < this->output_count_; i++) {
    UartSplitOutput *output = this->outputs_[i];
    ESP_LOGCONFIG(TAG, "  Output %u:", i);
    ESP_LOGCONFIG(TAG, "    RX Only: %s", YESNO(output->rx_only()));
    ESP_LOGCONFIG(TAG, "    Direction: %s", LOG_STR_ARG(output->mirror_tx() ? LOG_STR("BOTH") : LOG_STR("RX")));
  }
}

void UartSplitOutput::write_array(const uint8_t *data, size_t len) {
  if (this->rx_only_) {
    if (!this->drop_logged_ && len > 0) {
      ESP_LOGW(TAG, "RX-only output dropped %u bytes", static_cast<unsigned>(len));
      this->drop_logged_ = true;
    }
    return;
  }
  if (this->split_ == nullptr || this->split_->parent() == nullptr || len == 0) {
    return;
  }
  this->split_->parent()->write_array(data, len);
  this->split_->mirror_tx(this, data, len);
}

bool UartSplitOutput::peek_byte(uint8_t *data) {
  if (this->rx_.empty()) {
    return false;
  }
  *data = this->rx_.front();
  return true;
}

bool UartSplitOutput::read_array(uint8_t *data, size_t len) {
  if (this->rx_.size() < len) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    data[i] = this->rx_.front();
    this->rx_.pop();
  }
  return true;
}

size_t UartSplitOutput::available_for_write() {
  if (this->rx_only_ || this->split_ == nullptr || this->split_->parent() == nullptr) {
    return 0;
  }
  return this->split_->parent()->available_for_write();
}

uart::UARTFlushResult UartSplitOutput::flush() {
  if (this->rx_only_ || this->split_ == nullptr || this->split_->parent() == nullptr) {
    return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS;
  }
  return this->split_->parent()->flush();
}

bool UartSplitOutput::is_connected() {
  if (this->split_ == nullptr || this->split_->parent() == nullptr) {
    return false;
  }
  return this->split_->parent()->is_connected();
}

}  // namespace esphome::uart_split
