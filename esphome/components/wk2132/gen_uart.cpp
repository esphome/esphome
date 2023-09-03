/// @file gen_uart.cpp
/// @author DrCoolzic
/// @brief generic uart class implementation

#include "gen_uart.h"

namespace esphome {
namespace gen_uart {

static const char *const TAG = "gen_uart";

///////////////////////////////////////////////////////////////////////////////
// The GenericUART methods
///////////////////////////////////////////////////////////////////////////////
bool GenericUART::read_array(uint8_t *buffer, size_t len) {
  if (len > this->max_size_()) {
    ESP_LOGE(TAG, "Read buffer invalid call: requested %d bytes max size %d ...", len, this->max_size_());
    return false;
  }
  auto available = this->receive_buffer_.count();
  if (len > available) {
    ESP_LOGE(TAG, "read_array buffer underflow requested %d bytes available %d ...", len, available);
    len = available;
  }
  for (size_t i = 0; i < len; i++) {
    this->receive_buffer_.pop(buffer[i]);
  }
  return true;
}

void GenericUART::write_array(const uint8_t *buffer, size_t len) {
  if (len > this->max_size_()) {
    ESP_LOGE(TAG, "Write buffer invalid call: requested %d bytes max size %d ...", len, this->max_size_());
    len = this->max_size_();
  }

  this->write_data_(buffer, len);
}

void GenericUART::flush() {
  uint32_t const start_time = millis();
  while (this->tx_in_fifo_()) {  // wait until buffer empty
    if (millis() - start_time > 100) {
      ESP_LOGE(TAG, "Flush timed out: still %d bytes not sent...", this->tx_in_fifo_());
      return;
    }
    yield();  // reschedule thread to avoid blocking
  }
}

void GenericUART::rx_fifo_to_ring_() {
  // here we transfer from fifo to ring buffer

  uint8_t data[RING_BUFFER_SIZE];
  // we look if some characters has been received in the fifo
  if (auto to_transfer = this->rx_in_fifo_()) {
    this->read_data_(data, to_transfer);
    auto free = this->receive_buffer_.free();
    if (to_transfer > free) {
      ESP_LOGV(TAG, "Ring buffer overrun --> requested %d available %d", to_transfer, free);
      to_transfer = free;  // hopefully will do the rest next time
    }
    ESP_LOGV(TAG, "Transferred %d bytes from rx_fifo to buffer ring", to_transfer);
    for (size_t i = 0; i < to_transfer; i++)
      this->receive_buffer_.push(data[i]);
  }
}

///////////////////////////////////////////////////////////////////////////////
/// AUTOTEST FUNCTIONS BELOW
///////////////////////////////////////////////////////////////////////////////
#ifdef AUTOTEST_COMPONENT

class Increment {  // A "Functor" (A class object that acts like a method with state!)
 public:
  Increment() : i_(0) {}
  uint8_t operator()() { return i_++; }

 private:
  uint8_t i_;
};

void print_buffer(std::vector<uint8_t> buffer) {
  // quick and ugly hex converter to display buffer in hex format
  char hex_buffer[80];
  hex_buffer[50] = 0;
  for (size_t i = 0; i < buffer.size(); i++) {
    snprintf(&hex_buffer[3 * (i % 16)], sizeof(hex_buffer), "%02X ", buffer[i]);
    if (i % 16 == 15)
      ESP_LOGI(TAG, "   %s", hex_buffer);
  }
  if (buffer.size() % 16) {
    // null terminate if incomplete line
    hex_buffer[3 * (buffer.size() % 16) + 2] = 0;
    ESP_LOGI(TAG, "   %s", hex_buffer);
  }
}

/// @brief test the write_array method
void GenericUART::uart_send_test_(char *preamble) {
  auto start_exec = millis();
  // we send the maximum possible
  this->flush();
  size_t const to_send = this->max_size_() - tx_in_fifo_();
  if (to_send > 0) {
    std::vector<uint8_t> output_buffer(to_send);
    generate(output_buffer.begin(), output_buffer.end(), Increment());  // fill with incrementing number
    output_buffer[0] = to_send;                     // we send as the first byte the length of the buffer
    this->write_array(&output_buffer[0], to_send);  // we send the buffer
    this->flush();                                  // we wait until they are gone
    ESP_LOGI(TAG, "%s => sending  %d bytes - exec time %d ms ...", preamble, to_send, millis() - start_exec);
  }
}

/// @brief test the read_array method
void GenericUART::uart_receive_test_(char *preamble, bool print_buf) {
  auto start_exec = millis();
  uint8_t const to_read = this->available();
  if (to_read > 0) {
    std::vector<uint8_t> buffer(to_read);
    this->read_array(&buffer[0], to_read);
    if (print_buf)
      print_buffer(buffer);
  }
  ESP_LOGI(TAG, "%s => received %d bytes - exec time %d ms ...", preamble, to_read, millis() - start_exec);
}

#endif

}  // namespace gen_uart
}  // namespace esphome
