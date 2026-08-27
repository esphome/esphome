#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)
#include "usb_cdc_acm.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cstring>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "tusb.h"
#include "tinyusb_cdc_acm.h"

namespace esphome::usb_cdc_acm {

static const char *const TAG = "usb_cdc_acm";

// Maximum bytes to log in very verbose hex output (168 * 3 = 504, under TX buffer size of 512)
static constexpr size_t USB_CDC_MAX_LOG_BYTES = 168;

static constexpr size_t USB_TX_TASK_STACK_SIZE = 4096;
static constexpr size_t USB_TX_TASK_STACK_SIZE_VV = 8192;

// Upper bound on how long flush() may block in total: the TX ring buffer drain and
// the final TinyUSB flush share this budget.
static constexpr uint32_t FLUSH_TIMEOUT_MS = 100;

// Minimum interval between repeated warnings while a host stall persists.
static constexpr uint32_t LOG_THROTTLE_MS = 1000;

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
      .cdc_port = static_cast<tinyusb_cdcacm_itf_t>(this->itf_),
      .callback_rx = &tinyusb_cdc_rx_callback,
      .callback_rx_wanted_char = NULL,
      .callback_line_state_changed = &tinyusb_cdc_line_state_changed_callback,
      .callback_line_coding_changed = &tinyusb_cdc_line_coding_changed_callback,
  };

  esp_err_t result = tinyusb_cdcacm_init(&acm_cfg);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "tinyusb_cdcacm_init failed: %d", result);
    this->parent_->mark_failed();
    return;
  }

  // Use a larger stack size for very verbose logging
  constexpr size_t stack_size =
      ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE ? USB_TX_TASK_STACK_SIZE_VV : USB_TX_TASK_STACK_SIZE;

  // Create a simple, unique task name per interface
  char task_name[] = "usb_tx_0";
  task_name[sizeof(task_name) - 2] = format_hex_char(static_cast<char>(this->itf_));
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
  // Back-dated so a stall within the first LOG_THROTTLE_MS of uptime still logs
  // immediately (unsigned arithmetic keeps this wrap-safe).
  uint32_t stall_log_ms = millis() - LOG_THROTTLE_MS;

  while (true) {
    // Not holding any data while blocked waiting for more.
    this->usb_tx_busy_ = 0;

    // Wait for a notification from the bridge component
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Raise the busy flag before pulling data out of the ring buffer, so at every
    // instant flush() sees pending bytes in the ring buffer count or in this flag.
    this->usb_tx_busy_ = 1;

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

      if (flush_ret == ESP_OK) {
        continue;
      }

      // Bytes not yet handed to TinyUSB plus bytes still sitting in its transmit FIFO.
      // tud_cdc_n_write_occupied() is not public API in the pinned TinyUSB release, so
      // derive the occupancy from the FIFO depth TinyUSB itself is configured with.
      const size_t pending = tx_data_size + (CFG_TUD_CDC_TX_BUFSIZE - tud_cdc_n_write_available(this->itf_));

      // A flush timeout only means TinyUSB's transmit FIFO did not fully drain within
      // the wait window; the queued bytes are untouched and TinyUSB keeps sending them
      // from its transfer-complete callback once the host polls again. Clearing the
      // FIFO here would discard the tail of a frame whose head is already on the wire,
      // corrupting the stream mid-frame. Hold the data and retry instead; sustained
      // backpressure then propagates to the ring buffer, which drops whole writes with
      // a warning instead of splitting a frame.
      //
      // Gate the retry on DTR (tud_cdc_n_connected()) rather than tud_ready(): an
      // enumerated-but-idle host (no application holding the port open) never polls
      // the IN endpoint, so retrying on tud_ready() alone would wedge this task -- and
      // stall every write_array()/flush() caller behind a full ring buffer -- for as
      // long as the board sits plugged into an idle PC. DTR means an application has
      // the port open and is expected to eventually read.
      if (flush_ret == ESP_ERR_TIMEOUT && tud_cdc_n_connected(this->itf_)) {
        const uint32_t now = millis();
        if ((now - stall_log_ms) >= LOG_THROTTLE_MS) {
          stall_log_ms = now;
          ESP_LOGW(TAG, "USB TX itf=%d: host not reading; %zu bytes pending", this->itf_, pending);
        }
        continue;
      }

      if (flush_ret == ESP_ERR_TIMEOUT) {
        // No application has the port open (DTR deasserted) or the device is detached,
        // so the data cannot be delivered. TinyUSB does not clear its transmit FIFO on
        // bus reset; drop the data here so a stale partial frame is not replayed when
        // the port is (re)opened.
        ESP_LOGW(TAG, "USB TX itf=%d: not connected; dropping %zu bytes", this->itf_, pending);
      } else {
        ESP_LOGE(TAG, "USB TX itf=%d: flush failed (%s); dropping %zu bytes", this->itf_, esp_err_to_name(flush_ret),
                 pending);
      }
      tud_cdc_n_write_clear(this->itf_);
      break;
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
    // During a sustained host stall the ring buffer stays full (that is the intended
    // backpressure), so this path runs for every write; throttle the warning so the
    // log stays readable. The counter is a running total that is never reset: each
    // line reports all bytes dropped so far, so bytes dropped in the tail of one
    // stall are still accounted for by the next line, whenever that is. It also makes
    // the very first drop since boot detectable, which is logged unthrottled.
    const bool first_drop = this->tx_dropped_bytes_ == 0;
    this->tx_dropped_bytes_ += len;
    const uint32_t now = millis();
    if (first_drop || (now - this->tx_dropped_log_ms_) >= LOG_THROTTLE_MS) {
      this->tx_dropped_log_ms_ = now;
      ESP_LOGW(TAG, "USB TX itf=%d: buffer full, %" PRIu32 " bytes dropped total", this->itf_, this->tx_dropped_bytes_);
    }
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

size_t USBCDCACMInstance::available() {
  UBaseType_t waiting = 0;
  if (this->usb_rx_ringbuf_ != nullptr) {
    vRingbufferGetInfo(this->usb_rx_ringbuf_, nullptr, nullptr, nullptr, nullptr, &waiting);
  }
  return waiting + (this->has_peek_ ? 1 : 0);
}

// True while TX bytes have not yet reached TinyUSB's FIFO: still counted in the ring
// buffer, or held by the TX task (usb_tx_busy_) between pulling them from the ring
// buffer and handing them to TinyUSB -- there they are in neither the ring buffer
// count nor TinyUSB's FIFO.
bool USBCDCACMInstance::tx_pending_() {
  UBaseType_t waiting = 0;
  vRingbufferGetInfo(this->usb_tx_ringbuf_, nullptr, nullptr, nullptr, nullptr, &waiting);
  return waiting != 0 || this->usb_tx_busy_ != 0;
}

uart::UARTFlushResult USBCDCACMInstance::flush() {
  if (this->usb_tx_ringbuf_ == nullptr) {
    return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS;
  }

  // Bound the wait: when the host stalls or disconnects, the TX task holds on to
  // pending data rather than discarding it, so the ring buffer may not drain for as
  // long as the host stays away. flush() runs on the caller's (typically the main
  // loop) task and must not block indefinitely. Signed tick differences keep the
  // deadline arithmetic wrap-safe.
  TickType_t now = xTaskGetTickCount();
  const TickType_t deadline = now + pdMS_TO_TICKS(FLUSH_TIMEOUT_MS);
  while (this->tx_pending_()) {
    if (static_cast<int32_t>(now - deadline) >= 0) {
      return uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
    now = xTaskGetTickCount();
  }

  // Also wait for USB to finish transmitting, within whatever remains of the budget.
  // Floor at one tick: a zero-tick timeout takes esp_tinyusb's non-blocking branch,
  // whose return contract is that library's internal detail and may differ between
  // releases. One tick keeps the call on the blocking branch (ESP_OK/ESP_ERR_TIMEOUT)
  // at the cost of at most one tick over budget.
  const int32_t remaining = static_cast<int32_t>(deadline - now);
  const TickType_t flush_ticks = remaining > 0 ? static_cast<TickType_t>(remaining) : 1;
  switch (tinyusb_cdcacm_write_flush(static_cast<tinyusb_cdcacm_itf_t>(this->itf_), flush_ticks)) {
    case ESP_OK:
      return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS;
    case ESP_ERR_TIMEOUT:
    // ESP_ERR_NOT_FINISHED is the non-blocking branch's "still draining" result;
    // mapped like a timeout in case a future esp_tinyusb release returns it here.
    case ESP_ERR_NOT_FINISHED:
      return uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT;
    default:
      return uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED;
  }
}

void USBCDCACMInstance::check_logger_conflict() {}

}  // namespace esphome::usb_cdc_acm
#endif
