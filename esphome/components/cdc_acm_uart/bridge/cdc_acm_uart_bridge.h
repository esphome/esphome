#pragma once
#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "esphome/components/uart/uart_component_esp_idf.h"
#include "esphome/components/usb_cdc_acm/usb_cdc_acm.h"
#include "esphome/core/component.h"

#include <array>
#include <atomic>
#include "sdkconfig.h"

namespace esphome::cdc_acm_uart {

class CDCACMUARTBridge final : public Component {
 public:
  // Upper bound on the RX task's blocking read, so pause() takes effect without
  // aborting the read. Arriving bytes still unblock it immediately.
  static constexpr uint32_t UART_RX_WAIT_MS = 250;

  CDCACMUARTBridge(uart::IDFUARTComponent *uart_parent, usb_cdc_acm::USBCDCACMInstance *usb_cdc_parent)
      : uart_parent_(uart_parent), usb_cdc_parent_(usb_cdc_parent) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_dtr_pin(GPIOPin *dtr_pin) { this->dtr_pin_ = dtr_pin; }
  void set_rts_pin(GPIOPin *rts_pin) { this->rts_pin_ = rts_pin; }

  void set_line_coding();
  void set_line_state(bool dtr, bool rts);
  uart::IDFUARTComponent *get_uart_parent() const { return this->uart_parent_; }

  /**
   * Stop forwarding in both directions and hand the UART back to its configured
   * framing, so another component may use the bus. Main-loop only. The RX task parks
   * within UART_RX_WAIT_MS (a byte it was already reading is discarded). A host write
   * already in flight is allowed to drain first, which at low baud rates can take
   * seconds; the framing is restored only after that, so poll is_paused() rather than
   * waiting a fixed interval. Host bytes not yet written to the UART are discarded.
   * The DTR/RTS outputs hold their state while paused and follow the host again on
   * resume().
   */
  void pause();
  /**
   * Re-apply the host's line coding and line state, then resume forwarding. Main-loop
   * only. Deferred until any host write still draining has finished, so the reload
   * never truncates it.
   */
  void resume();
  /// True once both worker tasks are off the bus and the configured framing is restored.
  /// With no RX task (setup() failed or has not run) there is nothing to wait for.
  bool is_paused() const {
    return this->state_ == MainState::MAIN_STATE_PAUSED &&
           (this->uart_rx_task_handle_ == nullptr || this->rx_parked_ != 0);
  }

 protected:
  static void uart_rx_task_fn(void *arg);
  static void uart_tx_task_fn(void *arg);
  void uart_rx_task_();
  void uart_tx_task_();
  void restore_configured_framing_();
  // True when the TX task has no write in flight and the UART TX FIFO has drained.
  bool tx_idle_();
  void finish_resume_();
  void drive_line_state_();
  // Copy the host's line coding onto the UART settings; true if anything changed.
  bool sync_host_framing_();

  TaskHandle_t uart_rx_task_handle_{nullptr};
  TaskHandle_t uart_tx_task_handle_{nullptr};

  GPIOPin *dtr_pin_{nullptr};
  GPIOPin *rts_pin_{nullptr};

  uint32_t reload_requested_at_{0};

  // Worker staging, each sized to the CDC ring buffer it feeds or drains.
  std::array<uint8_t, CONFIG_TINYUSB_CDC_TX_BUFSIZE> uart_rx_buffer_{};
  std::array<uint8_t, CONFIG_TINYUSB_CDC_RX_BUFSIZE> uart_tx_buffer_{};

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
  // the pause hand-off knows when the bus is actually free.
  std::atomic<uint8_t> rx_parked_{0};
  std::atomic<uint8_t> tx_busy_{0};
  // Main-loop state; paused_ mirrors it for the worker tasks.
  enum class MainState : uint8_t {
    MAIN_STATE_RUNNING,
    MAIN_STATE_RELOAD_PENDING,  // host line coding debounced, forwarding continues
    MAIN_STATE_PAUSING,         // waiting for TX idle to restore the configured framing
    MAIN_STATE_PAUSED,
    MAIN_STATE_RESUMING,  // resume() requested while a host write still drains
  };
  MainState state_{MainState::MAIN_STATE_RUNNING};
  // Host line state, recorded even while paused so resume() can re-drive the pins.
  bool host_dtr_{false};
  bool host_rts_{false};
  // True once the host has sent any line coding; resume() then re-syncs to it.
  bool host_coding_seen_{false};
};

}  // namespace esphome::cdc_acm_uart
#endif
