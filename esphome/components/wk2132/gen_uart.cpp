/// @file gen_uart.cpp
/// @author DrCoolzic
/// @brief generic uart class implementation

#include "gen_uart.h"

namespace esphome {
namespace wk2132 {

static const char *const TAG = "gen_uart";

///////////////////////////////////////////////////////////////////////////////
// The GenericUART methods
///////////////////////////////////////////////////////////////////////////////

bool GenericUART::read_array(uint8_t *buffer, size_t len) {
  if (len > this->fifo_size()) {
    ESP_LOGE(TAG, "Read buffer invalid call: requested %d bytes max size %d ...", len, this->fifo_size());
    return false;
  }
  auto available = this->receive_buffer_.count();
  if (len > available) {
    ESP_LOGE(TAG, "read_array buffer underflow requested %d bytes available %d ...", len, available);
    len = available;
  }
  // retrieve the bytes from ring buffer
  for (size_t i = 0; i < len; i++) {
    this->receive_buffer_.pop(buffer[i]);
  }
  return true;
}

void GenericUART::write_array(const uint8_t *buffer, size_t len) {
  if (len > this->fifo_size()) {
    ESP_LOGE(TAG, "Write buffer invalid call: requested %d bytes max size %d ...", len, this->fifo_size());
    len = this->fifo_size();
  }

  auto free = this->transmit_buffer_.free();
  if (len > free) {
    ESP_LOGE(TAG, "write_array buffer overflow requested %d bytes available %d ...", len, free);
    len = free;
  }

  // transfers the bytes from the ring buffer
  for (size_t i = 0; i < len; i++) {
    this->transmit_buffer_.push(buffer[i]);
  }
}

///////////////////////////////////////////////////////////////////////////////
// AUTOTEST FUNCTIONS BELOW
///////////////////////////////////////////////////////////////////////////////
#ifdef AUTOTEST_COMPONENT

/// @brief A Functor class that return incremented numbers
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
  size_t const to_send = this->fifo_size();
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

}  // namespace wk2132
}  // namespace esphome
