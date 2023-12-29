#pragma once
#include <memory>

#include "mbus-frame.h"

namespace esphome {
namespace mbus {

class MBusFrameFactory {
 public:
  static std::unique_ptr<MBusFrame> create_empty_frame();
  static std::unique_ptr<MBusFrame> create_ack_frame();
  static std::unique_ptr<MBusFrame> create_nke_frame(uint8_t primary_address);
  static std::unique_ptr<MBusFrame> create_slave_select(std::vector<uint8_t> mask);
};

}  // namespace mbus
}  // namespace esphome
