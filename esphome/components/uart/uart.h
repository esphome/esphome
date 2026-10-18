#pragma once

#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "uart_component.h"

namespace esphome::uart {

class UARTDevice {
 public:
  UARTDevice() = default;
  UARTDevice(UARTComponent *parent) : parent_(parent) {}

  void set_uart_parent(UARTComponent *parent) { this->parent_ = parent; }

  void write_byte(uint8_t data) { this->parent_->write_byte(data); }

  void set_rx_full_threshold(size_t rx_full_threshold) { this->parent_->set_rx_full_threshold(rx_full_threshold); }
  void set_rx_full_threshold_ms(size_t time) { this->parent_->set_rx_full_threshold_ms(time); }
  size_t get_rx_full_threshold() { return this->parent_->get_rx_full_threshold(); }
  void set_rx_timeout(size_t rx_timeout) { this->parent_->set_rx_timeout(rx_timeout); }
  size_t get_rx_timeout() { return this->parent_->get_rx_timeout(); }

  void write_array(const uint8_t *data, size_t len) { this->parent_->write_array(data, len); }
  void write_array_progmem(const uint8_t *data, size_t len) { this->parent_->write_array_progmem(data, len); }
  void write_array(const std::vector<uint8_t> &data) { this->parent_->write_array(data); }
  template<size_t N> void write_array(const std::array<uint8_t, N> &data) {
    this->parent_->write_array(data.data(), data.size());
  }

  void write_str(const char *str) { this->parent_->write_str(str); }

  bool read_byte(uint8_t *data) { return this->parent_->read_byte(data); }
  bool peek_byte(uint8_t *data) { return this->parent_->peek_byte(data); }

  bool read_array(uint8_t *data, size_t len) { return this->parent_->read_array(data, len); }
  template<size_t N> optional<std::array<uint8_t, N>> read_array() {  // NOLINT
    // One object returned on both paths; a separate empty optional trips GCC's -Wmaybe-uninitialized.
    optional<std::array<uint8_t, N>> res(std::in_place);
    if (!this->read_array(res->data(), N)) {
      res.reset();
    }
    return res;
  }

  size_t available() { return this->parent_->available(); }

  /// Pass the bytes available now to on_chunk(data, len) in N byte chunks; returning false stops.
  /// Returns false only on a read error.
  template<size_t N = 64, typename F> bool read_available_chunks(F &&on_chunk) {
    return this->read_available_chunks<N>(on_chunk, this->available());
  }
  /// Same, reading exactly count bytes; count must not exceed available().
  template<size_t N = 64, typename F> bool read_available_chunks(F &&on_chunk, size_t count) {
    static_assert(N > 0, "read buffer size must be non-zero");
    uint8_t buf[N];
    while (count > 0) {
      const size_t len = std::min(count, N);
      if (!this->read_array(buf, len)) {
        return false;
      }
      count -= len;
      if (!continue_after(on_chunk, static_cast<const uint8_t *>(buf), len)) {
        break;
      }
    }
    return true;
  }

  /// Same as read_available_chunks(), calling on_byte(uint8_t) once per byte.
  template<size_t N = 64, typename F> bool read_available(F &&on_byte) {
    return this->read_available<N>(on_byte, this->available());
  }
  template<size_t N = 64, typename F> bool read_available(F &&on_byte, size_t count) {
    return this->read_available_chunks<N>(
        [&on_byte](const uint8_t *data, size_t len) {
          for (const uint8_t *p = data; p != data + len; p++) {
            if (!continue_after(on_byte, *p)) {
              return false;
            }
          }
          return true;
        },
        count);
  }

  /// Read and drop every byte available now, in chunks; returns how many were dropped.
  size_t discard_available();

  UARTFlushResult flush() { return this->parent_->flush(); }

  // Compat APIs
  int read() {
    uint8_t data;
    if (!this->read_byte(&data))
      return -1;
    return data;
  }
  size_t write(uint8_t data) {
    this->write_byte(data);
    return 1;
  }
  int peek() {
    uint8_t data;
    if (!this->peek_byte(&data))
      return -1;
    return data;
  }

  /// Check that the configuration of the UART bus matches the provided values and otherwise print a warning
  ESPDEPRECATED("Use uart.final_validate_device_schema() in Python instead. Removed in 2027.3.0", "2026.9.0")
  void check_uart_settings(uint32_t baud_rate, uint8_t stop_bits = 1,
                           UARTParityOptions parity = UART_CONFIG_PARITY_NONE, uint8_t data_bits = 8);

 protected:
  /// Call f(args...) and return its result; a void callback always continues.
  template<typename F, typename... A> static bool continue_after(F &f, A... args) {
    using R = std::invoke_result_t<F &, A...>;
    if constexpr (std::is_void_v<R>) {
      f(args...);
      return true;
    } else {
      static_assert(std::is_same_v<std::remove_cvref_t<R>, bool>, "callback must return void or bool");
      return f(args...);
    }
  }

  UARTComponent *parent_{nullptr};
};

}  // namespace esphome::uart
