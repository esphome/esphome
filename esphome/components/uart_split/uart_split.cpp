#include "uart_split.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cstdint>

namespace esphome::uart_split {

ESPHOME_LOG_TAG(TAG, "uart_split");

static constexpr size_t READ_CHUNK = 64;

void UartSplit::setup() {
  // An output carries the parent's bytes, so it reports the parent's settings to the devices on it.
  for (UartSplitOutput *output : this->outputs_) {
    output->copy_settings();
  }
}

void UartSplit::loop() {
  // A debugger fires inside read_array(), and what it writes comes back here. Reading again from there would hand the
  // bytes on in the wrong order and leave the outer pass a failed read.
  if (this->in_loop_) {
    return;
  }
  this->in_loop_ = true;
  this->pass_();
  this->in_loop_ = false;
}

void UartSplit::pass_() {
  size_t waiting = this->parent_->available();
  if (waiting == 0) {
    this->high_freq_.stop();
    return;
  }
  // A full writer leaves new bytes in the parent's buffer until it is half full; then the writer drops.
  size_t room = RX_BUFFER_SIZE;
  for (UartSplitOutput *output : this->outputs_) {
    output->end_dropping_if_empty();
    if (output->holds_back()) {
      room = std::min(room, output->rx_free());
    }
  }
  // A parent that does not report its buffer size (0) has no half to wait for. It still holds back for a writer that
  // has room, but never for a full one: the parent might never get that full.
  size_t hold_limit = this->parent_->get_rx_buffer_size() / 2;
  if (hold_limit == 0 && room > 0) {
    hold_limit = RX_BUFFER_SIZE / 2;
  }
  // No output holds more than RX_BUFFER_SIZE, so reading more in one pass would only drop it.
  size_t take = std::min(waiting, RX_BUFFER_SIZE);
  if (take > room && waiting < hold_limit) {
    take = room;
  }
  uint8_t buf[READ_CHUNK];
  size_t taken = 0;
  while (taken < take) {
    size_t len = std::min(take - taken, READ_CHUNK);
    if (!this->parent_->read_array(buf, len)) {
      break;
    }
    for (size_t i = 0; i < this->outputs_.size(); i++) {
      this->push_(i, buf, len);
    }
    taken += len;
  }
  // A pass moves at most RX_BUFFER_SIZE bytes. While more wait and this pass moved some, do not wait for the next tick.
  this->high_freq_.set(taken > 0 && waiting > taken);
}

void UartSplit::hand_on_received(const UartSplitOutput *from) {
  for (const UartSplitOutput *output : this->outputs_) {
    if (output != from && output->mirror_tx()) {
      // What the parent received before this write comes first on the output. A write that takes time (a full TX
      // FIFO) can receive more, and that comes after it.
      this->loop();
      return;
    }
  }
}

void UartSplit::mirror_tx(const UartSplitOutput *from, const uint8_t *data, size_t len) {
  for (size_t i = 0; i < this->outputs_.size(); i++) {
    UartSplitOutput *output = this->outputs_[i];
    if (output != from && output->mirror_tx()) {
      this->push_(i, data, len);
    }
  }
}

void UartSplit::push_(size_t index, const uint8_t *data, size_t len) {
  UartSplitOutput *output = this->outputs_[index];
  if (!output->inject_rx(data, len) && output->start_dropping()) {
    ESP_LOGW(TAG, "Output %u is full; dropping bytes until it has been read", static_cast<unsigned>(index));
  }
}

void UartSplit::dump_config() {
  ESP_LOGCONFIG(TAG,
                "UART Split:\n"
                "  Outputs: %u",
                static_cast<unsigned>(this->outputs_.size()));
  for (size_t i = 0; i < this->outputs_.size(); i++) {
    UartSplitOutput *output = this->outputs_[i];
    ESP_LOGCONFIG(TAG,
                  "  Output %u:\n"
                  "    RX Only: %s\n"
                  "    Direction: %s",
                  static_cast<unsigned>(i), YESNO(output->rx_only()),
                  LOG_STR_ARG(output->mirror_tx() ? LOG_STR("BOTH") : LOG_STR("RX")));
  }
}

void UartSplitOutput::copy_settings() {
  uart::UARTComponent *parent = this->split_->parent();
  this->set_baud_rate(parent->get_baud_rate());
  this->set_data_bits(parent->get_data_bits());
  this->set_parity(parent->get_parity());
  this->set_stop_bits(parent->get_stop_bits());
  this->rx_full_threshold_ = parent->get_rx_full_threshold();
  this->rx_timeout_ = parent->get_rx_timeout();
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
  this->debug_tx_(data, len);
  this->split_->hand_on_received(this);
  this->split_->parent()->write_array(data, len);
  this->split_->mirror_tx(this, data, len);
}

size_t UartSplitOutput::available_for_write() {
  if (this->rx_only_) {
    // Takes and drops every byte, so no line time to pace to. 0 would stall a writer, and SIZE_MAX (unknown) would
    // pace it to the baud rate.
    return RX_BUFFER_SIZE;
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
