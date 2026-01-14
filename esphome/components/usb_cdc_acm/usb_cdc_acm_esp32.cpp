#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "usb_cdc_acm.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cstring>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "tusb.h"
#include "tusb_cdc_acm.h"

namespace esphome::usb_cdc_acm {

static const char *const TAG = "usb_cdc_acm";

// Maximum bytes to log in very verbose hex output (168 * 3 = 504, under TX buffer size of 512)
static constexpr size_t USB_CDC_MAX_LOG_BYTES = 168;

static constexpr size_t USB_TX_TASK_STACK_SIZE = 4096;
static constexpr size_t USB_TX_TASK_STACK_SIZE_VV = 8192;

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
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
  char rx_hex_buf[format_hex_pretty_size(USB_CDC_MAX_LOG_BYTES)];
#endif
  ESP_LOGVV(TAG, "rx_buf = %s", format_hex_pretty_to(rx_hex_buf, rx_buf, rx_size));

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
      .cdc_port = static_cast<tinyusb_cdcacm_itf_t>(this->itf_),
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

void USBCDCACMInstance::dump_config() {}

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
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
    char tx_hex_buf[format_hex_pretty_size(USB_CDC_MAX_LOG_BYTES)];
#endif
    ESP_LOGVV(TAG, "data = %s", format_hex_pretty_to(tx_hex_buf, data, tx_data_size));

    // Serial data will be split up into 64 byte chunks to be sent over USB so this
    // usually will take multiple iterations
    uint8_t *data_head = &data[0];

    while (tx_data_size > 0) {
      size_t queued =
          tinyusb_cdcacm_write_queue(static_cast<tinyusb_cdcacm_itf_t>(this->itf_), data_head, tx_data_size);
      ESP_LOGV(TAG, "USB TX itf=%d: enqueued: size=%d, queued=%u", this->itf_, tx_data_size, queued);

      tx_data_size -= queued;
      data_head += queued;

      ESP_LOGV(TAG, "USB TX itf=%d: waiting 10ms for flush", this->itf_);
      esp_err_t flush_ret =
          tinyusb_cdcacm_write_flush(static_cast<tinyusb_cdcacm_itf_t>(this->itf_), pdMS_TO_TICKS(10));

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
  tinyusb_cdcacm_write_flush(static_cast<tinyusb_cdcacm_itf_t>(this->itf_), pdMS_TO_TICKS(100));
}

void USBCDCACMInstance::check_logger_conflict() {}

}  // namespace esphome::usb_cdc_acm
#endif
