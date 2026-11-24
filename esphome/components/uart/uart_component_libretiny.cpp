#ifdef USE_LIBRETINY

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "uart_component_libretiny.h"

#include "esphome/components/libretiny/core.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

#if LT_ARD_HAS_SOFTSERIAL
#include <SoftwareSerial.h>
#endif

namespace esphome::uart {

ESPHOME_LOG_TAG(TAG, "uart");

static const char *const UART_TYPE[] = {
    "hardware",
    "software",
};

// Port whose default pins match, for a board codegen has no pin data for; PIN_INVALID means not configured.
static int8_t default_port(pin_size_t tx, pin_size_t rx) {
#if LT_HW_UART0
  if ((tx == PIN_INVALID || tx == PIN_SERIAL0_TX) && (rx == PIN_INVALID || rx == PIN_SERIAL0_RX))
    return 0;
#endif
#if LT_HW_UART1
  if ((tx == PIN_INVALID || tx == PIN_SERIAL1_TX) && (rx == PIN_INVALID || rx == PIN_SERIAL1_RX))
    return 1;
#endif
#if LT_HW_UART2
  if ((tx == PIN_INVALID || tx == PIN_SERIAL2_TX) && (rx == PIN_INVALID || rx == PIN_SERIAL2_RX))
    return 2;
#endif
  return -1;
}

static SerialClass *hardware_serial(int8_t port) {
  switch (port) {
#if LT_HW_UART0
    case 0:
      return &Serial0;
#endif
#if LT_HW_UART1
    case 1:
      return &Serial1;
#endif
#if LT_HW_UART2
    case 2:
      return &Serial2;
#endif
    default:
      return nullptr;
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
    default:
      break;
  }

  config |= (this->data_bits_ - 4) << 8;
  config |= 0x10 + (this->stop_bits_ - 1) * 0x20;

  return config;
}

void LibreTinyUARTComponent::open_hardware_port_([[maybe_unused]] pin_size_t tx_pin,
                                                 [[maybe_unused]] pin_size_t rx_pin) {
  SerialClass *hardware = hardware_serial(this->hardware_idx_);
  if (hardware == nullptr)
    return;
  // LN882x: the port needs its SDK handle, at boot and after an end()
  libretiny::ensure_serial_handle(this->hardware_idx_);
#ifdef USE_UART_LIBRETINY_PIN_ROUTING
  // Routes a port with pin choices to the configured pins (LibreTiny 1.12+, checked in codegen). setPins()
  // replaces a left-out pin (PIN_INVALID) with the port's default, so default-pin ports start as before.
  hardware->begin(this->baud_rate_, this->get_config(), rx_pin, tx_pin);
#else
  hardware->begin(this->baud_rate_, this->get_config());
#endif
}

void LibreTinyUARTComponent::setup() {
  const pin_size_t tx_pin = this->tx_pin_ == nullptr ? PIN_INVALID : this->tx_pin_->get_pin();
  const pin_size_t rx_pin = this->rx_pin_ == nullptr ? PIN_INVALID : this->rx_pin_->get_pin();

  auto should_fallback_to_software_serial = [&]() -> bool {
    auto has_flags = [](InternalGPIOPin *pin, const gpio::Flags mask) -> bool {
      return pin && (pin->get_flags() & mask) != gpio::Flags::FLAG_NONE;
    };
    if (has_flags(this->tx_pin_,
                  gpio::Flags::FLAG_OPEN_DRAIN | gpio::Flags::FLAG_PULLUP | gpio::Flags::FLAG_PULLDOWN) ||
        has_flags(this->rx_pin_,
                  gpio::Flags::FLAG_OPEN_DRAIN | gpio::Flags::FLAG_PULLUP | gpio::Flags::FLAG_PULLDOWN)) {
#if LT_ARD_HAS_SOFTSERIAL
      ESP_LOGI(TAG, "Pins have flags set. Using Software Serial");
      return true;
#else
      ESP_LOGW(TAG, "Pin flags are set but not supported for hardware serial. Ignoring");
#endif
    }
    return false;
  };

  // Codegen picks the port from the board's pin lists; a board it has no pin data for matches the defaults here.
  if (this->hardware_idx_ < 0)
    this->hardware_idx_ = default_port(tx_pin, rx_pin);
  SerialClass *hardware = should_fallback_to_software_serial() ? nullptr : hardware_serial(this->hardware_idx_);
  if (hardware != nullptr) {
    this->serial_ = hardware;
    this->open_hardware_port_(tx_pin, rx_pin);
    return;
  }
  this->hardware_idx_ = -1;

#if LT_ARD_HAS_SOFTSERIAL
  if (this->rx_pin_) {
    this->rx_pin_->setup();
  }
  if (this->tx_pin_ && this->rx_pin_ != this->tx_pin_) {
    this->tx_pin_->setup();
  }
  bool tx_inverted = tx_pin_ != nullptr && tx_pin_->is_inverted();
  bool rx_inverted = rx_pin_ != nullptr && rx_pin_->is_inverted();
  this->serial_ = new SoftwareSerial(rx_pin, tx_pin, rx_inverted || tx_inverted);
  this->serial_->begin(this->baud_rate_, this->get_config());
#else
  this->serial_ = &Serial;
  ESP_LOGE(TAG, "  SoftwareSerial is not implemented for this chip. Only hardware pins are supported:");
#if LT_HW_UART0
  ESP_LOGE(TAG, "    TX=%u, RX=%u", PIN_SERIAL0_TX, PIN_SERIAL0_RX);
#endif
#if LT_HW_UART1
  ESP_LOGE(TAG, "    TX=%u, RX=%u", PIN_SERIAL1_TX, PIN_SERIAL1_RX);
#endif
#if LT_HW_UART2
  ESP_LOGE(TAG, "    TX=%u, RX=%u", PIN_SERIAL2_TX, PIN_SERIAL2_RX);
#endif
  this->mark_failed(LOG_STR("SoftwareSerial is not implemented for this chip."));
#endif
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
                "  Baud Rate: %" PRIu32 " baud\n"
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

size_t LibreTinyUARTComponent::available() { return this->serial_->available(); }
UARTFlushResult LibreTinyUARTComponent::flush() {
  ESP_LOGVV(TAG, "    Flushing");
  this->serial_->flush();
  return UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS;
}

void LibreTinyUARTComponent::check_logger_conflict() {
#ifdef USE_LOGGER
  if (this->hardware_idx_ == -1 || logger::global_logger->get_baud_rate() == 0) {
    return;
  }

  if (this->serial_ == logger::global_logger->get_hw_serial()) {
    ESP_LOGW(TAG, "  You're using the same serial port for logging and the UART component. Please "
                  "disable logging over the serial port by setting logger->baud_rate to 0.");
  }
#endif
}

#ifdef USE_UART_TX_BREAK
bool LibreTinyUARTComponent::set_tx_break(bool enable) {
  if (this->tx_pin_ == nullptr || this->serial_ == nullptr || this->is_failed())
    return false;
  SerialClass *hardware = hardware_serial(this->hardware_idx_);
  // software serial sends from a timer and its flush() does not wait for TX: the pin cannot be held
  if (hardware == nullptr)
    return false;
  // a hardware port does not invert its TX signal, the pin level would be the idle level
  if (this->tx_pin_->is_inverted())
    return false;
#ifdef USE_LOGGER
  // a logger that writes to this port would use it while it is closed
  if (logger::global_logger->get_baud_rate() != 0 && this->serial_ == logger::global_logger->get_hw_serial())
    return false;
#endif
  if (enable == this->tx_break_)
    return true;
  this->tx_break_ = enable;
  if (enable) {
    // the Arduino API has no break: the port is closed and the pin driven as a GPIO
    hardware->end();
    this->tx_pin_->setup();
    this->tx_pin_->digital_write(false);
    return true;
  }
#ifndef USE_BK72XX
  // BK72xx: giving the pin back pulls it down, so it stays low until the port drives it: one edge
  this->tx_pin_->digital_write(true);
#endif
  // The GPIO driver gives the pin back: RTL8720C begin() cannot take a pin it still holds,
  // and LibreTiny would skip the setup of the next break for a pin on record as an output
  const pin_size_t tx_pin = this->tx_pin_->get_pin();
  pinModeRemove(tx_pin, PIN_GPIO);
  this->open_hardware_port_(tx_pin, this->rx_pin_ == nullptr ? PIN_INVALID : this->rx_pin_->get_pin());
  return true;
}
#endif

}  // namespace esphome::uart
#endif  // USE_LIBRETINY
