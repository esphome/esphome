#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "usb_uart_bridge.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "tusb_cdc_acm.h"

#define STR_STOP_BITS(s) (((s) == 2) ? "UART_STOP_BITS_2" : (((s) == 1) ? "UART_STOP_BITS_1_5" : "UART_STOP_BITS_1"))
#define STR_PARITY(p) (((p) == 2) ? "UART_PARITY_EVEN" : (((p) == 1) ? "UART_PARITY_ODD" : "UART_PARITY_DISABLE"))
#define STR_DATA_BITS(b) \
  (((b) == 5) ? "UART_DATA_5_BITS" \
              : (((b) == 6) ? "UART_DATA_6_BITS" : (((b) == 7) ? "UART_DATA_7_BITS" : "UART_DATA_8_BITS")))

/*
 * uint32_t bit_rate;
 * uint8_t  stop_bits; ///< 0: 1 stop bit - 1: 1.5 stop bits - 2: 2 stop bits
 * uint8_t  parity;    ///< 0: None - 1: Odd - 2: Even - 3: Mark - 4: Space
 * uint8_t  data_bits; ///< can be 5, 6, 7, 8 or 16
 * */

namespace esphome::usb_uart_bridge {

static const char *TAG = "usb_uart_bridge";

static const uint8_t UART_RELOAD_DEBOUNCE_MS = 100;

static esp_err_t ringbuf_read_bytes(RingbufHandle_t ring_buf, uint8_t *out_buf, size_t out_buf_sz, size_t *rx_data_size,
                                    TickType_t xTicksToWait) {
  size_t read_sz;
  uint8_t *buf = static_cast<uint8_t *>(xRingbufferReceiveUpTo(ring_buf, &read_sz, xTicksToWait, out_buf_sz));

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

  // Register line state and line coding callbacks with the USB CDC ACM component
  this->usb_cdc_parent_->set_line_coding_callback(
      [this](uint32_t bit_rate, uint8_t stop_bits, uint8_t parity, uint8_t data_bits) {
        this->set_line_coding(bit_rate, stop_bits, parity, data_bits);
      });

  this->usb_cdc_parent_->set_line_state_callback([this](bool dtr, bool rts) { this->set_line_state(dtr, rts); });

  size_t stack_size = 4096;
  if (esp_log_level_get(TAG) > ESP_LOG_DEBUG) {
    stack_size = 8192;  // Increase stack size for debug logging
  }

  // Create task that reads from USB CDC RX buffer and writes to UART
  xTaskCreate(uart_tx_task_fn, "usb_uart_tx", stack_size, this, 4, &this->uart_tx_task_handle_);
  if (this->uart_tx_task_handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create UART TX task");
    this->mark_failed();
    return;
  }

  vTaskDelay(pdMS_TO_TICKS(100));

  // Create task that reads from UART and writes to USB CDC TX buffer
  this->uart_rx_task_param_ = {
      .usb_tx_handle = this->usb_cdc_parent_->get_tx_task_handle(),
      .uart_queue = *this->uart_parent_->get_uart_event_queue(),
  };
  xTaskCreate(uart_rx_task_fn, "uart_usb_rx", stack_size, this, 4, &this->uart_rx_task_handle_);
  if (this->uart_rx_task_handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create UART RX task");
    this->mark_failed();
    return;
  }
}

void USBUARTBridge::loop() {
  // UART settings changed only from main application task/context
  if (this->reload_uart_settings_ && this->uart_parent_ != nullptr) {
    // Debounce: only reload if at least UART_RELOAD_DEBOUNCE_MS since the request
    if (App.get_loop_component_start_time() - this->reload_uart_settings_ >= UART_RELOAD_DEBOUNCE_MS) {
      vTaskSuspend(this->uart_rx_task_handle_);
      vTaskSuspend(this->uart_tx_task_handle_);
      this->uart_parent_->load_settings(false);
      vTaskResume(this->uart_rx_task_handle_);
      vTaskResume(this->uart_tx_task_handle_);
      this->reload_uart_settings_ = 0;
    }
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
    ESP_LOGV(TAG, "Stop bits changed to %s", STR_STOP_BITS(stop_bits));
  }

  if (this->uart_parent_->get_parity() != parity) {
    this->uart_parent_->set_parity(static_cast<uart::UARTParityOptions>(parity));
    changed = true;
    ESP_LOGV(TAG, "Parity changed to %s", STR_PARITY(parity));
  }

  if (this->uart_parent_->get_data_bits() != data_bits) {
    this->uart_parent_->set_data_bits(data_bits);
    changed = true;
    ESP_LOGV(TAG, "Data bits changed to %s", STR_DATA_BITS(data_bits));
  }

  if (changed) {
    this->request_uart_settings_reload();
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
  bridge->uart_rx_task();
}

void USBUARTBridge::uart_tx_task_fn(void *arg) {
  auto *bridge = static_cast<USBUARTBridge *>(arg);
  bridge->uart_tx_task();
}

void USBUARTBridge::uart_rx_task() {
  TaskHandle_t usb_tx_handle = this->uart_rx_task_param_.usb_tx_handle;
  QueueHandle_t uart_queue = this->uart_rx_task_param_.uart_queue;
  RingbufHandle_t usb_tx_ringbuf = this->usb_cdc_parent_->get_tx_ringbuf();

  uint8_t data[USB_UART_BRIDGE_UART_RX_BUFFER_SIZE] = {0};
  uart_event_t event;

  while (1) {
    if (!xQueueReceive(uart_queue, (void *) &event, portMAX_DELAY)) {
      continue;
    }

    switch (event.type) {
      case UART_DATA:
        while (1) {
          const int rx_data_size = uart_read_bytes((uart_port_t) this->uart_parent_->get_hw_serial_number(), data,
                                                   MIN(USB_UART_BRIDGE_UART_RX_BUFFER_SIZE, event.size), 0);
          ESP_LOGV(TAG, "UART RX: %d bytes", rx_data_size);

          if (rx_data_size == 0) {
            // There's no more data to read, wake up USB TX task
            ESP_LOGV(TAG, "UART RX: waking up USB TX task");
            xTaskNotifyGive(usb_tx_handle);
            break;
          }

          if (rx_data_size < 0) {
            ESP_LOGE(TAG, "UART read failed: %d", rx_data_size);
            break;
          }

          // Send data to USB CDC TX ring buffer
          BaseType_t res = xRingbufferSend(usb_tx_ringbuf, data, rx_data_size, 0);
          if (res != pdTRUE) {
            ESP_LOGW(TAG, "USB TX buffer full; %d bytes lost", rx_data_size);
          }
        }
        break;

      default:
        ESP_LOGV(TAG, "UART event type: %d", event.type);
        break;
    }
  }
}

void USBUARTBridge::uart_tx_task() {
  RingbufHandle_t usb_rx_ringbuf = this->usb_cdc_parent_->get_rx_ringbuf();
  uint8_t data_to_uart[USB_UART_BRIDGE_UART_TX_BUFFER_SIZE];
  size_t rx_size;

  while (1) {
    ESP_LOGV(TAG, "Waiting for data to send to UART");
    esp_err_t ret = ringbuf_read_bytes(usb_rx_ringbuf, data_to_uart, sizeof(data_to_uart), &rx_size, portMAX_DELAY);

    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "USB RX RingBuf read failed");
      continue;
    }

    ESP_LOGV(TAG, "Sending %d bytes to UART", rx_size);
    size_t xfer_size =
        uart_write_bytes((uart_port_t) this->uart_parent_->get_hw_serial_number(), data_to_uart, rx_size);

    if (xfer_size != rx_size) {
      ESP_LOGW(TAG, "UART write incomplete (%d/%d bytes)", xfer_size, rx_size);
    }

    ESP_LOGV(TAG, "Waiting for UART TX to complete");
    esp_err_t result = uart_wait_tx_done((uart_port_t) this->uart_parent_->get_hw_serial_number(), portMAX_DELAY);
    if (result != ESP_OK) {
      ESP_LOGE(TAG, "uart_wait_tx_done failed: %d", result);
    }
  }
}

}  // namespace esphome::usb_uart_bridge
#endif
