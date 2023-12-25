#pragma once
#include "mbus-frame-meta.h"

namespace esphome {
namespace mbus {

const MBusFrameMeta MBusFrameDefinition::ACK_FRAME = MBusFrameMeta(0xE5, 0x00, 0x00, 0x01);
const MBusFrameMeta MBusFrameDefinition::SHORT_FRAME = MBusFrameMeta(0x10, 0x68, 0x00, 0x05);
const MBusFrameMeta MBusFrameDefinition::CONTROL_FRAME = MBusFrameMeta(0x68, 0x68, 0x03, 0x09);
const MBusFrameMeta MBusFrameDefinition::LONG_FRAME = MBusFrameMeta(0x68, 0x68, 0x03, 0x09);

}  // namespace mbus
}  // namespace esphome
