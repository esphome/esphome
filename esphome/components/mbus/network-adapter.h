#pragma once

#include "stdint.h"
#include <vector>
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace mbus {

class INetworkAdapter {
 public:
  virtual int8_t send(std::vector<uint8_t> &payload) = 0;
  virtual int8_t receive(std::vector<uint8_t> &payload) = 0;
};

class SerialAdapter : public INetworkAdapter {
 public:
  int8_t send(std::vector<uint8_t> &payload) override;
  int8_t receive(std::vector<uint8_t> &payload) override;

  SerialAdapter(uart::UARTDevice *uart) : _uart(uart) {}

 protected:
  uart::UARTDevice *_uart;
};

}  // namespace mbus
}  // namespace esphome
