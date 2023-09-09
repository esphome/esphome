/// @file uart_tester.cpp
/// @author DrCoolzic
/// @brief uart_tester classes implementation

#include "uart_tester.h"

namespace esphome {
namespace uart_tester {

static const char *const TAG = "UARTTester";

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

/// @brief A Functor class that return incremented numbers
class Increment {  // A "Functor" (A class object that acts like a method with state!)
 public:
  Increment() : i_(0) {}
  uint8_t operator()() { return i_++; }

 private:
  uint8_t i_;
};

// lambda
auto elapsed = [](uint32_t &last_time, bool micro = false) {
  if (micro) {
    auto e = micros() - last_time;
    last_time = micros();
    return e;
  } else {
    auto e = millis() - last_time;
    last_time = millis();
    return e;
  }
};

///////////////////////////////////////////////////////////////////////////////
// The UARTTester methods
///////////////////////////////////////////////////////////////////////////////
void UARTTester::setup() {
  ESP_LOGCONFIG(TAG, "Initializing uart tester %s ...", this->get_name());
  generate(output_buffer.begin(), output_buffer.end(), Increment());
}

void UARTTester::dump_config() {
  ESP_LOGCONFIG(TAG, "  Uart tester %s initialized", this->get_name());
  this->initialized_ = true;
}

void UARTTester::uart_send_frame_() {
  uint32_t time;
  elapsed(time);
  this->write_array(&output_buffer[0], BUFFER_SIZE);
  this->flush();
  ESP_LOGI(TAG, " sent frame of %d bytes - execution time %d µs", BUFFER_SIZE, elapsed(time));
}

void UARTTester::uart_receive_frame_() {
  uint32_t watch_dog = millis();
  uint32_t time;
  uint8_t to_read = BUFFER_SIZE;

  elapsed(time, true);
  while (to_read) {
    int available;
    if (available = this->available()) {
      this->read_array(&input_buffer[BUFFER_SIZE - to_read], available);
      to_read -= available;
    }
    while (to_read && !this->available()) {
      if (millis() - watch_dog > 100) {
        ESP_LOGE(TAG, " Receive frame timed out: still %d bytes not received...", to_read);
        return;
      }
      yield();  // reschedule our thread to avoid blocking
    }
    ESP_LOGI(TAG, " received frame of %d bytes - execution time %d µs", available, elapsed(time, true));
  }
  // print_buffer(input_buffer);
}

void UARTTester::uart_receive_frame_1_by_1_() {
  uint32_t watch_dog = millis();
  uint32_t time;
  uint8_t to_read = BUFFER_SIZE;

  elapsed(time, true);
  while (to_read) {
    if (this->available()) {
      this->read_array(&input_buffer[BUFFER_SIZE - to_read], 1);
      to_read--;
    }
    while (to_read && !this->available()) {
      if (millis() - watch_dog > 200) {
        ESP_LOGE(TAG, " Receive frame 1 by 1 timed out: still %d bytes not received...", to_read);
        return;
      }
      yield();  // reschedule our thread to avoid blocking
    }
  }
  ESP_LOGI(TAG, " received %d byte 1 by 1 - execution time %d µs", BUFFER_SIZE - to_read, elapsed(time, true));
  // print_buffer(input_buffer);
}

void UARTTester::loop() {
  if (!this->initialized_)
    return;
  static uint16_t loop_calls = 0;
  static uint32_t loop_time = 0;
  uint32_t time;

  ESP_LOGI(TAG, "loop %d for tester %s : %d ms since last call ...", loop_calls++, this->get_name(),
           elapsed(loop_time));

  if (mode_.none()) {  // mode 0
    char preamble[64];

    elapsed(time);
    this->uart_send_frame_();
    ESP_LOGI(TAG, "  send frame call time %d ms", elapsed(time));

    elapsed(time);
    this->uart_receive_frame_();
    ESP_LOGI(TAG, "  receive frame call time %d ms", elapsed(time));

    elapsed(time);
    this->uart_send_frame_();
    ESP_LOGI(TAG, "  send frame call time %d ms", elapsed(time));

    elapsed(time);
    this->uart_receive_frame_1_by_1_();
    ESP_LOGI(TAG, "  receive frame 1 by 1 call time %d ms", elapsed(time));
  }

  if (this->mode_.test(1)) {  // test echo mode (bit 0)
    // elapsed(time);
    // for (auto *uart : this->uart_list_) {
    //   uint8_t data;
    //   if (child->available()) {
    //     child->read_byte(&data);
    //     ESP_LOGI(TAG, "echo mode: read -> send %02X", data);
    //     child->write_byte(data);
    //   }
    // }
    // ESP_LOGI(TAG, "echo execution time %d µs...", elapsed(time));
  }

  ESP_LOGI(TAG, "loop execution time %d ms...", millis() - loop_time);
}

}  // namespace uart_tester
}  // namespace esphome
