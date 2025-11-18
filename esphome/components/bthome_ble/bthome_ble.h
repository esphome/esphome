#pragma once

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"

#include <map>
#include <vector>

#ifdef USE_ESP32

namespace esphome {
namespace bthome_ble {

// BTHome object IDs - subset of supported sensor types
enum BTHomeObjectId : uint8_t {
  PACKET_ID = 0x00,
  BATTERY = 0x01,
  TEMPERATURE = 0x02,
  HUMIDITY = 0x03,
  PRESSURE = 0x04,
  ILLUMINANCE = 0x05,
  MASS_KG = 0x06,
  MASS_LB = 0x07,
  DEWPOINT = 0x08,
  COUNT = 0x09,
  ENERGY = 0x0A,
  POWER = 0x0B,
  VOLTAGE = 0x0C,
  PM25 = 0x0D,
  PM10 = 0x0E,
  CO2 = 0x12,
  VOC = 0x13,
  MOISTURE = 0x14,
  BATTERY_LOW = 0x15,
  BATTERY_CHARGING = 0x16,
  CARBON_MONOXIDE = 0x17,
  COLD = 0x18,
  CONNECTIVITY = 0x19,
  DOOR = 0x1A,
  GARAGE_DOOR = 0x1B,
  GAS = 0x1C,
  HEAT = 0x1D,
  LIGHT = 0x1E,
  LOCK = 0x1F,
  MOISTURE_BOOL = 0x20,
  MOTION = 0x21,
  MOVING = 0x22,
  OCCUPANCY = 0x23,
  OPENING = 0x24,
  PLUG = 0x25,
  POWER_ON = 0x26,
  PRESENCE = 0x27,
  PROBLEM = 0x28,
  RUNNING = 0x29,
  SAFETY = 0x2A,
  SMOKE = 0x2B,
  SOUND = 0x2C,
  TAMPER = 0x2D,
  VIBRATION = 0x2E,
  WINDOW = 0x2F,
  HUMIDITY_PERCENT = 0x2E,
  MOISTURE_PERCENT = 0x2F,
  BUTTON = 0x3A,
  DIMMER = 0x3C,
  COUNT_UINT16 = 0x3D,
  COUNT_UINT32 = 0x3E,
  ROTATION = 0x3F,
  DISTANCE_MM = 0x40,
  DISTANCE_M = 0x41,
  DURATION = 0x42,
  CURRENT = 0x43,
  SPEED = 0x44,
  TEMPERATURE_PRECISE = 0x45,
  UV_INDEX = 0x46,
  VOLUME_L = 0x47,
  VOLUME_ML = 0x48,
  VOLUME_FLOW = 0x49,
  VOLTAGE_PRECISE = 0x4A,
  GAS_VOLUME = 0x4B,
  GAS_VOLUME_L = 0x4C,
  ENERGY_PRECISE = 0x4D,
  VOLUME_PRECISE = 0x4E,
  WATER = 0x4F,
  TIMESTAMP = 0x50,
  ACCELERATION = 0x51,
  GYROSCOPE = 0x52,
  TEXT = 0x53,
  RAW = 0x54,
};

struct BTHomeParseResult {
  std::map<uint8_t, float> sensors;
  std::map<uint8_t, bool> binary_sensors;
  std::map<uint8_t, std::string> text_sensors;
  bool has_encryption{false};
  bool is_trigger_based{false};
  uint8_t device_info{0};
};

class BTHomeListener : public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void set_bindkey(const std::string &bindkey);
  bool decrypt_payload(std::vector<uint8_t> &payload, const uint64_t &address, const uint32_t &count);

 protected:
  optional<std::string> bindkey_;
};

bool parse_bthome_data_byte(const uint8_t *data, uint8_t data_length, BTHomeParseResult &result);

}  // namespace bthome_ble
}  // namespace esphome

#endif
