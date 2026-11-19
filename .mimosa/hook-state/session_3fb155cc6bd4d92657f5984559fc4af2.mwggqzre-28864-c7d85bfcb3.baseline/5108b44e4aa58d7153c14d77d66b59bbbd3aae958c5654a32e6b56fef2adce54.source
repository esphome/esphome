#pragma once

#ifdef USE_ESP32
#include "esphome/core/helpers.h"
#include <driver/uart.h>

namespace esphome::logger {

inline void HOT Logger::write_msg_(const char *msg, uint16_t len) {
#if defined(USE_LOGGER_UART_SELECTION_USB_CDC) || defined(USE_LOGGER_UART_SELECTION_USB_SERIAL_JTAG)
  // USB CDC/JTAG - single write including newline (already in buffer)
  // Use fwrite to stdout which goes through VFS to USB console
  //
  // Note: These defines indicate the user's YAML configuration choice (hardware_uart: USB_CDC/USB_SERIAL_JTAG).
  // They are ONLY defined when the user explicitly selects USB as the logger output in their config.
  // This is compile-time selection, not runtime detection - if USB is configured, it's always used.
  // There is no fallback to regular UART if "USB isn't connected" - that's the user's responsibility
  // to configure correctly for their hardware. This approach eliminates runtime overhead.
  fwrite(msg, 1, len, stdout);
#else
  // Regular UART - single write including newline (already in buffer)
  uart_write_bytes(this->uart_num_, msg, len);
#endif
}

}  // namespace esphome::logger

#endif
