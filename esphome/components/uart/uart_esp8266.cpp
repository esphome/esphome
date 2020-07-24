#ifdef ARDUINO_ARCH_ESP8266
#include "uart.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#
namespace esphome {
namespace uart {

static const char *TAG = "uart_esp8266";
uint32_t UARTComponent::get_config() {
  uint32_t config = 0;

  if (this->parity_ == UART_CONFIG_PARITY_NONE)
    config |= UART_PARITY_NONE;
  else if (this->parity_ == UART_CONFIG_PARITY_EVEN)
    config |= UART_PARITY_EVEN;
  else if (this->parity_ == UART_CONFIG_PARITY_ODD)
    config |= UART_PARITY_ODD;

  switch (this->data_bits_) {
    case 5:
      config |= UART_NB_BIT_5;
      break;
    case 6:
      config |= UART_NB_BIT_6;
      break;
    case 7:
      config |= UART_NB_BIT_7;
      break;
    case 8:
      config |= UART_NB_BIT_8;
      break;
  }

  if (this->stop_bits_ == 1)
    config |= UART_NB_STOP_BIT_1;
  else
    config |= UART_NB_STOP_BIT_2;

  return config;
}

void UARTComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up UART bus...");
  // Use Arduino HardwareSerial UARTs if all used pins match the ones
  // preconfigured by the platform. For example if RX disabled but TX pin
  // is 1 we still want to use Serial.
  SerialConfig config = static_cast<SerialConfig>(get_config());

  if (this->tx_pin_.value_or(1) == 1 && this->rx_pin_.value_or(3) == 3) {
    this->hw_serial_ = &Serial;
    this->hw_serial_->begin(this->baud_rate_, config);
    this->hw_serial_->setRxBufferSize(this->rx_buffer_size_);
  } else if (this->tx_pin_.value_or(15) == 15 && this->rx_pin_.value_or(13) == 13) {
    this->hw_serial_ = &Serial;
    this->hw_serial_->begin(this->baud_rate_, config);
    this->hw_serial_->setRxBufferSize(this->rx_buffer_size_);
    this->hw_serial_->swap();
  } else if (this->tx_pin_.value_or(2) == 2 && this->rx_pin_.value_or(8) == 8) {
    this->hw_serial_ = &Serial1;
    this->hw_serial_->begin(this->baud_rate_, config);
    this->hw_serial_->setRxBufferSize(this->rx_buffer_size_);
  } else {
    this->sw_serial_ = new ESP8266SoftwareSerial();
    int8_t tx = this->tx_pin_.has_value() ? *this->tx_pin_ : -1;
    int8_t rx = this->rx_pin_.has_value() ? *this->rx_pin_ : -1;
    this->sw_serial_->setup(tx, rx, this->baud_rate_, this->stop_bits_, this->data_bits_, this->parity_,
                            this->rx_buffer_size_);
  }
}

void UARTComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "UART Bus:");
  if (this->tx_pin_.has_value()) {
    ESP_LOGCONFIG(TAG, "  TX Pin: GPIO%d", *this->tx_pin_);
  }
  if (this->rx_pin_.has_value()) {
    ESP_LOGCONFIG(TAG, "  RX Pin: GPIO%d", *this->rx_pin_);
    ESP_LOGCONFIG(TAG, "  RX Buffer Size: %u", this->rx_buffer_size_);  // NOLINT
  }
  ESP_LOGCONFIG(TAG, "  Baud Rate: %u baud", this->baud_rate_);
  ESP_LOGCONFIG(TAG, "  Data Bits: %u", this->data_bits_);
  ESP_LOGCONFIG(TAG, "  Parity: %s", parity_to_str(this->parity_));
  ESP_LOGCONFIG(TAG, "  Stop bits: %u", this->stop_bits_);
  if (this->hw_serial_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Using hardware serial interface.");
  } else {
    ESP_LOGCONFIG(TAG, "  Using software serial");
  }
  this->check_logger_conflict_();
}

void UARTComponent::write_byte(uint8_t data) {
  if (this->hw_serial_ != nullptr) {
    this->hw_serial_->write(data);
  } else {
    this->sw_serial_->write_byte(data);
  }
  ESP_LOGVV(TAG, "    Wrote 0b" BYTE_TO_BINARY_PATTERN " (0x%02X)", BYTE_TO_BINARY(data), data);
}
void UARTComponent::write_array(const uint8_t *data, size_t len) {
  if (this->hw_serial_ != nullptr) {
    this->hw_serial_->write(data, len);
  } else {
    for (size_t i = 0; i < len; i++)
      this->sw_serial_->write_byte(data[i]);
  }
  for (size_t i = 0; i < len; i++) {
    ESP_LOGVV(TAG, "    Wrote 0b" BYTE_TO_BINARY_PATTERN " (0x%02X)", BYTE_TO_BINARY(data[i]), data[i]);
  }
}
void UARTComponent::write_str(const char *str) {
  if (this->hw_serial_ != nullptr) {
    this->hw_serial_->write(str);
  } else {
    const auto *data = reinterpret_cast<const uint8_t *>(str);
    for (size_t i = 0; data[i] != 0; i++)
      this->sw_serial_->write_byte(data[i]);
  }
  ESP_LOGVV(TAG, "    Wrote \"%s\"", str);
}
bool UARTComponent::read_byte(uint8_t *data) {
  if (!this->check_read_timeout_())
    return false;
  if (this->hw_serial_ != nullptr) {
    *data = this->hw_serial_->read();
  } else {
    *data = this->sw_serial_->read_byte();
  }
  ESP_LOGVV(TAG, "    Read 0b" BYTE_TO_BINARY_PATTERN " (0x%02X)", BYTE_TO_BINARY(*data), *data);
  return true;
}
bool UARTComponent::peek_byte(uint8_t *data) {
  if (!this->check_read_timeout_())
    return false;
  if (this->hw_serial_ != nullptr) {
    *data = this->hw_serial_->peek();
  } else {
    *data = this->sw_serial_->peek_byte();
  }
  return true;
}
bool UARTComponent::read_array(uint8_t *data, size_t len) {
  if (!this->check_read_timeout_(len))
    return false;
  if (this->hw_serial_ != nullptr) {
    this->hw_serial_->readBytes(data, len);
  } else {
    for (size_t i = 0; i < len; i++)
      data[i] = this->sw_serial_->read_byte();
  }
  for (size_t i = 0; i < len; i++) {
    ESP_LOGVV(TAG, "    Read 0b" BYTE_TO_BINARY_PATTERN " (0x%02X)", BYTE_TO_BINARY(data[i]), data[i]);
  }

  return true;
}
bool UARTComponent::check_read_timeout_(size_t len) {
  if (this->available() >= int(len))
    return true;

  uint32_t start_time = millis();
  while (this->available() < int(len)) {
    if (millis() - start_time > 100) {
      ESP_LOGE(TAG, "Reading from UART timed out at byte %u!", this->available());
      return false;
    }
    yield();
  }
  return true;
}
int UARTComponent::available() {
  if (this->hw_serial_ != nullptr) {
    return this->hw_serial_->available();
  } else {
    return this->sw_serial_->available();
  }
}
void UARTComponent::flush() {
  ESP_LOGVV(TAG, "    Flushing...");
  if (this->hw_serial_ != nullptr) {
    this->hw_serial_->flush();
  } else {
    this->sw_serial_->flush();
  }
}
void ESP8266SoftwareSerial::setup(int8_t tx_pin, int8_t rx_pin, uint32_t baud_rate, uint8_t stop_bits,
                                  uint32_t data_bits, UARTParityOptions parity, size_t rx_buffer_size) {
  this->bit_time_ = F_CPU / baud_rate;
  this->rx_buffer_size_ = rx_buffer_size;
  this->stop_bits_ = stop_bits;
  this->data_bits_ = data_bits;
  this->parity_ = parity;
  if (tx_pin != -1) {
    auto pin = GPIOPin(tx_pin, OUTPUT);
    this->gpio_tx_pin_ = &pin;
    pin.setup();
    this->tx_pin_ = pin.to_isr();
    this->tx_pin_->digital_write(true);
  }
  if (rx_pin != -1) {
    auto pin = GPIOPin(rx_pin, INPUT);
    pin.setup();
    this->gpio_rx_pin_ = &pin;
    this->rx_pin_ = pin.to_isr();
    this->rx_buffer_ = new uint8_t[this->rx_buffer_size_];
    pin.attach_interrupt(ESP8266SoftwareSerial::gpio_intr, this, FALLING);
  }
}
void ICACHE_RAM_ATTR ESP8266SoftwareSerial::gpio_intr(ESP8266SoftwareSerial *arg) {
  uint32_t wait = arg->bit_time_ + arg->bit_time_ / 3 - 500;
  const uint32_t start = ESP.getCycleCount();
  uint8_t rec = 0;
  // Manually unroll the loop
  for (int i = 0; i < arg->data_bits_; i++)
    rec |= arg->read_bit_(&wait, start) << i;

  /* If parity is enabled, just read it and ignore it. */
  /* TODO: Should we check parity? Or is it too slow for nothing added..*/
  if (arg->parity_ == UART_CONFIG_PARITY_EVEN)
    arg->read_bit_(&wait, start);
  else if (arg->parity_ == UART_CONFIG_PARITY_ODD)
    arg->read_bit_(&wait, start);

  // Stop bit
  arg->wait_(&wait, start);
  if (arg->stop_bits_ == 2)
    arg->wait_(&wait, start);

  arg->rx_buffer_[arg->rx_in_pos_] = rec;
  arg->rx_in_pos_ = (arg->rx_in_pos_ + 1) % arg->rx_buffer_size_;
  // Clear RX pin so that the interrupt doesn't re-trigger right away again.
  arg->rx_pin_->clear_interrupt();
}
void ICACHE_RAM_ATTR HOT ESP8266SoftwareSerial::write_byte(uint8_t data) {
  if (this->tx_pin_ == nullptr) {
    ESP_LOGE(TAG, "UART doesn't have TX pins set!");
    return;
  }
  bool parity_bit = false;
  bool need_parity_bit = true;
  if (this->parity_ == UART_CONFIG_PARITY_EVEN)
    parity_bit = true;
  else if (this->parity_ == UART_CONFIG_PARITY_ODD)
    parity_bit = false;
  else
    need_parity_bit = false;

  {
    InterruptLock lock;
    uint32_t wait = this->bit_time_;
    const uint32_t start = ESP.getCycleCount();
    // Start bit
    this->write_bit_(false, &wait, start);
    for (int i = 0; i < this->data_bits_; i++) {
      bool bit = data & (1 << i);
      this->write_bit_(bit, &wait, start);
      if (need_parity_bit)
        parity_bit ^= bit;
    }
    if (need_parity_bit)
      this->write_bit_(parity_bit, &wait, start);
    // Stop bit
    this->write_bit_(true, &wait, start);
    if (this->stop_bits_ == 2)
      this->wait_(&wait, start);
  }
}
void ICACHE_RAM_ATTR ESP8266SoftwareSerial::wait_(uint32_t *wait, const uint32_t &start) {
  while (ESP.getCycleCount() - start < *wait)
    ;
  *wait += this->bit_time_;
}
bool ICACHE_RAM_ATTR ESP8266SoftwareSerial::read_bit_(uint32_t *wait, const uint32_t &start) {
  this->wait_(wait, start);
  return this->rx_pin_->digital_read();
}
void ICACHE_RAM_ATTR ESP8266SoftwareSerial::write_bit_(bool bit, uint32_t *wait, const uint32_t &start) {
  this->tx_pin_->digital_write(bit);
  this->wait_(wait, start);
}
uint8_t ESP8266SoftwareSerial::read_byte() {
  if (this->rx_in_pos_ == this->rx_out_pos_)
    return 0;
  uint8_t data = this->rx_buffer_[this->rx_out_pos_];
  this->rx_out_pos_ = (this->rx_out_pos_ + 1) % this->rx_buffer_size_;
  return data;
}
uint8_t ESP8266SoftwareSerial::peek_byte() {
  if (this->rx_in_pos_ == this->rx_out_pos_)
    return 0;
  return this->rx_buffer_[this->rx_out_pos_];
}
void ESP8266SoftwareSerial::flush() {
  // Flush is a NO-OP with software serial, all bytes are written immediately.
}
int ESP8266SoftwareSerial::available() {
  int avail = int(this->rx_in_pos_) - int(this->rx_out_pos_);
  if (avail < 0)
    return avail + this->rx_buffer_size_;
  return avail;
}

}  // namespace uart
}  // namespace esphome
#endif  // ARDUINO_ARCH_ESP8266
