#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "usb_cdc_acm.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "tusb.h"
#include "tusb_cdc_acm.h"

namespace esphome::usb_cdc_acm {

static const char *TAG = "usb_cdc_acm";

static constexpr size_t USB_TX_TASK_STACK_SIZE = 4096;
static constexpr size_t USB_TX_TASK_STACK_SIZE_VV = 8192;

// Global component instance for managing USB device
USBCDCACMComponent *global_usb_cdc_component = nullptr;

static USBCDCACMInstance *get_instance_by_itf(int itf) {
  if (global_usb_cdc_component == nullptr) {
    return nullptr;
  }
  return global_usb_cdc_component->get_interface_by_number(itf);
}

static void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event) {
  USBCDCACMInstance *instance = get_instance_by_itf(itf);
  if (instance == nullptr) {
    ESP_LOGE(TAG, "RX callback: invalid interface %d", itf);
    return;
  }

  size_t rx_size = 0;
  static uint8_t rx_buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE] = {0};

  // read from USB
  esp_err_t ret =
      tinyusb_cdcacm_read(static_cast<tinyusb_cdcacm_itf_t>(itf), rx_buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
  ESP_LOGV(TAG, "tinyusb_cdc_rx_callback itf=%d (size: %u)", itf, rx_size);
  ESP_LOGVV(TAG, "rx_buf = %s", format_hex_pretty(rx_buf, rx_size).c_str());

  if (ret == ESP_OK && rx_size > 0) {
    RingbufHandle_t rx_ringbuf = instance->get_rx_ringbuf();
    if (rx_ringbuf != nullptr) {
      BaseType_t send_res = xRingbufferSend(rx_ringbuf, rx_buf, rx_size, 0);
      if (send_res != pdTRUE) {
        ESP_LOGE(TAG, "USB RX itf=%d: buffer full, %u bytes lost", itf, rx_size);
      } else {
        ESP_LOGV(TAG, "USB RX itf=%d: queued %u bytes", itf, rx_size);
      }
    }
  }
}

static void tinyusb_cdc_line_state_changed_callback(int itf, cdcacm_event_t *event) {
  USBCDCACMInstance *instance = get_instance_by_itf(itf);
  if (instance == nullptr) {
    ESP_LOGE(TAG, "Line state callback: invalid interface %d", itf);
    return;
  }

  int dtr = event->line_state_changed_data.dtr;
  int rts = event->line_state_changed_data.rts;
  ESP_LOGV(TAG, "Line state itf=%d: DTR=%d, RTS=%d", itf, dtr, rts);

  // Queue event for processing in main loop
  instance->queue_line_state_event(dtr != 0, rts != 0);
}

static void tinyusb_cdc_line_coding_changed_callback(int itf, cdcacm_event_t *event) {
  USBCDCACMInstance *instance = get_instance_by_itf(itf);
  if (instance == nullptr) {
    ESP_LOGE(TAG, "Line coding callback: invalid interface %d", itf);
    return;
  }

  uint32_t bit_rate = event->line_coding_changed_data.p_line_coding->bit_rate;
  uint8_t stop_bits = event->line_coding_changed_data.p_line_coding->stop_bits;
  uint8_t parity = event->line_coding_changed_data.p_line_coding->parity;
  uint8_t data_bits = event->line_coding_changed_data.p_line_coding->data_bits;
  ESP_LOGV(TAG, "Line coding itf=%d: bit_rate=%" PRIu32 " stop_bits=%u parity=%u data_bits=%u", itf, bit_rate,
           stop_bits, parity, data_bits);

  // Queue event for processing in main loop
  instance->queue_line_coding_event(bit_rate, stop_bits, parity, data_bits);
}

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

//==============================================================================
// USBCDCACMInstance Implementation
//==============================================================================

void USBCDCACMInstance::setup() {
  this->usb_tx_ringbuf_ = xRingbufferCreate(CONFIG_TINYUSB_CDC_TX_BUFSIZE, RINGBUF_TYPE_BYTEBUF);
  if (this->usb_tx_ringbuf_ == nullptr) {
    ESP_LOGE(TAG, "USB TX buffer creation error for itf %d", this->itf_);
    this->parent_->mark_failed();
    return;
  }

  this->usb_rx_ringbuf_ = xRingbufferCreate(CONFIG_TINYUSB_CDC_RX_BUFSIZE, RINGBUF_TYPE_BYTEBUF);
  if (this->usb_rx_ringbuf_ == nullptr) {
    ESP_LOGE(TAG, "USB RX buffer creation error for itf %d", this->itf_);
    this->parent_->mark_failed();
    return;
  }

  // Configure this CDC interface
  const tinyusb_config_cdcacm_t acm_cfg = {
      .usb_dev = TINYUSB_USBDEV_0,
      .cdc_port = this->itf_,
      .callback_rx = &tinyusb_cdc_rx_callback,
      .callback_rx_wanted_char = NULL,
      .callback_line_state_changed = &tinyusb_cdc_line_state_changed_callback,
      .callback_line_coding_changed = &tinyusb_cdc_line_coding_changed_callback,
  };

  esp_err_t result = tusb_cdc_acm_init(&acm_cfg);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "tusb_cdc_acm_init failed: %d", result);
    this->parent_->mark_failed();
    return;
  }

  // Use a larger stack size for (very) verbose logging
  const size_t stack_size = esp_log_level_get(TAG) > ESP_LOG_DEBUG ? USB_TX_TASK_STACK_SIZE_VV : USB_TX_TASK_STACK_SIZE;

  // Create a simple, unique task name per interface
  char task_name[] = "usb_tx_0";
  task_name[sizeof(task_name) - 1] = format_hex_char(static_cast<char>(this->itf_));
  xTaskCreate(usb_tx_task_fn, task_name, stack_size, this, 4, &this->usb_tx_task_handle_);

  if (this->usb_tx_task_handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create USB TX task for itf %d", this->itf_);
    this->parent_->mark_failed();
    return;
  }
}

void USBCDCACMInstance::loop() {
  // Process events from the lock-free queue
  this->process_events_();
}

void USBCDCACMInstance::queue_line_state_event(bool dtr, bool rts) {
  // Allocate event from pool
  CDCEvent *event = this->event_pool_.allocate();
  if (event == nullptr) {
    ESP_LOGW(TAG, "Event pool exhausted, line state event dropped (itf=%d)", this->itf_);
    return;
  }

  event->type = CDC_EVENT_LINE_STATE_CHANGED;
  event->data.line_state.dtr = dtr;
  event->data.line_state.rts = rts;

  if (!this->event_queue_.push(event)) {
    ESP_LOGW(TAG, "Event queue full, line state event dropped (itf=%d)", this->itf_);
    // Return event to pool since we couldn't queue it
    this->event_pool_.release(event);
  } else {
    // Wake main loop immediately to process event
#if defined(USE_SOCKET_SELECT_SUPPORT) && defined(USE_WAKE_LOOP_THREADSAFE)
    App.wake_loop_threadsafe();
#endif
  }
}

void USBCDCACMInstance::queue_line_coding_event(uint32_t bit_rate, uint8_t stop_bits, uint8_t parity,
                                                uint8_t data_bits) {
  // Allocate event from pool
  CDCEvent *event = this->event_pool_.allocate();
  if (event == nullptr) {
    ESP_LOGW(TAG, "Event pool exhausted, line coding event dropped (itf=%d)", this->itf_);
    return;
  }

  event->type = CDC_EVENT_LINE_CODING_CHANGED;
  event->data.line_coding.bit_rate = bit_rate;
  event->data.line_coding.stop_bits = stop_bits;
  event->data.line_coding.parity = parity;
  event->data.line_coding.data_bits = data_bits;

  if (!this->event_queue_.push(event)) {
    ESP_LOGW(TAG, "Event queue full, line coding event dropped (itf=%d)", this->itf_);
    // Return event to pool since we couldn't queue it
    this->event_pool_.release(event);
  } else {
    // Wake main loop immediately to process event
#if defined(USE_SOCKET_SELECT_SUPPORT) && defined(USE_WAKE_LOOP_THREADSAFE)
    App.wake_loop_threadsafe();
#endif
  }
}

void USBCDCACMInstance::process_events_() {
  // Process all pending events from the queue
  CDCEvent *event;
  while ((event = this->event_queue_.pop()) != nullptr) {
    switch (event->type) {
      case CDC_EVENT_LINE_STATE_CHANGED: {
        bool dtr = event->data.line_state.dtr;
        bool rts = event->data.line_state.rts;

        // Invoke user callback in main loop context
        if (this->line_state_callback_ != nullptr) {
          this->line_state_callback_(dtr, rts);
        }
        break;
      }
      case CDC_EVENT_LINE_CODING_CHANGED: {
        uint32_t bit_rate = event->data.line_coding.bit_rate;
        uint8_t stop_bits = event->data.line_coding.stop_bits;
        uint8_t parity = event->data.line_coding.parity;
        uint8_t data_bits = event->data.line_coding.data_bits;

        // Update UART configuration based on CDC line coding
        this->baud_rate_ = bit_rate;
        this->data_bits_ = data_bits;

        // Convert CDC stop bits to UART stop bits format
        // CDC: 0=1 stop bit, 1=1.5 stop bits, 2=2 stop bits
        this->stop_bits_ = (stop_bits == 0) ? 1 : (stop_bits == 1) ? 1 : 2;

        // Convert CDC parity to UART parity format
        // CDC: 0=None, 1=Odd, 2=Even, 3=Mark, 4=Space
        switch (parity) {
          case 0:
            this->parity_ = uart::UART_CONFIG_PARITY_NONE;
            break;
          case 1:
            this->parity_ = uart::UART_CONFIG_PARITY_ODD;
            break;
          case 2:
            this->parity_ = uart::UART_CONFIG_PARITY_EVEN;
            break;
          default:
            // Mark and Space parity are not commonly supported, default to None
            this->parity_ = uart::UART_CONFIG_PARITY_NONE;
            break;
        }

        // Invoke user callback in main loop context
        if (this->line_coding_callback_ != nullptr) {
          this->line_coding_callback_(bit_rate, stop_bits, parity, data_bits);
        }
        break;
      }
    }
    // Return event to pool for reuse
    this->event_pool_.release(event);
  }
}

void USBCDCACMInstance::usb_tx_task_fn(void *arg) {
  auto *instance = static_cast<USBCDCACMInstance *>(arg);
  instance->usb_tx_task();
}

void USBCDCACMInstance::usb_tx_task() {
  uint8_t data[CONFIG_TINYUSB_CDC_TX_BUFSIZE] = {0};
  size_t tx_data_size = 0;

  while (1) {
    // Wait for a notification from the bridge component
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // When we do wake up, we can be sure there is data in the ring buffer
    esp_err_t ret = ringbuf_read_bytes(this->usb_tx_ringbuf_, data, CONFIG_TINYUSB_CDC_TX_BUFSIZE, &tx_data_size, 0);

    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "USB TX itf=%d: RingBuf read failed", this->itf_);
      continue;
    } else if (tx_data_size == 0) {
      ESP_LOGD(TAG, "USB TX itf=%d: RingBuf empty, skipping", this->itf_);
      continue;
    }

    ESP_LOGV(TAG, "USB TX itf=%d: Read %d bytes from buffer", this->itf_, tx_data_size);
    ESP_LOGVV(TAG, "data = %s", format_hex_pretty(data, tx_data_size).c_str());

    // Serial data will be split up into 64 byte chunks to be sent over USB so this
    // usually will take multiple iterations
    uint8_t *data_head = &data[0];

    while (tx_data_size > 0) {
      size_t queued = tinyusb_cdcacm_write_queue(this->itf_, data_head, tx_data_size);
      ESP_LOGV(TAG, "USB TX itf=%d: enqueued: size=%d, queued=%u", this->itf_, tx_data_size, queued);

      tx_data_size -= queued;
      data_head += queued;

      ESP_LOGV(TAG, "USB TX itf=%d: waiting 10ms for flush", this->itf_);
      esp_err_t flush_ret = tinyusb_cdcacm_write_flush(this->itf_, pdMS_TO_TICKS(10));

      if (flush_ret != ESP_OK) {
        ESP_LOGE(TAG, "USB TX itf=%d: flush failed", this->itf_);
        tud_cdc_n_write_clear(this->itf_);
        break;
      }
    }
  }
}

//==============================================================================
// UARTComponent Interface Implementation
//==============================================================================

void USBCDCACMInstance::write_array(const uint8_t *data, size_t len) {
  if (len == 0) {
    return;
  }

  // Write data to TX ring buffer
  BaseType_t send_res = xRingbufferSend(this->usb_tx_ringbuf_, data, len, 0);
  if (send_res != pdTRUE) {
    ESP_LOGW(TAG, "USB TX itf=%d: buffer full, %u bytes dropped", this->itf_, len);
    return;
  }

  // Notify TX task that data is available
  if (this->usb_tx_task_handle_ != nullptr) {
    xTaskNotifyGive(this->usb_tx_task_handle_);
  }
}

bool USBCDCACMInstance::peek_byte(uint8_t *data) {
  if (this->has_peek_) {
    *data = this->peek_buffer_;
    return true;
  }

  if (this->read_byte(&this->peek_buffer_)) {
    *data = this->peek_buffer_;
    this->has_peek_ = true;
    return true;
  }

  return false;
}

bool USBCDCACMInstance::read_array(uint8_t *data, size_t len) {
  if (len == 0) {
    return true;
  }

  size_t original_len = len;
  size_t bytes_read = 0;

  // First, use the peek buffer if available
  if (this->has_peek_) {
    data[0] = this->peek_buffer_;
    this->has_peek_ = false;
    bytes_read = 1;
    data++;
    if (--len == 0) {  // Decrement len first, then check it...
      return true;     // No more to read
    }
  }

  // Read remaining bytes from RX ring buffer
  size_t rx_size = 0;
  uint8_t *buf = static_cast<uint8_t *>(xRingbufferReceiveUpTo(this->usb_rx_ringbuf_, &rx_size, 0, len));
  if (buf == nullptr) {
    return false;
  }

  memcpy(data, buf, rx_size);
  vRingbufferReturnItem(this->usb_rx_ringbuf_, (void *) buf);
  bytes_read += rx_size;
  data += rx_size;
  len -= rx_size;
  if (len == 0) {
    return true;  // No more to read
  }

  // Buffer's data may wrap around, in which case we should perform another read
  buf = static_cast<uint8_t *>(xRingbufferReceiveUpTo(this->usb_rx_ringbuf_, &rx_size, 0, len));
  if (buf == nullptr) {
    return false;
  }

  memcpy(data, buf, rx_size);
  vRingbufferReturnItem(this->usb_rx_ringbuf_, (void *) buf);
  bytes_read += rx_size;

  return bytes_read == original_len;
}

int USBCDCACMInstance::available() {
  UBaseType_t waiting = 0;
  if (this->usb_rx_ringbuf_ != nullptr) {
    vRingbufferGetInfo(this->usb_rx_ringbuf_, nullptr, nullptr, nullptr, nullptr, &waiting);
  }
  return static_cast<int>(waiting) + (this->has_peek_ ? 1 : 0);
}

void USBCDCACMInstance::flush() {
  // Wait for TX ring buffer to be empty
  if (this->usb_tx_ringbuf_ == nullptr) {
    return;
  }

  UBaseType_t waiting = 1;
  while (waiting > 0) {
    vRingbufferGetInfo(this->usb_tx_ringbuf_, nullptr, nullptr, nullptr, nullptr, &waiting);
    if (waiting > 0) {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }

  // Also wait for USB to finish transmitting
  tinyusb_cdcacm_write_flush(this->itf_, pdMS_TO_TICKS(100));
}

//==============================================================================
// USBCDCACMComponent Implementation
//==============================================================================

USBCDCACMComponent::USBCDCACMComponent() { global_usb_cdc_component = this; }

void USBCDCACMComponent::setup() {
  // Setup all registered interfaces
  for (auto interface : this->interfaces_) {
    if (interface != nullptr) {
      interface->setup();
    }
  }
}

void USBCDCACMComponent::loop() {
  // Call loop() on all registered interfaces to process events
  for (auto interface : this->interfaces_) {
    if (interface != nullptr) {
      interface->loop();
    }
  }
}

void USBCDCACMComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "USB CDC-ACM:\n"
                "  Number of Interfaces: %d",
                this->interfaces_[MAX_USB_CDC_INSTANCES - 1] != nullptr ? MAX_USB_CDC_INSTANCES : 1);
}

void USBCDCACMComponent::add_interface(USBCDCACMInstance *interface) {
  uint8_t itf_num = static_cast<uint8_t>(interface->get_itf());
  if (itf_num < MAX_USB_CDC_INSTANCES) {
    this->interfaces_[itf_num] = interface;
  } else {
    ESP_LOGE(TAG, "Interface number must be less than %u", MAX_USB_CDC_INSTANCES);
  }
}

USBCDCACMInstance *USBCDCACMComponent::get_interface_by_number(uint8_t itf) {
  for (auto interface : this->interfaces_) {
    if ((interface != nullptr) && (interface->get_itf() == static_cast<tinyusb_cdcacm_itf_t>(itf))) {
      return interface;
    }
  }
  return nullptr;
}

}  // namespace esphome::usb_cdc_acm
#endif
