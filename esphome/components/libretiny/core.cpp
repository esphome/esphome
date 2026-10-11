#ifdef USE_LIBRETINY

// HAL functions live in hal.cpp; this file only holds the serial port
// workaround below.

#include "core.h"

#ifdef USE_LN882X
#include <sdk_private.h>

// The SDK's open port handles (components/serial/serial.c); not in its header.
extern "C" Serial_t *serial_handles[SER_PORT_NUM];

namespace esphome::libretiny {

void ensure_serial_handle(uint8_t port) {
  if (port >= SER_PORT_NUM || serial_handles[port] != nullptr)
    return;
  // serial_init() zeroes and opens it; the port stays open for good.
  serial_handles[port] = new Serial_t();
}

}  // namespace esphome::libretiny
#endif  // USE_LN882X

#endif  // USE_LIBRETINY
