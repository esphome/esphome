#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "usb_uart_bridge.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "soc/soc_caps.h"
#include "esp_log.h"

#include "tinyusb_cdc_acm.h"

namespace esphome::usb_uart_bridge {

static const char *const TAG = "usb_uart_bridge";

static constexpr size_t USB_TASK_STACK_SIZE = 4096;
static constexpr size_t USB_TASK_STACK_SIZE_VV = 8192;
static constexpr size_t RINGBUF_RETRY_CHUNK_SIZE = 64;
static constexpr uint32_t LOG_THROTTLE_MS = 1000;
static constexpr uint32_t UART_RELOAD_SETTLE_MS = 20;
// Priority for the RX/TX worker tasks; above the default but below the USB/Wi-Fi
// system tasks so the bridge stays responsive without starving the stack.
static constexpr UBaseType_t TASK_PRIORITY = 4;

static bool should_log_now(uint32_t *last_ms, uint32_t interval_ms) {
  uint32_t now = esp_log_timestamp();
  if ((now - *last_ms) >= interval_ms) {
    *last_ms = now;
    return true;
  }
  return false;
}

static bool ringbuf_send_with_retry(RingbufHandle_t ringbuf, const uint8_t *data, size_t len, uint32_t *log_ms) {
  if (len == 0) {
    return true;
  }

  if (xRingbufferSend(ringbuf, data, len, pdMS_TO_TICKS(1)) == pdTRUE) {
    return true;
  }

  size_t offset = 0;
  while (offset < len) {
    size_t chunk = std::min(RINGBUF_RETRY_CHUNK_SIZE, len - offset);
    if (xRingbufferSend(ringbuf, data + offset, chunk, pdMS_TO_TICKS(1)) != pdTRUE) {
      if (should_log_now(log_ms, LOG_THROTTLE_MS)) {
        ESP_LOGW(TAG, "USB TX buffer full; some data is lost");
      }
      return false;
    }
    offset += chunk;
  }
  return true;
}

void USBUARTBridge::setup() {
  // DTR/RTS carry the host's logical line state: true = asserted, false = deasserted
  // (matching the USB CDC SET_CONTROL_LINE_STATE bits and set_line_state() below).
  // Initialize them deasserted, since no host has raised them yet. For a conventional
  // active-low serial interface (DTR#/RTS#), configure the pins with `inverted: true`
  // so deasserted idles HIGH and an assertion drives LOW, as a real adapter would.
  if (this->dtr_pin_ != nullptr) {
    this->dtr_pin_->setup();
    this->dtr_pin_->digital_write(false);
  }

  if (this->rts_pin_ != nullptr) {
    this->rts_pin_->setup();
    this->rts_pin_->digital_write(false);
  }

  if (this->uart_parent_ == nullptr) {
    ESP_LOGE(TAG, "UART parent not set");
    this->mark_failed();
    return;
  }

  // A failed UART never assigned its port number, so the worker tasks would run
  // against an indeterminate port -- possibly a valid one owned by someone else.
  if (this->uart_parent_->is_failed()) {
    ESP_LOGE(TAG, "UART parent failed; aborting");
    this->mark_failed();
    return;
  }

  if (this->usb_cdc_parent_ == nullptr) {
    ESP_LOGE(TAG, "USB CDC ACM parent not set");
    this->mark_failed();
    return;
  }

  this->uart_rx_buffer_.reset(new (std::nothrow) uint8_t[this->uart_rx_buffer_size_]);
  if (this->uart_rx_buffer_ == nullptr) {
    ESP_LOGE(TAG, "UART RX buffer allocation failed");
    this->mark_failed();
    return;
  }

  this->uart_tx_buffer_.reset(new (std::nothrow) uint8_t[this->uart_tx_buffer_size_]);
  if (this->uart_tx_buffer_ == nullptr) {
    ESP_LOGE(TAG, "UART TX buffer allocation failed");
    this->mark_failed();
    return;
  }

  // Validate the USB CDC side is fully initialized before spawning tasks. The tasks
  // dereference these handles/ring buffers immediately (uart_tx_task_ blocks in
  // xRingbufferReceive() on the RX ringbuf; uart_rx_task_ notifies the TX task handle).
  // usb_cdc_acm sets up first (priority IO > HARDWARE), so a null here means its setup
  // failed -- abort rather than run those primitives against null handles.
  this->usb_tx_task_handle_ = this->usb_cdc_parent_->get_tx_task_handle();
  if (this->usb_tx_task_handle_ == nullptr || this->usb_cdc_parent_->get_rx_ringbuf() == nullptr ||
      this->usb_cdc_parent_->get_tx_ringbuf() == nullptr) {
    ESP_LOGE(TAG, "USB CDC ACM not ready; aborting");
    this->mark_failed();
    return;
  }

  // Use a larger stack size for very verbose (hex dump) logging. Gate on the
  // compile-time level, matching the buffers that are only compiled in at VV.
  constexpr size_t stack_size =
      ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE ? USB_TASK_STACK_SIZE_VV : USB_TASK_STACK_SIZE;

  // Unique per-instance task names (keyed on the CDC interface number) so multiple
  // bridge instances don't produce ambiguous duplicates in task dumps/backtraces.
  char tx_task_name[] = "usb_uart_tx_0";
  char rx_task_name[] = "uart_usb_rx_0";
  const char itf_char = format_hex_char(static_cast<char>(this->usb_cdc_parent_->get_itf()));
  tx_task_name[sizeof(tx_task_name) - 2] = itf_char;
  rx_task_name[sizeof(rx_task_name) - 2] = itf_char;

  // Create task that reads from USB CDC RX buffer and writes to UART
  xTaskCreate(uart_tx_task_fn, tx_task_name, stack_size, this, TASK_PRIORITY, &this->uart_tx_task_handle_);
  if (this->uart_tx_task_handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create UART TX task");
    this->mark_failed();
    return;
  }

  // Create task that reads from UART and writes to USB CDC TX buffer
  xTaskCreate(uart_rx_task_fn, rx_task_name, stack_size, this, TASK_PRIORITY, &this->uart_rx_task_handle_);
  if (this->uart_rx_task_handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create UART RX task");
    // Tear down the TX task we already created so it isn't left running on a failed component.
    vTaskDelete(this->uart_tx_task_handle_);
    this->uart_tx_task_handle_ = nullptr;
    this->mark_failed();
    return;
  }

  // Register the line state/coding callbacks only after both tasks exist, so a failed
  // task creation above never leaves callbacks bound to a component that won't run --
  // set_line_state() would otherwise still drive the DTR/RTS GPIOs from a dead bridge.
  this->usb_cdc_parent_->set_line_state_callback([this](bool dtr, bool rts) { this->set_line_state(dtr, rts); });
  this->usb_cdc_parent_->set_line_coding_callback(
      [this](uint32_t, uint8_t, uint8_t, uint8_t) { this->set_line_coding(); });

  // loop() only services pending line-coding reloads; stay off the main loop until
  // set_line_coding() schedules one.
  this->disable_loop();
}

void USBUARTBridge::dump_config() {
  ESP_LOGCONFIG(TAG, "USB-UART Bridge:");
  LOG_PIN("  DTR Pin: ", this->dtr_pin_);
  LOG_PIN("  RTS Pin: ", this->rts_pin_);
  if (this->uart_parent_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  UART Bus: %u", this->uart_parent_->get_hw_serial_number());
  }
  if (this->usb_cdc_parent_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  USB CDC Interface: %u", static_cast<uint8_t>(this->usb_cdc_parent_->get_itf()));
  }
}

void USBUARTBridge::loop() {
  if (!this->reload_pending_) {
    return;
  }
  if ((millis() - this->reload_requested_at_) < UART_RELOAD_SETTLE_MS) {
    return;
  }

  this->reload_pending_ = false;
  this->uart_settings_reload_();
  this->disable_loop();
}

void USBUARTBridge::set_line_coding() {
  if (this->uart_parent_ == nullptr || this->usb_cdc_parent_ == nullptr) {
    return;
  }

  // usb_cdc_acm has already translated the raw USB line coding into UARTComponent
  // values on the CDC instance (in process_events_, main loop) before invoking this
  // callback. Mirror those onto the hardware UART rather than re-deriving them here --
  // a single source of truth keeps the framing translation from drifting (and is how
  // the earlier parity/stop-bit mapping bugs crept in).
  bool changed = false;

  // The baud rate arrives unvalidated from the wire. Reject dwDTERate = 0 (the CDC
  // "B0"/hang-up encoding) here -- older IDF revisions divide by the requested rate
  // in the clock-divider computation -- and anything above the SoC's ceiling. Rates
  // in between are the driver's call: it cleanly refuses rates its dividers cannot
  // reach (logged when the reload runs), so legacy low rates that the hardware can
  // do are passed through just like a YAML-configured UART would accept them.
  const uint32_t baud = this->usb_cdc_parent_->get_baud_rate();
  if (baud == 0 || baud > SOC_UART_BITRATE_MAX) {
    ESP_LOGW(TAG, "Ignoring unsupported baud rate %" PRIu32 " from host; keeping %" PRIu32, baud,
             this->uart_parent_->get_baud_rate());
  } else if (this->uart_parent_->get_baud_rate() != baud) {
    this->uart_parent_->set_baud_rate(baud);
    changed = true;
  }

  const uint8_t stop_bits = this->usb_cdc_parent_->get_stop_bits();
  if (this->uart_parent_->get_stop_bits() != stop_bits) {
    this->uart_parent_->set_stop_bits(stop_bits);
    changed = true;
  }

  const auto parity = this->usb_cdc_parent_->get_parity();
  if (this->uart_parent_->get_parity() != parity) {
    this->uart_parent_->set_parity(parity);
    changed = true;
  }

  // data_bits is stored raw on the CDC instance and may be a value the UART cannot
  // represent (USB CDC permits 16); only 5-8 are valid, so ignore anything else.
  const uint8_t data_bits = this->usb_cdc_parent_->get_data_bits();
  if (data_bits < 5 || data_bits > 8) {
    ESP_LOGW(TAG, "Ignoring unsupported data bits %u from host; keeping %u", data_bits,
             this->uart_parent_->get_data_bits());
  } else if (this->uart_parent_->get_data_bits() != data_bits) {
    this->uart_parent_->set_data_bits(data_bits);
    changed = true;
  }

  if (changed) {
    ESP_LOGV(TAG, "Line coding: baud=%" PRIu32 ", data_bits=%u, stop_bits=%u, parity=%u", baud,
             this->uart_parent_->get_data_bits(), this->uart_parent_->get_stop_bits(),
             static_cast<uint8_t>(this->uart_parent_->get_parity()));
    // Coalesce rapid line-coding updates from host.
    this->reload_requested_at_ = millis();
    this->reload_pending_ = true;
    // Runs on the main loop (via USBCDCACMInstance::process_events_), so the plain
    // enable is correct here.
    this->enable_loop();
  }
}

void USBUARTBridge::set_line_state(bool dtr, bool rts) {
  ESP_LOGV(TAG, "Line state: DTR=%d, RTS=%d", dtr, rts);
  if (this->dtr_pin_ != nullptr) {
    this->dtr_pin_->digital_write(dtr);
  }
  if (this->rts_pin_ != nullptr) {
    this->rts_pin_->digital_write(rts);
  }
}

void USBUARTBridge::uart_rx_task_fn(void *arg) {
  auto *bridge = static_cast<USBUARTBridge *>(arg);
  bridge->uart_rx_task_();
}

void USBUARTBridge::uart_tx_task_fn(void *arg) {
  auto *bridge = static_cast<USBUARTBridge *>(arg);
  bridge->uart_tx_task_();
}

void USBUARTBridge::uart_rx_task_() {
  TaskHandle_t usb_tx_handle = this->usb_tx_task_handle_;
  RingbufHandle_t usb_tx_ringbuf = this->usb_cdc_parent_->get_tx_ringbuf();
  uart_port_t uart_num = (uart_port_t) this->uart_parent_->get_hw_serial_number();
  uint32_t tx_full_log_ms = 0;
  uint32_t err_log_ms = 0;

  uint8_t *data = this->uart_rx_buffer_.get();
  const size_t buf_size = this->uart_rx_buffer_size_;

  while (true) {
    // Block until at least one byte is available from UART.
    int total_rx_size = uart_read_bytes(uart_num, data, 1, portMAX_DELAY);
    if (total_rx_size < 0) {
      // Throttled: a persistently failing driver would otherwise flood at 100 Hz.
      if (should_log_now(&err_log_ms, LOG_THROTTLE_MS)) {
        ESP_LOGE(TAG, "UART read failed: %d", total_rx_size);
      }
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    if (total_rx_size == 0) {
      continue;
    }

    // Drain the currently buffered burst without waiting.
    while (true) {
      int rx_data_size = uart_read_bytes(uart_num, data + total_rx_size, buf_size - total_rx_size, 0);
      ESP_LOGV(TAG, "UART RX: %d bytes", rx_data_size);
      if (rx_data_size == 0) {
        break;
      }
      if (rx_data_size < 0) {
        if (should_log_now(&err_log_ms, LOG_THROTTLE_MS)) {
          ESP_LOGE(TAG, "UART read failed: %d", rx_data_size);
        }
        break;
      }
      total_rx_size += rx_data_size;
      if (total_rx_size >= (int) buf_size) {
        break;
      }
    }

    ringbuf_send_with_retry(usb_tx_ringbuf, data, total_rx_size, &tx_full_log_ms);

    ESP_LOGV(TAG, "UART RX: waking up USB TX task");
    xTaskNotifyGive(usb_tx_handle);
  }
}

void USBUARTBridge::uart_tx_task_() {
  RingbufHandle_t usb_rx_ringbuf = this->usb_cdc_parent_->get_rx_ringbuf();
  uart_port_t uart_num = (uart_port_t) this->uart_parent_->get_hw_serial_number();
  uint8_t *data_to_uart = this->uart_tx_buffer_.get();
  const size_t buf_size = this->uart_tx_buffer_size_;
  size_t rx_size;
  uint32_t err_log_ms = 0;

  while (true) {
    ESP_LOGV(TAG, "Waiting for data to send to UART");
    esp_err_t ret = usb_cdc_acm::ringbuf_read_bytes(usb_rx_ringbuf, data_to_uart, buf_size, &rx_size, portMAX_DELAY);

    if (ret != ESP_OK) {
      // Throttled: a broken ring buffer would otherwise make this a hot error loop.
      if (should_log_now(&err_log_ms, LOG_THROTTLE_MS)) {
        ESP_LOGE(TAG, "USB RX RingBuf read failed");
      }
      continue;
    }

    ESP_LOGV(TAG, "Sending %zu bytes to UART", rx_size);
    // uart_write_bytes() returns int (-1 on error); keep it signed so a failure isn't
    // turned into SIZE_MAX by an implicit conversion.
    int xfer_size = uart_write_bytes(uart_num, data_to_uart, rx_size);

    if (xfer_size < 0) {
      if (should_log_now(&err_log_ms, LOG_THROTTLE_MS)) {
        ESP_LOGE(TAG, "UART write failed: %d", xfer_size);
      }
    } else if (static_cast<size_t>(xfer_size) != rx_size) {
      ESP_LOGW(TAG, "UART write incomplete (%d/%zu bytes)", xfer_size, rx_size);
    }
  }
}

void USBUARTBridge::uart_settings_reload_() {
  if (this->uart_parent_ != nullptr) {
    // Apply the new line coding to the live driver. Unlike load_settings(), which
    // deletes and reinstalls the driver, this only rewrites the framing registers and
    // leaves the RX/TX ring buffers intact — so the RX/TX tasks blocked in
    // uart_read_bytes()/uart_write_bytes() are never disturbed and there is no
    // use-after-free risk. Runs in the main loop (see loop()), matching the IDF UART
    // component's main-loop-only threading contract.
    this->uart_parent_->apply_settings_live();
  }
}

}  // namespace esphome::usb_uart_bridge
#endif
