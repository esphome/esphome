#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/gpio.h"

#include <array>
#include <cstddef>
#include <cstdint>

#ifdef USE_MICROCHIP_ATSHA204A_I2C
#include "esphome/components/i2c/i2c.h"
#endif
#if defined(USE_MICROCHIP_ATSHA204A_SWI_UART) || defined(USE_MICROCHIP_ATSHA204A_SWI_RMT)
#include "esphome/components/uart/uart.h"
#endif
#ifdef USE_MICROCHIP_ATSHA204A_SWI_RMT
#include "driver/rmt_tx.h"
#endif

namespace esphome::microchip_atsha204a {

inline constexpr size_t MAX_COMMAND = 84;
inline constexpr size_t MAX_RESPONSE = 35;

enum class Receive : uint8_t { RECEIVE_PENDING, RECEIVE_COMPLETE, RECEIVE_INVALID_LENGTH, RECEIVE_IO_ERROR };

class Transport {
 public:
  virtual ~Transport() = default;
  virtual bool setup() = 0;
  virtual void dump_config() = 0;
  virtual bool is_single_wire() const { return false; }
  virtual bool wake() = 0;
  virtual bool command(const uint8_t *packet, size_t size) = 0;
  virtual bool request_response() = 0;
  virtual Receive receive(uint8_t *packet) = 0;
  virtual bool power_down(bool idle, bool allow_nack = false) = 0;
};

#ifdef USE_MICROCHIP_ATSHA204A_I2C
class I2CTransport : public Transport, public i2c::I2CDevice {
 public:
  bool setup() override { return true; }
  void dump_config() override;
  bool wake() override;
  bool command(const uint8_t *packet, size_t size) override;
  bool request_response() override { return true; }
  Receive receive(uint8_t *packet) override;
  bool power_down(bool idle, bool allow_nack) override;
};
#endif

#if defined(USE_MICROCHIP_ATSHA204A_SWI_UART) || defined(USE_MICROCHIP_ATSHA204A_SWI_RMT)
class SWITransport : public Transport, public uart::UARTDevice {
 public:
  bool is_single_wire() const override { return true; }
  bool command(const uint8_t *packet, size_t size) override;
  bool request_response() override;
  Receive receive(uint8_t *packet) override;
  bool power_down(bool idle, bool allow_nack) override;

 protected:
  virtual bool send(const uint8_t *bytes, size_t size) = 0;
  void wait_to_send_();

  std::array<uint8_t, (1 + MAX_RESPONSE) * 8> tokens_{};
  size_t tokens_received_{};
  uint32_t received_at_{};
  bool received_packet_{};
};
#endif

#ifdef USE_MICROCHIP_ATSHA204A_SWI_UART
class UARTTransport : public SWITransport {
 public:
  explicit UARTTransport(InternalGPIOPin *wake_pin) : wake_pin_(wake_pin) {}
  bool setup() override;
  void dump_config() override;
  bool wake() override;

 protected:
  bool send(const uint8_t *bytes, size_t size) override;

  InternalGPIOPin *const wake_pin_;
};
#endif

#ifdef USE_MICROCHIP_ATSHA204A_SWI_RMT
class RMTTransport : public SWITransport {
 public:
  explicit RMTTransport(uint8_t pin) : pin_(pin) {}
  bool setup() override;
  void dump_config() override;
  bool wake() override;

 protected:
  bool send(const uint8_t *bytes, size_t size) override;
  bool transmit_(const uint8_t *bytes, size_t size);

  const uint8_t pin_;
  rmt_channel_handle_t tx_{};
  rmt_encoder_handle_t encoder_{};
  std::array<uint8_t, 1 + MAX_COMMAND> frame_{};
};
#endif

}  // namespace esphome::microchip_atsha204a
