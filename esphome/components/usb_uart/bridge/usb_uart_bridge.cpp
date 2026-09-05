#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "usb_uart_bridge.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "soc/soc_caps.h"

#include "tinyusb_cdc_acm.h"

namespace esphome::usb_uart_bridge {

static const char *const TAG = "usb_uart_bridge";

static constexpr size_t USB_TASK_STACK_SIZE = 4096;
static constexpr size_t USB_TASK_STACK_SIZE_VV = 8192;
static constexpr size_t RINGBUF_RETRY_CHUNK_SIZE = 64;
static constexpr uint32_t LOG_THROTTLE_MS = 1000;
static constexpr uint32_t UART_RELOAD_SETTLE_MS = 20;
// Above the default priority but below the USB/Wi-Fi system tasks.
static constexpr UBaseType_t TASK_PRIORITY = 4;

static bool should_log_now(uint32_t *last_ms, uint32_t interval_ms) {
  uint32_t now = millis();
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
  // Line state starts deasserted (no host yet); active-low DTR#/RTS# wiring is
  // handled by configuring the pins inverted, so deasserted idles HIGH.
  if (this->dtr_pin_ != nullptr) {
    this->dtr_pin_->setup();
    this->dtr_pin_->digital_write(false);
  }

  if (this->rts_pin_ != nullptr) {
    this->rts_pin_->setup();
    this->rts_pin_->digital_write(false);
  }

  // A failed UART never assigned its port number, so the worker tasks would run
  // against an indeterminate port.
  if (this->uart_parent_->is_failed()) {
    ESP_LOGE(TAG, "UART parent failed; aborting");
    this->mark_failed();
    return;
  }

  this->configured_baud_rate_ = this->uart_parent_->get_baud_rate();
  this->configured_parity_ = this->uart_parent_->get_parity();
  this->configured_stop_bits_ = this->uart_parent_->get_stop_bits();
  this->configured_data_bits_ = this->uart_parent_->get_data_bits();

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

  // The tasks dereference these handles/ring buffers immediately; usb_cdc_acm sets
  // up first (priority IO > HARDWARE), so a null here means its setup failed.
  this->usb_tx_task_handle_ = this->usb_cdc_parent_->get_tx_task_handle();
  if (this->usb_tx_task_handle_ == nullptr || this->usb_cdc_parent_->get_rx_ringbuf() == nullptr ||
      this->usb_cdc_parent_->get_tx_ringbuf() == nullptr) {
    ESP_LOGE(TAG, "USB CDC ACM not ready; aborting");
    this->mark_failed();
    return;
  }

  // Larger stack for the very-verbose hex-dump logging path.
  constexpr size_t stack_size =
      ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE ? USB_TASK_STACK_SIZE_VV : USB_TASK_STACK_SIZE;

  // Per-instance task names (keyed on the CDC interface number) keep task dumps
  // unambiguous with multiple bridges.
  char tx_task_name[] = "usb_uart_tx_0";
  char rx_task_name[] = "uart_usb_rx_0";
  const char itf_char = format_hex_char(static_cast<char>(this->usb_cdc_parent_->get_itf()));
  tx_task_name[sizeof(tx_task_name) - 2] = itf_char;
  rx_task_name[sizeof(rx_task_name) - 2] = itf_char;

  xTaskCreate(uart_tx_task_fn, tx_task_name, stack_size, this, TASK_PRIORITY, &this->uart_tx_task_handle_);
  if (this->uart_tx_task_handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create UART TX task");
    this->mark_failed();
    return;
  }

  xTaskCreate(uart_rx_task_fn, rx_task_name, stack_size, this, TASK_PRIORITY, &this->uart_rx_task_handle_);
  if (this->uart_rx_task_handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create UART RX task");
    vTaskDelete(this->uart_tx_task_handle_);
    this->uart_tx_task_handle_ = nullptr;
    this->mark_failed();
    return;
  }

  // Only register callbacks once both tasks exist, so a failed setup never drives
  // DTR/RTS from a dead bridge.
  this->usb_cdc_parent_->set_line_state_callback([this](bool dtr, bool rts) { this->set_line_state(dtr, rts); });
  this->usb_cdc_parent_->set_line_coding_callback([this](uint32_t, uint8_t, uint8_t, uint8_t) {
    this->host_coding_seen_ = true;
    this->set_line_coding();
  });

  // loop() only services line-coding reloads; stay off the main loop until one is
  // scheduled.
  this->disable_loop();
}

void USBUARTBridge::dump_config() {
  ESP_LOGCONFIG(TAG, "USB-UART Bridge:");
  LOG_PIN("  DTR Pin: ", this->dtr_pin_);
  LOG_PIN("  RTS Pin: ", this->rts_pin_);
  ESP_LOGCONFIG(TAG, "  UART Bus: %u", this->uart_parent_->get_hw_serial_number());
  ESP_LOGCONFIG(TAG, "  USB CDC Interface: %u", static_cast<uint8_t>(this->usb_cdc_parent_->get_itf()));
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
  // Another component owns the UART's framing while paused; resume() re-syncs.
  if (this->paused_ != 0) {
    return;
  }

  // usb_cdc_acm has already translated the wire coding onto the CDC instance (main
  // loop); mirror it here so the framing translation has a single source of truth.
  bool changed = false;

  // Reject 0 (the CDC B0/hang-up encoding; older IDF revisions divide by the rate)
  // and rates above the SoC ceiling. Anything in between is the driver's call,
  // matching what a YAML-configured UART accepts.
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

  // USB CDC permits data-bit counts the UART cannot represent (up to 16).
  const uint8_t data_bits = this->usb_cdc_parent_->get_data_bits();
  if (data_bits < 5 || data_bits > 8) {
    ESP_LOGW(TAG, "Ignoring unsupported data bits %u from host; keeping %u", data_bits,
             this->uart_parent_->get_data_bits());
  } else if (this->uart_parent_->get_data_bits() != data_bits) {
    this->uart_parent_->set_data_bits(data_bits);
    changed = true;
  }

  if (changed) {
    ESP_LOGV(TAG, "Line coding: baud=%" PRIu32 ", data_bits=%u, stop_bits=%u, parity=%u",
             this->uart_parent_->get_baud_rate(), this->uart_parent_->get_data_bits(),
             this->uart_parent_->get_stop_bits(), static_cast<uint8_t>(this->uart_parent_->get_parity()));
    // Coalesce rapid line-coding updates from the host.
    this->reload_requested_at_ = millis();
    this->reload_pending_ = true;
    // Main-loop context (via USBCDCACMInstance::process_events_).
    this->enable_loop();
  }
}

void USBUARTBridge::pause() {
  if (this->paused_ != 0) {
    return;
  }
  this->paused_ = 1;
  // A null RX task means setup() has not completed (or failed): nothing to stop, and
  // the framing snapshot does not exist yet. The RX task starts parked.
  if (this->uart_rx_task_handle_ == nullptr) {
    return;
  }
  // A coalesced host reload must not land once another component owns the bus.
  this->reload_pending_ = false;
  this->disable_loop();
  this->restore_configured_framing_();
}

void USBUARTBridge::resume() {
  if (this->paused_ == 0) {
    return;
  }
  this->paused_ = 0;
  if (this->uart_rx_task_handle_ == nullptr) {
    return;
  }
  xTaskNotifyGive(this->uart_rx_task_handle_);
  if (this->host_coding_seen_) {
    this->set_line_coding();
  }
}

void USBUARTBridge::restore_configured_framing_() {
  if (this->uart_parent_->get_baud_rate() == this->configured_baud_rate_ &&
      this->uart_parent_->get_parity() == this->configured_parity_ &&
      this->uart_parent_->get_stop_bits() == this->configured_stop_bits_ &&
      this->uart_parent_->get_data_bits() == this->configured_data_bits_) {
    return;
  }
  this->uart_parent_->set_baud_rate(this->configured_baud_rate_);
  this->uart_parent_->set_parity(this->configured_parity_);
  this->uart_parent_->set_stop_bits(this->configured_stop_bits_);
  this->uart_parent_->set_data_bits(this->configured_data_bits_);
  this->uart_settings_reload_();
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
  uart_port_t uart_num = static_cast<uart_port_t>(this->uart_parent_->get_hw_serial_number());
  // Back-dated so a problem within the first LOG_THROTTLE_MS of uptime still logs.
  uint32_t tx_full_log_ms = millis() - LOG_THROTTLE_MS;
  uint32_t err_log_ms = millis() - LOG_THROTTLE_MS;

  uint8_t *data = this->uart_rx_buffer_.get();
  const size_t buf_size = this->uart_rx_buffer_size_;

  while (true) {
    if (this->paused_ != 0) {
      // Parked until resume() notifies; nothing is read, so the other owner sees
      // every byte.
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      continue;
    }

    // Block until at least one byte is available from UART.
    int total_rx_size = uart_read_bytes(uart_num, data, 1, pdMS_TO_TICKS(UART_RX_WAIT_MS));
    if (total_rx_size < 0) {
      if (should_log_now(&err_log_ms, LOG_THROTTLE_MS)) {
        ESP_LOGE(TAG, "UART read failed: %d", total_rx_size);
      }
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    if (total_rx_size == 0) {
      continue;
    }
    // pause() landed during the read: don't forward a byte to a host that is gone.
    if (this->paused_ != 0) {
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
  uart_port_t uart_num = static_cast<uart_port_t>(this->uart_parent_->get_hw_serial_number());
  uint8_t *data_to_uart = this->uart_tx_buffer_.get();
  const size_t buf_size = this->uart_tx_buffer_size_;
  size_t rx_size;
  // Back-dated so a problem within the first LOG_THROTTLE_MS of uptime still logs.
  uint32_t err_log_ms = millis() - LOG_THROTTLE_MS;
  uint32_t drop_log_ms = millis() - LOG_THROTTLE_MS;

  while (true) {
    ESP_LOGV(TAG, "Waiting for data to send to UART");
    esp_err_t ret = usb_cdc_acm::ringbuf_read_bytes(usb_rx_ringbuf, data_to_uart, buf_size, &rx_size, portMAX_DELAY);

    if (ret != ESP_OK) {
      if (should_log_now(&err_log_ms, LOG_THROTTLE_MS)) {
        ESP_LOGE(TAG, "USB RX RingBuf read failed");
      }
      // Yield: this task runs above the main loop, so a persistent failure must not
      // become a tight loop.
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    // Another component owns the UART; host bytes must not interleave with its traffic.
    if (this->paused_ != 0) {
      if (should_log_now(&drop_log_ms, LOG_THROTTLE_MS)) {
        ESP_LOGW(TAG, "Paused; dropping %zu bytes from host", rx_size);
      }
      continue;
    }

    ESP_LOGV(TAG, "Sending %zu bytes to UART", rx_size);
    // Signed: uart_write_bytes() returns -1 on error.
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
  // apply_settings_live() rewrites the framing registers without reinstalling the
  // driver, so the worker tasks blocked inside it are undisturbed. Runs on the main
  // loop (see loop()), matching the IDF UART component's threading contract.
  this->uart_parent_->apply_settings_live();
}

}  // namespace esphome::usb_uart_bridge
#endif
