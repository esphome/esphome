#pragma once
#include <memory>

#include "mbus-frame.h"

namespace esphome {
namespace mbus {

class MBusFrameFactory {
 public:
  static std::unique_ptr<MBusFrame> CreateNKEFrame(uint8_t primary_address);
  static std::unique_ptr<MBusFrame> CreateSlaveSelect(std::vector<uint8_t> mask);
};

}  // namespace mbus
}  // namespace esphome
