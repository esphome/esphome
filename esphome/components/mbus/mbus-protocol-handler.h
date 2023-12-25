#pragma once
#include <vector>
#include "stdint.h"
#include "esphome/core/hal.h"
#include "mbus-frame.h"

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

class MBusProtocolHandler {
 public:
  static const uint32_t rx_timeout{1000};

  int8_t receive();
  int8_t send(MBusFrame &frame);

  bool is_ack_resonse();
  MBusProtocolHandler(INetworkAdapter *networkAdapter) : _networkAdapter(networkAdapter) {}

 protected:
  INetworkAdapter *_networkAdapter;
  std::vector<uint8_t> _rx_buffer;
  uint32_t _timestamp{0};
};

}  // namespace mbus
}  // namespace esphome
