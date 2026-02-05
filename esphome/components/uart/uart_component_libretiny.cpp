#ifdef USE_LIBRETINY

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "uart_component_libretiny.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

#if LT_ARD_HAS_SOFTSERIAL
#include <SoftwareSerial.h>
#endif

namespace esphome::uart {

static const char *const TAG = "uart.lt";

static const char *UART_TYPE[] = {
    "hardware",
    "software",
};

static bool gpioPinHasAnyFlagSet(const InternalGPIOPin *pin, const gpio::Flags mask) {
  return pin && (pin->get_flags() & mask) != gpio::Flags::FLAG_NONE;
}

static bool shouldFallbackToSoftwareSerial(const InternalGPIOPin *rx_pin, const InternalGPIOPin *tx_pin) {
  if (gpioPinHasAnyFlagSet(tx_pin,
                           gpio::Flags::FLAG_OPEN_DRAIN | gpio::Flags::FLAG_PULLUP | gpio::Flags::FLAG_PULLDOWN) ||
      gpioPinHasAnyFlagSet(rx_pin,
                           gpio::Flags::FLAG_OPEN_DRAIN | gpio::Flags::FLAG_PULLUP | gpio::Flags::FLAG_PULLDOWN)) {
    ESP_LOGI(TAG, "Pins has flags set. Using Software Serial");
    return true;
  }

  return false;
}

bool LibreTinyUARTComponent::pins_contain_(const FixedVector<pin_size_t> &pins, pin_size_t pin_num) const {
  return pins.end() != std::find(pins.begin(), pins.end(), pin_num);
}

void LibreTinyUARTComponent::print_pins(const char *uart_name, const FixedVector<pin_size_t> &tx_pins,
                                        const FixedVector<pin_size_t> &rx_pins) const {
  if (tx_pins.empty() && rx_pins.empty()) {
    return;
  }
  for (size_t i = 0; i < tx_pins.size(); ++i) {
    for (size_t j = 0; j < rx_pins.size(); ++j) {
      ESP_LOGE(TAG, "    %s TX:%u, RX:%u", tx_pins[i], rx_pins[j]);
    }
  }
}

uint16_t LibreTinyUARTComponent::get_config() {
  uint16_t config = 0;

  switch (this->parity_) {
    case UART_CONFIG_PARITY_NONE:
      config |= SERIAL_PARITY_NONE;
      break;
    case UART_CONFIG_PARITY_EVEN:
      config |= SERIAL_PARITY_EVEN;
      break;
    case UART_CONFIG_PARITY_ODD:
      config |= SERIAL_PARITY_ODD;
      break;
  }

  config |= (this->data_bits_ - 4) << 8;
  config |= 0x10 + (this->stop_bits_ - 1) * 0x20;

  return config;
}

void LibreTinyUARTComponent::setup() {
  int8_t tx_pin = tx_pin_ == nullptr ? -1 : tx_pin_->get_pin();
  int8_t rx_pin = rx_pin_ == nullptr ? -1 : rx_pin_->get_pin();

  auto &uart_manager = this->lt_component_->get_uart_manager();

  auto fallback_to_sw_serial = shouldFallbackToSoftwareSerial(rx_pin_, tx_pin_);
  if (!fallback_to_sw_serial) {
    if (uart_manager.init_uart(0, this->baud_rate_, get_config(), rx_pin, tx_pin)) {
      this->hardware_idx_ = 0;
      this->serial_ = uart_manager.get_hw_serial_by_number(0);
    } else if (uart_manager.init_uart(1, this->baud_rate_, get_config(), rx_pin, tx_pin)) {
      this->hardware_idx_ = 1;
      this->serial_ = uart_manager.get_hw_serial_by_number(1);
    } else if (uart_manager.init_uart(2, this->baud_rate_, get_config(), rx_pin, tx_pin)) {
      this->hardware_idx_ = 2;
      this->serial_ = uart_manager.get_hw_serial_by_number(2);
    } else {
      fallback_to_sw_serial = true;
      ESP_LOGI(TAG, "Selected pins don't match any hardware UART. Falling back to Software Serial.");
    }
  }

  if (fallback_to_sw_serial) {
#if LT_ARD_HAS_SOFTSERIAL
    bool tx_inverted = tx_pin_ != nullptr && tx_pin_->is_inverted();
    bool rx_inverted = rx_pin_ != nullptr && rx_pin_->is_inverted();

    if (this->rx_pin_) {
      this->rx_pin_->setup();
    }
    if (this->tx_pin_ && this->rx_pin_ != this->tx_pin_) {
      this->tx_pin_->setup();
    }
    this->hardware_idx_ = -1;
    this->serial_ = new SoftwareSerial(rx_pin, tx_pin, rx_inverted || tx_inverted);
    this->serial_->begin(this->baud_rate_, get_config());
#else
    this->serial_ = uart_manager.get_hw_serial_by_number(LT_UART_DEFAULT_SERIAL);
    this->hardware_idx_ = LT_UART_DEFAULT_SERIAL;
    // use the default uart without changing pins
    (void) uart_manager.init_uart(LT_UART_DEFAULT_SERIAL, this->baud_rate_, get_config());

    ESP_LOGE(TAG, "  SoftwareSerial is not implemented for this chip. Only hardware pins are supported:");
    print_pins("UART0", uart_manager.get_tx_pins_for_uart(0), uart_manager.get_tx_pins_for_uart(0));
    print_pins("UART1", uart_manager.get_tx_pins_for_uart(1), uart_manager.get_tx_pins_for_uart(1));
    print_pins("UART2", uart_manager.get_tx_pins_for_uart(2), uart_manager.get_tx_pins_for_uart(2));
    this->mark_failed();
    return;
#endif
  }
}

void LibreTinyUARTComponent::dump_config() {
  bool is_software = this->hardware_idx_ == -1;
  ESP_LOGCONFIG(TAG,
                "UART Bus:\n"
                "  Type: %s",
                UART_TYPE[is_software]);
  if (!is_software) {
    ESP_LOGCONFIG(TAG, "  Port number: %d", this->hardware_idx_);
  }
  LOG_PIN("  TX Pin: ", tx_pin_);
  LOG_PIN("  RX Pin: ", rx_pin_);
  if (this->rx_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  RX Buffer Size: %u", this->rx_buffer_size_);
  }
  ESP_LOGCONFIG(TAG,
                "  Baud Rate: %u baud\n"
                "  Data Bits: %u\n"
                "  Parity: %s\n"
                "  Stop bits: %u",
                this->baud_rate_, this->data_bits_, LOG_STR_ARG(parity_to_str(this->parity_)), this->stop_bits_);
  this->check_logger_conflict();
}

void LibreTinyUARTComponent::write_array(const uint8_t *data, size_t len) {
  this->serial_->write(data, len);
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(UART_DIRECTION_TX, data[i]);
  }
#endif
}

bool LibreTinyUARTComponent::peek_byte(uint8_t *data) {
  if (!this->check_read_timeout_())
    return false;
  *data = this->serial_->peek();
  return true;
}

bool LibreTinyUARTComponent::read_array(uint8_t *data, size_t len) {
  if (!this->check_read_timeout_(len))
    return false;
  this->serial_->readBytes(data, len);
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(UART_DIRECTION_RX, data[i]);
  }
#endif
  return true;
}

int LibreTinyUARTComponent::available() { return this->serial_->available(); }
void LibreTinyUARTComponent::flush() {
  ESP_LOGVV(TAG, "    Flushing");
  this->serial_->flush();
}

void LibreTinyUARTComponent::check_logger_conflict() {
#ifdef USE_LOGGER
  if (this->hardware_idx_ == -1 || logger::global_logger->get_baud_rate() == 0) {
    return;
  }

  if (this->hardware_idx_ == logger::global_logger->get_hw_serial_number()) {
    ESP_LOGW(TAG, "  You're using the same serial port for logging and the UART component. Please "
                  "disable logging over the serial port by setting logger->baud_rate to 0.");
  }
#endif
}

}  // namespace esphome::uart
#endif  // USE_LIBRETINY
