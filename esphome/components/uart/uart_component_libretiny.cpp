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

// A board with more than one pin choice for a UART lists them all in PINS_SERIALn_RX/TX and leaves
// PIN_SERIALn_RX/TX as PIN_INVALID, so match against the list when the board has one.
#if LT_HW_UART0
#ifdef PINS_SERIAL0_RX
static constexpr pin_size_t SERIAL0_RX_PINS[] = PINS_SERIAL0_RX;
#else
static constexpr pin_size_t SERIAL0_RX_PINS[] = {PIN_SERIAL0_RX};
#endif
#ifdef PINS_SERIAL0_TX
static constexpr pin_size_t SERIAL0_TX_PINS[] = PINS_SERIAL0_TX;
#else
static constexpr pin_size_t SERIAL0_TX_PINS[] = {PIN_SERIAL0_TX};
#endif
#endif
#if LT_HW_UART1
#ifdef PINS_SERIAL1_RX
static constexpr pin_size_t SERIAL1_RX_PINS[] = PINS_SERIAL1_RX;
#else
static constexpr pin_size_t SERIAL1_RX_PINS[] = {PIN_SERIAL1_RX};
#endif
#ifdef PINS_SERIAL1_TX
static constexpr pin_size_t SERIAL1_TX_PINS[] = PINS_SERIAL1_TX;
#else
static constexpr pin_size_t SERIAL1_TX_PINS[] = {PIN_SERIAL1_TX};
#endif
#endif
#if LT_HW_UART2
#ifdef PINS_SERIAL2_RX
static constexpr pin_size_t SERIAL2_RX_PINS[] = PINS_SERIAL2_RX;
#else
static constexpr pin_size_t SERIAL2_RX_PINS[] = {PIN_SERIAL2_RX};
#endif
#ifdef PINS_SERIAL2_TX
static constexpr pin_size_t SERIAL2_TX_PINS[] = PINS_SERIAL2_TX;
#else
static constexpr pin_size_t SERIAL2_TX_PINS[] = {PIN_SERIAL2_TX};
#endif
#endif

// True when the pin is unused (-1) or one of the pins the UART can be routed to.
template<size_t N> static bool uart_pin_matches(int16_t pin, const pin_size_t (&pins)[N]) {
  if (pin == -1)
    return true;
  for (pin_size_t p : pins) {
    if (p != PIN_INVALID && p == pin)
      return true;
  }
  return false;
}

#if !LT_ARD_HAS_SOFTSERIAL
// Writes the usable pins as "11/14", or "none" when the board has no pin for this role.
template<size_t N> static void format_uart_pins(char *buf, size_t size, const pin_size_t (&pins)[N]) {
  size_t pos = 0;
  buf[0] = '\0';
  for (pin_size_t p : pins) {
    if (p == PIN_INVALID || pos >= size)
      continue;
    pos += snprintf(buf + pos, size - pos, pos == 0 ? "%u" : "/%u", p);
  }
  if (pos == 0)
    snprintf(buf, size, "none");
}

template<size_t T, size_t R>
static void log_uart_pins(uint8_t port, const pin_size_t (&tx_pins)[T], const pin_size_t (&rx_pins)[R]) {
  char tx[16];
  char rx[16];
  format_uart_pins(tx, sizeof(tx), tx_pins);
  format_uart_pins(rx, sizeof(rx), rx_pins);
  ESP_LOGE(TAG, "    UART%u: TX=%s, RX=%s", port, tx, rx);
}
#endif

static const char *const UART_TYPE[] = {
    "hardware",
    "software",
};

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

void LibreTinyUARTComponent::setup() {
  int16_t tx_pin = tx_pin_ == nullptr ? -1 : tx_pin_->get_pin();
  int16_t rx_pin = rx_pin_ == nullptr ? -1 : rx_pin_->get_pin();

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

  if (false) {  // NOLINT(readability-simplify-boolean-expr)
    return;
  }
#if LT_HW_UART0
  else if (uart_pin_matches(tx_pin, SERIAL0_TX_PINS) && uart_pin_matches(rx_pin, SERIAL0_RX_PINS) &&
           !should_fallback_to_software_serial()) {
    this->serial_ = &Serial0;
    this->hardware_idx_ = 0;
  }
#endif
#if LT_HW_UART1
  else if (uart_pin_matches(tx_pin, SERIAL1_TX_PINS) && uart_pin_matches(rx_pin, SERIAL1_RX_PINS) &&
           !should_fallback_to_software_serial()) {
    this->serial_ = &Serial1;
    this->hardware_idx_ = 1;
  }
#endif
#if LT_HW_UART2
  else if (uart_pin_matches(tx_pin, SERIAL2_TX_PINS) && uart_pin_matches(rx_pin, SERIAL2_RX_PINS) &&
           !should_fallback_to_software_serial()) {
    this->serial_ = &Serial2;
    this->hardware_idx_ = 2;
  }
#endif
  else {
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
#else
    this->serial_ = &Serial;
    ESP_LOGE(TAG, "  SoftwareSerial is not implemented for this chip. Only hardware pins are supported:");
#if LT_HW_UART0
    log_uart_pins(0, SERIAL0_TX_PINS, SERIAL0_RX_PINS);
#endif
#if LT_HW_UART1
    log_uart_pins(1, SERIAL1_TX_PINS, SERIAL1_RX_PINS);
#endif
#if LT_HW_UART2
    log_uart_pins(2, SERIAL2_TX_PINS, SERIAL2_RX_PINS);
#endif
    this->mark_failed(LOG_STR("SoftwareSerial is not implemented for this chip."));
    return;
#endif
  }

  if (this->hardware_idx_ >= 0) {
    libretiny::ensure_serial_handle(this->hardware_idx_);
    // Pass the pins so a UART with several pin choices is routed to the configured ones.
    auto *hw = static_cast<SerialClass *>(this->serial_);
    hw->begin(this->baud_rate_, get_config(), rx_pin == -1 ? PIN_INVALID : rx_pin, tx_pin == -1 ? PIN_INVALID : tx_pin);
  } else {
    this->serial_->begin(this->baud_rate_, get_config());
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

}  // namespace esphome::uart
#endif  // USE_LIBRETINY
