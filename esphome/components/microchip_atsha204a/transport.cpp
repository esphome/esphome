#include "transport.h"

#include <algorithm>

#include "esphome/core/gpio_pin.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#if defined(USE_MICROCHIP_ATSHA204A_SWI_UART) && defined(USE_RP2)
#include "esphome/components/uart/uart_component_rp2.h"
#elif defined(USE_MICROCHIP_ATSHA204A_SWI_UART) && defined(USE_LIBRETINY)
#include "esphome/components/uart/uart_component_libretiny.h"
#endif
#ifdef USE_MICROCHIP_ATSHA204A_SWI_RMT
#include "driver/gpio.h"
#include "esp_err.h"
#endif

namespace esphome::microchip_atsha204a {

ESPHOME_LOG_TAG(TAG, "microchip_atsha204a");
static constexpr uint8_t MIN_RESPONSE_SIZE = 4;

#ifdef USE_MICROCHIP_ATSHA204A_I2C
static constexpr uint8_t I2C_COMMAND = 0x03;
static constexpr uint8_t I2C_IDLE = 0x02;
static constexpr uint8_t I2C_SLEEP = 0x01;
// Keep transfers within the smallest supported Wire buffer (32 bytes).
static constexpr size_t I2C_CHUNK_SIZE = 30;

void I2CTransport::dump_config() {
  ESP_LOGCONFIG(TAG, "  Transport: I2C");
  LOG_I2C_DEVICE(this);
}

bool I2CTransport::wake() {
  // The address byte holds SDA low for eight clocks. A sleeping chip is expected to NACK.
  const uint8_t zero = 0;
  const auto status = this->bus_->write(0, &zero, 1);
  return status == i2c::ERROR_OK || status == i2c::ERROR_NOT_ACKNOWLEDGED;
}

bool I2CTransport::command(const uint8_t *packet, size_t size) {
  uint8_t data[I2C_CHUNK_SIZE + 1]{I2C_COMMAND};
  while (size != 0) {
    // Each I2C write repeats the word address and continues at the chip's input counter.
    const size_t count = std::min(size, I2C_CHUNK_SIZE);
    std::copy_n(packet, count, data + 1);
    if (this->write(data, count + 1) != i2c::ERROR_OK)
      return false;
    packet += count;
    size -= count;
  }
  return true;
}

Receive I2CTransport::receive(uint8_t *packet) {
  const auto status = this->read(packet, 1);
  if (status == i2c::ERROR_NOT_ACKNOWLEDGED)
    return Receive::RECEIVE_PENDING;
  if (status != i2c::ERROR_OK)
    return Receive::RECEIVE_IO_ERROR;
  if (packet[0] < MIN_RESPONSE_SIZE || packet[0] > MAX_RESPONSE)
    return Receive::RECEIVE_INVALID_LENGTH;
  for (size_t offset = 1; offset < packet[0];) {
    const size_t count = std::min<size_t>(packet[0] - offset, I2C_CHUNK_SIZE);
    if (this->read(packet + offset, count) != i2c::ERROR_OK)
      return Receive::RECEIVE_IO_ERROR;
    offset += count;
  }
  return Receive::RECEIVE_COMPLETE;
}

bool I2CTransport::power_down(bool idle, bool allow_nack) {
  const uint8_t flag = idle ? I2C_IDLE : I2C_SLEEP;
  const auto status = this->write(&flag, 1);
  return status == i2c::ERROR_OK || (allow_nack && status == i2c::ERROR_NOT_ACKNOWLEDGED);
}
#endif

#if defined(USE_MICROCHIP_ATSHA204A_SWI_UART) || defined(USE_MICROCHIP_ATSHA204A_SWI_RMT)
static constexpr uint8_t decode_byte(const uint8_t *tokens) {
  uint8_t value = 0;
  for (unsigned bit = 0; bit < 8; bit++) {
    if (tokens[bit] == 0x7F || tokens[bit] == 0x7E)
      value |= 1U << bit;
  }
  return value;
}

static constexpr uint8_t SWI_COMMAND = 0x77;
static constexpr uint8_t SWI_TRANSMIT = 0x88;
static constexpr uint8_t SWI_IDLE = 0xBB;
static constexpr uint8_t SWI_SLEEP = 0xCC;
static constexpr uint32_t SWI_WAKE_US = 80;
static constexpr uint32_t SWI_TURNAROUND_US = 100;
static constexpr uint8_t TRANSMIT_ECHO[] = {0x7D, 0x7D, 0x7D, 0x7F, 0x7D, 0x7D, 0x7D, 0x7E};
static_assert(decode_byte(TRANSMIT_ECHO) == SWI_TRANSMIT);

void SWITransport::wait_to_send_() {
  if (this->received_packet_) {
    const uint32_t elapsed = micros() - this->received_at_;
    if (elapsed < SWI_TURNAROUND_US)
      delayMicroseconds(SWI_TURNAROUND_US - elapsed);
    this->received_packet_ = false;
  }
}

bool SWITransport::command(const uint8_t *packet, size_t size) {
  this->wait_to_send_();
  uint8_t frame[1 + MAX_COMMAND]{SWI_COMMAND};
  std::copy_n(packet, size, frame + 1);
  return this->send(frame, size + 1);
}

bool SWITransport::request_response() {
  this->wait_to_send_();
  this->discard_available();
  this->tokens_received_ = 0;
  return this->send(&SWI_TRANSMIT, 1);
}

Receive SWITransport::receive(uint8_t *packet) {
  const size_t count = std::min(this->available(), this->tokens_.size() - this->tokens_received_);
  if (count != 0 && !this->read_array(this->tokens_.data() + this->tokens_received_, count))
    return Receive::RECEIVE_IO_ERROR;
  this->tokens_received_ += count;
  if (this->tokens_received_ < 16)
    return Receive::RECEIVE_PENDING;
  const uint8_t size = decode_byte(this->tokens_.data() + 8);
  if (decode_byte(this->tokens_.data()) != SWI_TRANSMIT || size < MIN_RESPONSE_SIZE || size > MAX_RESPONSE)
    return Receive::RECEIVE_INVALID_LENGTH;
  if (this->tokens_received_ < (1U + size) * 8)
    return Receive::RECEIVE_PENDING;
  for (size_t i = 0; i < size; i++)
    packet[i] = decode_byte(this->tokens_.data() + (1 + i) * 8);
  this->received_at_ = micros();
  this->received_packet_ = true;
  return Receive::RECEIVE_COMPLETE;
}

bool SWITransport::power_down(bool idle, bool) {
  this->wait_to_send_();
  const uint8_t flag = idle ? SWI_IDLE : SWI_SLEEP;
  return this->send(&flag, 1);
}
#endif

#ifdef USE_MICROCHIP_ATSHA204A_SWI_UART
bool UARTTransport::setup() {
#if defined(USE_ESP8266)
  // The ESP8266 software UART reports unlimited write space because it has no TX FIFO.
  const bool hardware_uart = this->parent_->available_for_write() != SIZE_MAX;
#elif defined(USE_RP2)
  const bool hardware_uart = static_cast<uart::RP2UartComponent *>(this->parent_)->is_hw_serial();
#elif defined(USE_LIBRETINY)
  const bool hardware_uart = static_cast<uart::LibreTinyUARTComponent *>(this->parent_)->get_hw_serial_number() >= 0;
#else
  constexpr bool hardware_uart = true;
#endif
  if (!hardware_uart) {
    ESP_LOGE(TAG, "Single-wire UART requires a hardware UART");
    return false;
  }
  setup_output_pin(this->wake_pin_, true);
  return true;
}

bool UARTTransport::wake() {
  this->wake_pin_->digital_write(false);
  delayMicroseconds(SWI_WAKE_US);
  this->wake_pin_->digital_write(true);
  return true;
}

bool UARTTransport::send(const uint8_t *bytes, size_t size) {
  uint8_t tokens[16];
  while (size != 0) {
    const size_t count = std::min<size_t>(2, size);
    for (size_t i = 0; i < count; i++) {
      for (unsigned bit = 0; bit < 8; bit++)
        tokens[i * 8 + bit] = (bytes[i] & (1U << bit)) ? 0x7F : 0x7D;
    }
    this->write_array(tokens, count * 8);
    bytes += count;
    size -= count;
  }
  const auto status = this->flush();
  return status == uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS ||
         status == uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS;
}

void UARTTransport::dump_config() {
  ESP_LOGCONFIG(TAG, "  Transport: Single-wire UART");
  LOG_PIN("  Wake pin: ", this->wake_pin_);
}

#endif

#ifdef USE_MICROCHIP_ATSHA204A_SWI_RMT
static constexpr uint32_t RMT_RESOLUTION_HZ = 2000000;
static constexpr size_t RMT_SYMBOLS = 64;
static constexpr uint8_t RMT_WAKE = 0;
static constexpr size_t RMT_SYMBOLS_PER_BYTE = 16;

static size_t RMT_ENCODER_FUNC_ATTR encode_swi(const void *data, size_t size, size_t written, size_t free,
                                               rmt_symbol_word_t *symbols, bool *done, void *) {
  const auto *bytes = static_cast<const uint8_t *>(data);
  const size_t offset = written / RMT_SYMBOLS_PER_BYTE;
  if (size == 1 && bytes[0] == RMT_WAKE) {
    symbols[0] = {.duration0 = SWI_WAKE_US * (RMT_RESOLUTION_HZ / 1000000), .level0 = 0, .duration1 = 2, .level1 = 1};
    *done = true;
    return 1;
  }

  size_t count = 0;
  while (offset + count < size && free >= RMT_SYMBOLS_PER_BYTE) {
    const uint8_t byte = bytes[offset + count++];
    // At 2 MHz each symbol duration is in half microseconds. Both tokens last 39 us.
    for (unsigned bit = 0; bit < 8; bit++) {
      const bool one = (byte & (1U << bit)) != 0;
      *symbols++ = {.duration0 = 9, .level0 = 0, .duration1 = 12, .level1 = 1};
      *symbols++ = {.duration0 = 9, .level0 = static_cast<uint16_t>(one), .duration1 = 48, .level1 = 1};
    }
    free -= RMT_SYMBOLS_PER_BYTE;
  }
  *done = offset + count == size;
  return count * RMT_SYMBOLS_PER_BYTE;
}

bool RMTTransport::setup() {
  const auto gpio = static_cast<gpio_num_t>(this->pin_);
  rmt_tx_channel_config_t tx{};
  tx.gpio_num = gpio;
  tx.clk_src = RMT_CLK_SRC_DEFAULT;
  tx.resolution_hz = RMT_RESOLUTION_HZ;
  tx.mem_block_symbols = RMT_SYMBOLS;
  tx.trans_queue_depth = 1;
  tx.flags.init_level = true;
  rmt_simple_encoder_config_t encoder{};
  encoder.callback = encode_swi;
  encoder.min_chunk_size = RMT_SYMBOLS_PER_BYTE;

  // The UART bus has already enabled input. Add open-drain output on the same GPIO.
  if (gpio_od_enable(gpio) != ESP_OK || rmt_new_tx_channel(&tx, &this->tx_) != ESP_OK ||
      rmt_new_simple_encoder(&encoder, &this->encoder_) != ESP_OK || rmt_enable(this->tx_) != ESP_OK) {
    if (this->encoder_ != nullptr)
      rmt_del_encoder(this->encoder_);
    if (this->tx_ != nullptr)
      rmt_del_channel(this->tx_);
    return false;
  }
  return true;
}

void RMTTransport::dump_config() { ESP_LOGCONFIG(TAG, "  Transport: Single-wire RMT (UART RX on GPIO%u)", this->pin_); }

bool RMTTransport::wake() { return this->transmit_(&RMT_WAKE, 1); }

bool RMTTransport::transmit_(const uint8_t *bytes, size_t size) {
  // A timed-out transaction can still own the frame. Recover when the driver releases it.
  if (rmt_tx_wait_all_done(this->tx_, 0) != ESP_OK)
    return false;

  std::copy_n(bytes, size, this->frame_.begin());
  rmt_transmit_config_t config{};
  config.flags.eot_level = true;
  const auto status = rmt_transmit(this->tx_, this->encoder_, this->frame_.data(), size, &config);
  if (status == ESP_OK) {
    // A maximum-length frame takes about 27 ms.
    const auto wait_status = rmt_tx_wait_all_done(this->tx_, 40);
    if (wait_status != ESP_OK) {
      ESP_LOGW(TAG, "RMT transmit did not complete: %s", esp_err_to_name(wait_status));
      return false;
    }
  } else {
    ESP_LOGW(TAG, "RMT transmit failed: %s", esp_err_to_name(status));
  }
  this->frame_.fill(0);
  return status == ESP_OK;
}

bool RMTTransport::send(const uint8_t *bytes, size_t size) { return this->transmit_(bytes, size); }

#endif

}  // namespace esphome::microchip_atsha204a
