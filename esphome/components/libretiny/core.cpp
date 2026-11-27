#ifdef USE_LIBRETINY

// HAL functions live in hal.cpp; this file only holds the serial port
// workaround below.

#include "core.h"
#include "esphome/core/defines.h"

#ifdef USE_LN882X
#include <sdk_private.h>

// The SDK's open port handles (components/serial/serial.c); not in its header.
extern "C" Serial_t *serial_handles[SER_PORT_NUM];

namespace esphome::libretiny {

void ensure_serial_handle(uint8_t port) {
  if (port >= SER_PORT_NUM)
    return;
#ifdef USE_UART_TX_BREAK
  // serial_deinit() clears the SDK entry only; reuse the storage.
  static Serial_t *storage[SER_PORT_NUM];
  if (serial_handles[port] != nullptr) {
    storage[port] = serial_handles[port];
    return;
  }
  if (storage[port] == nullptr)
    storage[port] = new Serial_t();
  serial_handles[port] = storage[port];
#else
  if (serial_handles[port] != nullptr)
    return;
  // serial_init() zeroes and opens it; the port stays open for good.
  serial_handles[port] = new Serial_t();
#endif
}

}  // namespace esphome::libretiny
#endif  // USE_LN882X

#endif  // USE_LIBRETINY
