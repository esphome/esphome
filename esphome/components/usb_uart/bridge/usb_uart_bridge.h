#pragma once
#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "esphome/components/uart/uart_component_esp_idf.h"
#include "esphome/components/usb_cdc_acm/usb_cdc_acm.h"
#include "esphome/core/component.h"

#include <atomic>
#include <memory>
#include "freertos/ringbuf.h"
#include "tinyusb_cdc_acm.h"

namespace esphome::usb_uart_bridge {

class USBUARTBridge final : public Component {
 public:
  // Upper bound on the RX task's blocking read, so pause() takes effect without
  // aborting the read. Arriving bytes still unblock it immediately.
  static constexpr uint32_t UART_RX_WAIT_MS = 250;

  USBUARTBridge(uart::IDFUARTComponent *uart_parent, usb_cdc_acm::USBCDCACMInstance *usb_cdc_parent,
                size_t uart_rx_buffer_size, size_t uart_tx_buffer_size)
      : uart_rx_buffer_size_(uart_rx_buffer_size),
        uart_tx_buffer_size_(uart_tx_buffer_size),
        uart_parent_(uart_parent),
        usb_cdc_parent_(usb_cdc_parent) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_dtr_pin(GPIOPin *dtr_pin) { this->dtr_pin_ = dtr_pin; }
  void set_rts_pin(GPIOPin *rts_pin) { this->rts_pin_ = rts_pin; }

  void set_line_coding();
  void set_line_state(bool dtr, bool rts);

  /**
   * Stop forwarding in both directions and hand the UART back to its configured
   * framing, so another component may use the bus. Bytes the host sends while paused
   * are discarded. Main-loop only. The worker tasks stop within UART_RX_WAIT_MS; poll
   * is_paused() before touching the bus.
   */
  void pause();
  /// Re-apply the host's line coding, then resume forwarding. Main-loop only.
  void resume();
  /// True once pause() was called and both worker tasks have stopped touching the UART.
  bool is_paused() const { return this->paused_ != 0 && this->rx_parked_ != 0 && this->tx_busy_ == 0; }

 protected:
  static void uart_rx_task_fn(void *arg);
  static void uart_tx_task_fn(void *arg);
  void uart_rx_task_();
  void uart_tx_task_();
  void uart_settings_reload_();
  void restore_configured_framing_();
  // Copy the host's line coding onto the UART settings; true if anything changed.
  bool sync_host_framing_();

  TaskHandle_t uart_rx_task_handle_{nullptr};
  TaskHandle_t uart_tx_task_handle_{nullptr};
  TaskHandle_t usb_tx_task_handle_{nullptr};

  GPIOPin *dtr_pin_{nullptr};
  GPIOPin *rts_pin_{nullptr};

  uint32_t reload_requested_at_{0};

  size_t uart_rx_buffer_size_;
  size_t uart_tx_buffer_size_;
  std::unique_ptr<uint8_t[]> uart_rx_buffer_{nullptr};
  std::unique_ptr<uint8_t[]> uart_tx_buffer_{nullptr};

  uart::IDFUARTComponent *uart_parent_;
  usb_cdc_acm::USBCDCACMInstance *usb_cdc_parent_;

  // YAML framing, captured at setup; the host's line coding overwrites the UART's
  // settings, so pause() needs the original to restore.
  uint32_t configured_baud_rate_{0};
  uart::UARTParityOptions configured_parity_{uart::UART_CONFIG_PARITY_NONE};
  uint8_t configured_stop_bits_{0};
  uint8_t configured_data_bits_{0};

  // Written on the main loop, read by both worker tasks. uint8_t rather than bool:
  // GCC on Xtensa emits an out-of-line call for atomic<bool>.
  std::atomic<uint8_t> paused_{0};
  // Raised by the RX task while parked and by the TX task around each UART write, so
  // is_paused() reports when the bus is actually free.
  std::atomic<uint8_t> rx_parked_{0};
  std::atomic<uint8_t> tx_busy_{0};
  bool reload_pending_{false};
  // True once the host has sent any line coding; resume() then re-syncs to it.
  bool host_coding_seen_{false};
};

}  // namespace esphome::usb_uart_bridge
#endif
