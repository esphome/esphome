#pragma once
#include <memory>
#include <vector>

#include "esphome/components/mbus/mbus_frame.h"

namespace esphome {
namespace mbus {

class MBusFrameFactory {
 public:
  // request frames
  static std::unique_ptr<MBusFrame> create_empty_frame();
  static std::unique_ptr<MBusFrame> create_nke_frame(uint8_t primary_address);
  static std::unique_ptr<MBusFrame> create_req_ud2_frame();
  static std::unique_ptr<MBusFrame> create_slave_select(uint64_t secondary_address);

  // response frames
  static std::unique_ptr<MBusFrame> create_ack_frame();
  static std::unique_ptr<MBusFrame> create_short_frame(uint8_t control, uint8_t address, uint8_t checksum);
  static std::unique_ptr<MBusFrame> create_control_frame(uint8_t control, uint8_t address, uint8_t control_information,
                                                         uint8_t checksum);
  static std::unique_ptr<MBusFrame> create_long_frame(uint8_t control, uint8_t address, uint8_t control_information,
                                                      std::vector<uint8_t> &data, uint8_t checksum);
};

}  // namespace mbus
}  // namespace esphome
