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
#include "esp_log.h"

#include "tinyusb_cdc_acm.h"

namespace esphome::usb_uart_bridge {

static constexpr const char *TAG = "usb_uart_bridge";

static constexpr size_t USB_TASK_STACK_SIZE = 4096;
static constexpr size_t USB_TASK_STACK_SIZE_VV = 8192;
static constexpr size_t RINGBUF_RETRY_CHUNK_SIZE = 64;
static constexpr uint32_t LOG_THROTTLE_MS = 1000;
static constexpr uint32_t UART_RELOAD_SETTLE_MS = 20;

/*
 * uint8_t  stop_bits; ///< 0: 1 stop bit - 1: 1.5 stop bits - 2: 2 stop bits
 * uint8_t  parity;    ///< 0: None - 1: Odd - 2: Even - 3: Mark - 4: Space
 * uint8_t  data_bits; ///< can be 5, 6, 7, 8 or 16
 * */

static const char *str_stop_bits(uint8_t s) {
  return (s == 2) ? "UART_STOP_BITS_2" : (s == 1) ? "UART_STOP_BITS_1_5" : "UART_STOP_BITS_1";
}
static const char *str_parity(uint8_t p) {
  return (p == 2) ? "UART_PARITY_EVEN" : (p == 1) ? "UART_PARITY_ODD" : "UART_PARITY_DISABLE";
}
static const char *str_data_bits(uint8_t b) {
  return (b == 5)   ? "UART_DATA_5_BITS"
         : (b == 6) ? "UART_DATA_6_BITS"
         : (b == 7) ? "UART_DATA_7_BITS"
                    : "UART_DATA_8_BITS";
}

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

static esp_err_t ringbuf_read_bytes(RingbufHandle_t ring_buf, uint8_t *out_buf, size_t out_buf_sz, size_t *rx_data_size,
                                    TickType_t x_ticks_to_wait) {
  size_t read_sz;
  uint8_t *buf = static_cast<uint8_t *>(xRingbufferReceiveUpTo(ring_buf, &read_sz, x_ticks_to_wait, out_buf_sz));

  if (buf == nullptr) {
    return ESP_FAIL;
  }

  memcpy(out_buf, buf, read_sz);
  vRingbufferReturnItem(ring_buf, (void *) buf);
  *rx_data_size = read_sz;

  // Buffer's data can be wrapped, in which case we should perform another read
  buf = static_cast<uint8_t *>(xRingbufferReceiveUpTo(ring_buf, &read_sz, 0, out_buf_sz - *rx_data_size));
  if (buf != nullptr) {
    memcpy(out_buf + *rx_data_size, buf, read_sz);
    vRingbufferReturnItem(ring_buf, (void *) buf);
    *rx_data_size += read_sz;
  }

  return ESP_OK;
}

void USBUARTBridge::setup() {
  if (this->dtr_pin_ != nullptr) {
    this->dtr_pin_->setup();
    this->dtr_pin_->digital_write(true);
  }

  if (this->rts_pin_ != nullptr) {
    this->rts_pin_->setup();
    this->rts_pin_->digital_write(true);
  }

  if (this->uart_parent_ == nullptr) {
    ESP_LOGE(TAG, "UART parent not set");
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

  // Register line state and line coding callbacks with the USB CDC ACM component
  this->usb_cdc_parent_->set_line_state_callback([this](bool dtr, bool rts) { this->set_line_state(dtr, rts); });
  this->usb_cdc_parent_->set_line_coding_callback(
      [this](uint32_t bit_rate, uint8_t stop_bits, uint8_t parity, uint8_t data_bits) {
        this->set_line_coding(bit_rate, stop_bits, parity, data_bits);
      });

  // Use a larger stack size for (very) verbose logging
  const size_t stack_size = esp_log_level_get(TAG) > ESP_LOG_DEBUG ? USB_TASK_STACK_SIZE_VV : USB_TASK_STACK_SIZE;

  // Create task that reads from USB CDC RX buffer and writes to UART
  xTaskCreate(uart_tx_task_fn, "usb_uart_tx", stack_size, this, 4, &this->uart_tx_task_handle_);
  if (this->uart_tx_task_handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create UART TX task");
    this->mark_failed();
    return;
  }

  // Create task that reads from UART and writes to USB CDC TX buffer
  this->usb_tx_task_handle_ = this->usb_cdc_parent_->get_tx_task_handle();
  if (this->usb_tx_task_handle_ == nullptr) {
    // usb_cdc_acm sets up first (priority IO > HARDWARE); a null handle means its
    // setup failed. The RX task notifies this handle on every burst, so abort
    // rather than call xTaskNotifyGive(nullptr).
    ESP_LOGE(TAG, "USB CDC TX task not available; aborting");
    this->mark_failed();
    return;
  }
  xTaskCreate(uart_rx_task_fn, "uart_usb_rx", stack_size, this, 4, &this->uart_rx_task_handle_);
  if (this->uart_rx_task_handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create UART RX task");
    this->mark_failed();
    return;
  }
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
}

void USBUARTBridge::set_line_coding(uint32_t bit_rate, uint8_t stop_bits, uint8_t parity, uint8_t data_bits) {
  if (this->uart_parent_ == nullptr) {
    return;
  }

  bool changed = false;

  if (this->uart_parent_->get_baud_rate() != bit_rate) {
    this->uart_parent_->set_baud_rate(bit_rate);
    changed = true;
    ESP_LOGV(TAG, "Baud rate changed to %" PRIu32, bit_rate);
  }

  auto uart_stop_bits = (stop_bits == 0 ? UART_STOP_BITS_1 : (stop_bits == 1 ? UART_STOP_BITS_1_5 : UART_STOP_BITS_2));
  if (this->uart_parent_->get_stop_bits() != uart_stop_bits) {
    this->uart_parent_->set_stop_bits(uart_stop_bits);
    changed = true;
    ESP_LOGV(TAG, "Stop bits changed to %s", str_stop_bits(stop_bits));
  }

  // USB CDC parity numbering (0=None, 1=Odd, 2=Even, 3=Mark, 4=Space) differs from
  // UARTParityOptions (0=None, 1=Even, 2=Odd). Map explicitly; Mark/Space are
  // unsupported by the UART and fall back to None.
  auto uart_parity = (parity == 1)   ? uart::UART_CONFIG_PARITY_ODD
                     : (parity == 2) ? uart::UART_CONFIG_PARITY_EVEN
                                     : uart::UART_CONFIG_PARITY_NONE;
  if (this->uart_parent_->get_parity() != uart_parity) {
    this->uart_parent_->set_parity(uart_parity);
    changed = true;
    ESP_LOGV(TAG, "Parity changed to %s", str_parity(parity));
  }

  if (this->uart_parent_->get_data_bits() != data_bits) {
    this->uart_parent_->set_data_bits(data_bits);
    changed = true;
    ESP_LOGV(TAG, "Data bits changed to %s", str_data_bits(data_bits));
  }

  if (changed) {
    // Coalesce rapid line-coding updates from host.
    this->reload_requested_at_ = millis();
    this->reload_pending_ = true;
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

  uint8_t *data = this->uart_rx_buffer_.get();
  const size_t buf_size = this->uart_rx_buffer_size_;

  while (true) {
    // Block until at least one byte is available from UART.
    int total_rx_size = uart_read_bytes(uart_num, data, 1, portMAX_DELAY);
    if (total_rx_size < 0) {
      ESP_LOGE(TAG, "UART read failed: %d", total_rx_size);
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
        ESP_LOGE(TAG, "UART read failed: %d", rx_data_size);
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

  while (true) {
    ESP_LOGV(TAG, "Waiting for data to send to UART");
    esp_err_t ret = ringbuf_read_bytes(usb_rx_ringbuf, data_to_uart, buf_size, &rx_size, portMAX_DELAY);

    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "USB RX RingBuf read failed");
      continue;
    }

    ESP_LOGV(TAG, "Sending %d bytes to UART", rx_size);
    size_t xfer_size = uart_write_bytes(uart_num, data_to_uart, rx_size);

    if (xfer_size != rx_size) {
      ESP_LOGW(TAG, "UART write incomplete (%d/%d bytes)", xfer_size, rx_size);
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
