#pragma once

namespace esphome {
namespace haier {
namespace smartair2_protocol {

enum class ConditioningMode : uint8_t { AUTO = 0x00, COOL = 0x01, HEAT = 0x02, FAN = 0x03, DRY = 0x04 };

enum class FanMode : uint8_t { FAN_HIGH = 0x00, FAN_MID = 0x01, FAN_LOW = 0x02, FAN_AUTO = 0x03 };

struct HaierPacketControl {
  // Control bytes starts here
  // 10
  uint8_t : 8;  // Temperature high byte
  // 11
  uint8_t room_temperature;  // current room temperature 1°C step
  // 12
  uint8_t : 8;  // Humidity high byte
  // 13
  uint8_t room_humidity;  // Humidity 0%-100% with 1% step
  // 14
  uint8_t : 8;
  // 15
  uint8_t cntrl;  // In AC => ESP packets - 0x7F, in ESP => AC packets - 0x00
  // 16
  uint8_t : 8;
  // 17
  uint8_t : 8;
  // 18
  uint8_t : 8;
  // 19
  uint8_t : 8;
  // 20
  uint8_t : 8;
  // 21
  uint8_t ac_mode;  // See enum ConditioningMode
  // 22
  uint8_t : 8;
  // 23
  uint8_t fan_mode;  // See enum FanMode
  // 24
  uint8_t : 8;
  // 25
  uint8_t swing_mode;  // In normal mode: If 1 - swing both direction, if 0 - horizontal_swing and
                       // vertical_swing define vertical/horizontal/off
                       // In alternative mode: 0 - off, 01 - vertical,  02 - horizontal, 03 - both
  // 26
  uint8_t : 3;
  uint8_t use_fahrenheit : 1;
  uint8_t : 3;
  uint8_t lock_remote : 1;  // Disable remote
  // 27
  uint8_t ac_power : 1;  // Is ac on or off
  uint8_t : 2;
  uint8_t health_mode : 1;  // Health mode on or off
  uint8_t compressor : 1;   // Compressor on or off ???
  uint8_t half_degree : 1;  // Use half degree
  uint8_t ten_degree : 1;   // 10 degree status (only work in heat mode)
  uint8_t : 0;
  // 28
  uint8_t : 8;
  // 29
  uint8_t use_swing_bits : 1;    // Indicate if horizontal_swing and vertical_swing should be used
  uint8_t turbo_mode : 1;        // Turbo mode
  uint8_t quiet_mode : 1;        // Sleep mode
  uint8_t horizontal_swing : 1;  // Horizontal swing (if swing_both == 0)
  uint8_t vertical_swing : 1;    // Vertical swing (if swing_both == 0) if vertical_swing and horizontal_swing both 0 =>
                                 // swing off
  uint8_t display_status : 1;    // Led on or off
  uint8_t : 0;
  // 30
  uint8_t : 8;
  // 31
  uint8_t : 8;
  // 32
  uint8_t : 8;  // Target temperature high byte
  // 33
  uint8_t set_point;  // Target temperature with 16°C offset, 1°C step
};

struct HaierStatus {
  uint16_t subcommand;
  HaierPacketControl control;
};

}  // namespace smartair2_protocol
}  // namespace haier
}  // namespace esphome
