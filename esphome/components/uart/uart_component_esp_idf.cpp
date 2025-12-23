#ifdef USE_ESP32

#include "uart_component_esp_idf.h"
#include <cinttypes>
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/gpio.h"
#include "driver/gpio.h"
#include "soc/gpio_num.h"
#include "soc/uart_pins.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

namespace esphome::uart {

static const char *const TAG = "uart.idf";

uart_config_t IDFUARTComponent::get_config_() {
  uart_parity_t parity = UART_PARITY_DISABLE;
  if (this->parity_ == UART_CONFIG_PARITY_EVEN) {
    parity = UART_PARITY_EVEN;
  } else if (this->parity_ == UART_CONFIG_PARITY_ODD) {
    parity = UART_PARITY_ODD;
  }

  uart_word_length_t data_bits;
  switch (this->data_bits_) {
    case 5:
      data_bits = UART_DATA_5_BITS;
      break;
    case 6:
      data_bits = UART_DATA_6_BITS;
      break;
    case 7:
      data_bits = UART_DATA_7_BITS;
      break;
    case 8:
      data_bits = UART_DATA_8_BITS;
      break;
    default:
      data_bits = UART_DATA_BITS_MAX;
      break;
  }

  uart_config_t uart_config{};
  uart_config.baud_rate = this->baud_rate_;
  uart_config.data_bits = data_bits;
  uart_config.parity = parity;
  uart_config.stop_bits = this->stop_bits_ == 1 ? UART_STOP_BITS_1 : UART_STOP_BITS_2;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.source_clk = UART_SCLK_DEFAULT;
  uart_config.rx_flow_ctrl_thresh = 122;

  return uart_config;
}

void IDFUARTComponent::setup() {
  static uint8_t next_uart_num = 0;

#ifdef USE_LOGGER
  bool logger_uses_hardware_uart = true;

#ifdef USE_LOGGER_USB_CDC
  if (logger::global_logger->get_uart() == logger::UART_SELECTION_USB_CDC) {
    // this is not a hardware UART, ignore it
    logger_uses_hardware_uart = false;
  }
#endif  // USE_LOGGER_USB_CDC

#ifdef USE_LOGGER_USB_SERIAL_JTAG
  if (logger::global_logger->get_uart() == logger::UART_SELECTION_USB_SERIAL_JTAG) {
    // this is not a hardware UART, ignore it
    logger_uses_hardware_uart = false;
  }
#endif  // USE_LOGGER_USB_SERIAL_JTAG

  if (logger_uses_hardware_uart && logger::global_logger->get_baud_rate() > 0 &&
      logger::global_logger->get_uart_num() == next_uart_num) {
    next_uart_num++;
  }
#endif  // USE_LOGGER

  if (next_uart_num >= SOC_UART_NUM) {
    ESP_LOGW(TAG, "Maximum number of UART components created already");
    this->mark_failed();
    return;
  }
  this->uart_num_ = static_cast<uart_port_t>(next_uart_num++);
  this->lock_ = xSemaphoreCreateMutex();

#if (SOC_UART_LP_NUM >= 1)
  size_t fifo_len = ((this->uart_num_ < SOC_UART_HP_NUM) ? SOC_UART_FIFO_LEN : SOC_LP_UART_FIFO_LEN);
#else
  size_t fifo_len = SOC_UART_FIFO_LEN;
#endif
  if (this->rx_buffer_size_ <= fifo_len) {
    ESP_LOGW(TAG, "rx_buffer_size is too small, must be greater than %zu", fifo_len);
    this->rx_buffer_size_ = fifo_len * 2;
  }

  xSemaphoreTake(this->lock_, portMAX_DELAY);

  this->load_settings(false);

  xSemaphoreGive(this->lock_);
}

void IDFUARTComponent::load_settings(bool dump_config) {
  esp_err_t err;

  if (uart_is_driver_installed(this->uart_num_)) {
#ifdef USE_UART_WAKE_LOOP_ON_RX
    if (this->rx_event_task_handle_ != nullptr) {
      vTaskDelete(this->rx_event_task_handle_);
      this->rx_event_task_handle_ = nullptr;
    }
#endif
    err = uart_driver_delete(this->uart_num_);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "uart_driver_delete failed: %s", esp_err_to_name(err));
      this->mark_failed();
      return;
    }
  }
  err = uart_driver_install(this->uart_num_,        // UART number
                            this->rx_buffer_size_,  // RX ring buffer size
                            0,   // TX ring buffer size. If zero, driver will not use a TX buffer and TX function will
                                 // block task until all data has been sent out
                            20,  // event queue size/depth
                            &this->uart_event_queue_,  // event queue
                            0                          // Flags used to allocate the interrupt
  );
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  int8_t tx = this->tx_pin_ != nullptr ? this->tx_pin_->get_pin() : -1;
  int8_t rx = this->rx_pin_ != nullptr ? this->rx_pin_->get_pin() : -1;
  int8_t flow_control = this->flow_control_pin_ != nullptr ? this->flow_control_pin_->get_pin() : -1;

  // Workaround for ESP-IDF issue: https://github.com/espressif/esp-idf/issues/17459
  // Commit 9ed617fb17 removed gpio_func_sel() calls from uart_set_pin(), which breaks
  // UART on default UART0 pins that may have residual state from boot console.
  // Reset these pins before configuring UART to ensure they're in a clean state.
  if (tx == U0TXD_GPIO_NUM || tx == U0RXD_GPIO_NUM) {
    gpio_reset_pin(static_cast<gpio_num_t>(tx));
  }
  if (rx == U0TXD_GPIO_NUM || rx == U0RXD_GPIO_NUM) {
    gpio_reset_pin(static_cast<gpio_num_t>(rx));
  }

  // Setup pins after reset to preserve open drain/pullup/pulldown flags
  auto setup_pin_if_needed = [](InternalGPIOPin *pin) {
    if (!pin) {
      return;
    }
    const auto mask = gpio::Flags::FLAG_OPEN_DRAIN | gpio::Flags::FLAG_PULLUP | gpio::Flags::FLAG_PULLDOWN;
    if ((pin->get_flags() & mask) != gpio::Flags::FLAG_NONE) {
      pin->setup();
    }
  };

  setup_pin_if_needed(this->rx_pin_);
  if (this->rx_pin_ != this->tx_pin_) {
    setup_pin_if_needed(this->tx_pin_);
  }

  uint32_t invert = 0;
  if (this->tx_pin_ != nullptr && this->tx_pin_->is_inverted()) {
    invert |= UART_SIGNAL_TXD_INV;
  }
  if (this->rx_pin_ != nullptr && this->rx_pin_->is_inverted()) {
    invert |= UART_SIGNAL_RXD_INV;
  }

  err = uart_set_line_inverse(this->uart_num_, invert);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "uart_set_line_inverse failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  err = uart_set_pin(this->uart_num_, tx, rx, flow_control, UART_PIN_NO_CHANGE);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  err = uart_set_rx_full_threshold(this->uart_num_, this->rx_full_threshold_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "uart_set_rx_full_threshold failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  err = uart_set_rx_timeout(this->uart_num_, this->rx_timeout_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "uart_set_rx_timeout failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  auto mode = this->flow_control_pin_ != nullptr ? UART_MODE_RS485_HALF_DUPLEX : UART_MODE_UART;
  err = uart_set_mode(this->uart_num_, mode);  // per docs, must be called only after uart_driver_install()
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "uart_set_mode failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  uart_config_t uart_config = this->get_config_();
  err = uart_param_config(this->uart_num_, &uart_config);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

#ifdef USE_UART_WAKE_LOOP_ON_RX
  // Start the RX event task to enable low-latency data notifications
  this->start_rx_event_task_();
#endif  // USE_UART_WAKE_LOOP_ON_RX

  if (dump_config) {
    ESP_LOGCONFIG(TAG, "Reloaded UART %u", this->uart_num_);
    this->dump_config();
  }
}

void IDFUARTComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "UART Bus %u:", this->uart_num_);
  LOG_PIN("  TX Pin: ", this->tx_pin_);
  LOG_PIN("  RX Pin: ", this->rx_pin_);
  LOG_PIN("  Flow Control Pin: ", this->flow_control_pin_);
  if (this->rx_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG,
                  "  RX Buffer Size: %u\n"
                  "  RX Full Threshold: %u\n"
                  "  RX Timeout: %u",
                  this->rx_buffer_size_, this->rx_full_threshold_, this->rx_timeout_);
  }
  ESP_LOGCONFIG(TAG,
                "  Baud Rate: %" PRIu32 " baud\n"
                "  Data Bits: %u\n"
                "  Parity: %s\n"
                "  Stop bits: %u"
#ifdef USE_UART_WAKE_LOOP_ON_RX
                "\n  Wake on data RX: ENABLED"
#endif
                ,
                this->baud_rate_, this->data_bits_, LOG_STR_ARG(parity_to_str(this->parity_)), this->stop_bits_);
  this->check_logger_conflict();
}

void IDFUARTComponent::set_rx_full_threshold(size_t rx_full_threshold) {
  if (this->is_ready()) {
    esp_err_t err = uart_set_rx_full_threshold(this->uart_num_, rx_full_threshold);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "uart_set_rx_full_threshold failed: %s", esp_err_to_name(err));
      return;
    }
  }
  this->rx_full_threshold_ = rx_full_threshold;
}

void IDFUARTComponent::set_rx_timeout(size_t rx_timeout) {
  if (this->is_ready()) {
    esp_err_t err = uart_set_rx_timeout(this->uart_num_, rx_timeout);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "uart_set_rx_timeout failed: %s", esp_err_to_name(err));
      return;
    }
  }
  this->rx_timeout_ = rx_timeout;
}

void IDFUARTComponent::write_array(const uint8_t *data, size_t len) {
  xSemaphoreTake(this->lock_, portMAX_DELAY);
  int32_t write_len = uart_write_bytes(this->uart_num_, data, len);
  xSemaphoreGive(this->lock_);
  if (write_len != (int32_t) len) {
    ESP_LOGW(TAG, "uart_write_bytes failed: %d != %zu", write_len, len);
    this->mark_failed();
  }
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(UART_DIRECTION_TX, data[i]);
  }
#endif
}

bool IDFUARTComponent::peek_byte(uint8_t *data) {
  if (!this->check_read_timeout_())
    return false;
  xSemaphoreTake(this->lock_, portMAX_DELAY);
  if (this->has_peek_) {
    *data = this->peek_byte_;
  } else {
    int len = uart_read_bytes(this->uart_num_, data, 1, 20 / portTICK_PERIOD_MS);
    if (len == 0) {
      *data = 0;
    } else {
      this->has_peek_ = true;
      this->peek_byte_ = *data;
    }
  }
  xSemaphoreGive(this->lock_);
  return true;
}

bool IDFUARTComponent::read_array(uint8_t *data, size_t len) {
  size_t length_to_read = len;
  int32_t read_len = 0;
  if (!this->check_read_timeout_(len))
    return false;
  xSemaphoreTake(this->lock_, portMAX_DELAY);
  if (this->has_peek_) {
    length_to_read--;
    *data = this->peek_byte_;
    data++;
    this->has_peek_ = false;
  }
  if (length_to_read > 0)
    read_len = uart_read_bytes(this->uart_num_, data, length_to_read, 20 / portTICK_PERIOD_MS);
  xSemaphoreGive(this->lock_);
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(UART_DIRECTION_RX, data[i]);
  }
#endif
  return read_len == (int32_t) length_to_read;
}

int IDFUARTComponent::available() {
  size_t available = 0;
  esp_err_t err;

  xSemaphoreTake(this->lock_, portMAX_DELAY);
  err = uart_get_buffered_data_len(this->uart_num_, &available);
  xSemaphoreGive(this->lock_);

  if (err != ESP_OK) {
    ESP_LOGW(TAG, "uart_get_buffered_data_len failed: %s", esp_err_to_name(err));
    this->mark_failed();
  }
  if (this->has_peek_) {
    available++;
  }
  return available;
}

void IDFUARTComponent::flush() {
  ESP_LOGVV(TAG, "    Flushing");
  xSemaphoreTake(this->lock_, portMAX_DELAY);
  uart_wait_tx_done(this->uart_num_, portMAX_DELAY);
  xSemaphoreGive(this->lock_);
}

void IDFUARTComponent::check_logger_conflict() {}

#ifdef USE_UART_WAKE_LOOP_ON_RX
void IDFUARTComponent::start_rx_event_task_() {
  // Create FreeRTOS task to monitor UART events
  BaseType_t result = xTaskCreate(rx_event_task_func,    // Task function
                                  "uart_rx_evt",         // Task name (max 16 chars)
                                  2240,                  // Stack size in bytes (~2.2KB); increase if needed for logging
                                  this,                  // Task parameter (this pointer)
                                  tskIDLE_PRIORITY + 1,  // Priority (low, just above idle)
                                  &this->rx_event_task_handle_  // Task handle
  );

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create RX event task");
    return;
  }

  ESP_LOGV(TAG, "RX event task started");
}

void IDFUARTComponent::rx_event_task_func(void *param) {
  auto *self = static_cast<IDFUARTComponent *>(param);
  uart_event_t event;

  ESP_LOGV(TAG, "RX event task running");

  // Run forever - task lifecycle matches component lifecycle
  while (true) {
    // Wait for UART events (blocks efficiently)
    if (xQueueReceive(self->uart_event_queue_, &event, portMAX_DELAY) == pdTRUE) {
      switch (event.type) {
        case UART_DATA:
          // Data available in UART RX buffer - wake the main loop
          ESP_LOGVV(TAG, "Data event: %d bytes", event.size);
          App.wake_loop_threadsafe();
          break;

        case UART_FIFO_OVF:
        case UART_BUFFER_FULL:
          ESP_LOGW(TAG, "FIFO overflow or ring buffer full - clearing");
          uart_flush_input(self->uart_num_);
          App.wake_loop_threadsafe();
          break;

        default:
          // Ignore other event types
          ESP_LOGVV(TAG, "Event type: %d", event.type);
          break;
      }
    }
  }
}
#endif  // USE_UART_WAKE_LOOP_ON_RX

}  // namespace esphome::uart
#endif  // USE_ESP32
