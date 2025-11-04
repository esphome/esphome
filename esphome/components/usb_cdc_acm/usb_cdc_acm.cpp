#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "usb_cdc_acm.h"
#include "esphome/core/log.h"

#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_log.h"

#include "tusb.h"
#include "tusb_cdc_acm.h"

namespace esphome {
namespace usb_cdc_acm {

static const char *TAG = "usb_cdc_acm";

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
  ESP_LOG_BUFFER_HEXDUMP(TAG, rx_buf, rx_size, ESP_LOG_VERBOSE);

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

  // Invoke the registered callback if present
  instance->invoke_line_state_callback(dtr != 0, rts != 0);
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

  // Invoke the registered callback if present
  instance->invoke_line_coding_callback(bit_rate, stop_bits, parity, data_bits);
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
    return;
  }

  this->usb_rx_ringbuf_ = xRingbufferCreate(CONFIG_TINYUSB_CDC_RX_BUFSIZE, RINGBUF_TYPE_BYTEBUF);
  if (this->usb_rx_ringbuf_ == nullptr) {
    ESP_LOGE(TAG, "USB RX buffer creation error for itf %d", this->itf_);
    return;
  }

  // Configure this CDC interface
  this->acm_cfg_ = {
      .usb_dev = TINYUSB_USBDEV_0,
      .cdc_port = this->itf_,
      .rx_unread_buf_sz = CONFIG_TINYUSB_CDC_RX_BUFSIZE,
      .callback_rx = &tinyusb_cdc_rx_callback,
      .callback_rx_wanted_char = NULL,
      .callback_line_state_changed = &tinyusb_cdc_line_state_changed_callback,
      .callback_line_coding_changed = &tinyusb_cdc_line_coding_changed_callback,
  };

  esp_err_t result = tusb_cdc_acm_init(&this->acm_cfg_);
  if (result != ESP_OK) {
    this->mark_failed();
  }

  size_t stack_size = 4096;
  if (esp_log_level_get(TAG) > ESP_LOG_DEBUG) {
    stack_size = 8192;  // Increase stack size for debug logging
  }

  // Create a simple, unique task name per interface
  char task_name[] = "usb_tx_0";
  task_name[sizeof(task_name) - 1] = static_cast<char>(this->itf_) + '0';
  xTaskCreate(usb_tx_task_fn, task_name, stack_size, this, 4, &this->usb_tx_task_handle_);

  if (this->usb_tx_task_handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create USB TX task for itf %d", this->itf_);
    return;
  }
}

void USBCDCACMInstance::invoke_line_coding_callback(uint32_t bit_rate, uint8_t stop_bits, uint8_t parity,
                                                    uint8_t data_bits) {
  if (this->line_coding_callback_) {
    this->line_coding_callback_(bit_rate, stop_bits, parity, data_bits);
  }
}

void USBCDCACMInstance::invoke_line_state_callback(bool dtr, bool rts) {
  if (this->line_state_callback_) {
    this->line_state_callback_(dtr, rts);
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
    ESP_LOG_BUFFER_HEXDUMP(TAG, data, tx_data_size, ESP_LOG_VERBOSE);

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
// USBCDCACMComponent Implementation
//==============================================================================

USBCDCACMComponent::USBCDCACMComponent() { global_usb_cdc_component = this; }

void USBCDCACMComponent::setup() {
  // Setup all registered interfaces
  for (uint8_t i = 0; i < MAX_USB_CDC_INSTANCES; i++) {
    if (this->interfaces_[i] != nullptr) {
      this->interfaces_[i]->setup();
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
  for (uint8_t i = 0; i < MAX_USB_CDC_INSTANCES; i++) {
    if ((this->interfaces_[i] != nullptr) &&
        (this->interfaces_[i]->get_itf() == static_cast<tinyusb_cdcacm_itf_t>(itf))) {
      return this->interfaces_[i];
    }
  }
  return nullptr;
}

}  // namespace usb_cdc_acm
}  // namespace esphome
#endif
