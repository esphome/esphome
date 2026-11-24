#include "virtual_uart.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome::virtual_uart {

ESPHOME_LOG_TAG(TAG, "virtual_uart");

void log_own_write_dropped() { ESP_LOGW(TAG, "on_tx wrote to its own UART, dropped"); }

void log_inject_dropped(size_t len) { ESP_LOGW(TAG, "No room for %zu bytes in the RX buffer, dropped", len); }

void dump_virtual_uart_config(uart::UARTComponent *component) {
  ESP_LOGCONFIG(TAG,
                "Virtual UART:\n"
                "  Baud Rate: %" PRIu32 " baud\n"
                "  Data Bits: %u\n"
                "  Parity: %s\n"
                "  Stop bits: %u\n"
                "  RX Buffer Size: %zu",
                component->get_baud_rate(), component->get_data_bits(),
                LOG_STR_ARG(uart::parity_to_str(component->get_parity())), component->get_stop_bits(),
                component->get_rx_buffer_size());
}

}  // namespace esphome::virtual_uart
