#pragma once

#ifdef USE_LIBRETINY

#include <Arduino.h>

namespace esphome::libretiny {

#ifdef USE_LN882X
/// Give a hardware UART its SDK handle before its Serial begin()s.
///
/// LibreTiny's LN882H Serial passes the SDK's per port handle to
/// serial_init(), which fills it in, but only the default log port (UART1)
/// has one at boot. Any other port hands it NULL and the chip faults before
/// setup, so a logger or uart on UART0 never boots. Drop this once LibreTiny
/// gives each port storage of its own.
void ensure_serial_handle(uint8_t port);
#else
inline void ensure_serial_handle(uint8_t /*port*/) {}
#endif

}  // namespace esphome::libretiny

#endif  // USE_LIBRETINY
