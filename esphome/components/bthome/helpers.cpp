#include "helpers.h"

#include <cstring>

namespace esphome {
namespace bthome {

size_t get_bthome_value_length(BTHomeObjectType obj_type) {
  switch (obj_type) {
    // 1 Byte (uint8 / sint8)
    case BTHomeObjectType::PACKET_ID:
    case BTHomeObjectType::BATTERY_PCT:
    case BTHomeObjectType::COUNT_U8:
    case BTHomeObjectType::HUMIDITY_PCT_U8:
    case BTHomeObjectType::MOISTURE_PCT_U8:
    case BTHomeObjectType::UV_INDEX_E1:
    case BTHomeObjectType::TEMPERATURE_C_I8:
    case BTHomeObjectType::TEMPERATURE_C_I8_0_35:
    case BTHomeObjectType::COUNT_I8:
    case BTHomeObjectType::CHANNEL:

    // Binary sensors:
    case BTHomeObjectType::GENERIC_BOOLEAN:
    case BTHomeObjectType::POWER_ON:
    case BTHomeObjectType::OPENING_OPEN:
    case BTHomeObjectType::BATTERY_LOW:
    case BTHomeObjectType::BATTERY_CHARGING:
    case BTHomeObjectType::CO_DETECTED:
    case BTHomeObjectType::COLD_DETECTED:
    case BTHomeObjectType::CONNECTIVITY_CONNECTED:
    case BTHomeObjectType::DOOR_OPEN:
    case BTHomeObjectType::GARAGE_DOOR_OPEN:
    case BTHomeObjectType::GAS_DETECTED:
    case BTHomeObjectType::HEAT_DETECTED:
    case BTHomeObjectType::LIGHT_DETECTED:
    case BTHomeObjectType::LOCK_UNLOCKED:
    case BTHomeObjectType::MOISTURE_WET:
    case BTHomeObjectType::MOTION_DETECTED:
    case BTHomeObjectType::MOVING_ACTIVE:
    case BTHomeObjectType::OCCUPANCY_DETECTED:
    case BTHomeObjectType::PLUG_PLUGGED_IN:
    case BTHomeObjectType::PRESENCE_HOME:
    case BTHomeObjectType::PROBLEM_DETECTED:
    case BTHomeObjectType::RUNNING_ACTIVE:
    case BTHomeObjectType::SAFETY_SAFE:
    case BTHomeObjectType::SMOKE_DETECTED:
    case BTHomeObjectType::SOUND_DETECTED:
    case BTHomeObjectType::TAMPER_ACTIVE:
    case BTHomeObjectType::VIBRATION_DETECTED:
    case BTHomeObjectType::WINDOW_OPEN:
      return 1;

    // 2 Bytes (uint16 / sint16)
    case BTHomeObjectType::TEMPERATURE_C_E2:
    case BTHomeObjectType::HUMIDITY_PCT_E2:
    case BTHomeObjectType::MASS_KG_E2:
    case BTHomeObjectType::MASS_LB_E2:
    case BTHomeObjectType::DEWPOINT_C_E2:
    case BTHomeObjectType::VOLTAGE_V_E3:
    case BTHomeObjectType::PM25_UGM3:
    case BTHomeObjectType::PM10_UGM3:
    case BTHomeObjectType::CO2_PPM:
    case BTHomeObjectType::TVOC_UGM3:
    case BTHomeObjectType::MOISTURE_PCT_E2:
    case BTHomeObjectType::COUNT_U16:
    case BTHomeObjectType::ROTATION_DEG_E1:
    case BTHomeObjectType::DISTANCE_MM:
    case BTHomeObjectType::DISTANCE_M_E1:
    case BTHomeObjectType::CURRENT_A_E3:
    case BTHomeObjectType::SPEED_MS_E2:
    case BTHomeObjectType::TEMPERATURE_C_E1:
    case BTHomeObjectType::VOLUME_L_E1:
    case BTHomeObjectType::VOLUME_ML:
    case BTHomeObjectType::VOLUME_FLOW_M3HR_E3:
    case BTHomeObjectType::VOLTAGE_V_E1:
    case BTHomeObjectType::ACCELERATION_MSS_E3:
    case BTHomeObjectType::GYROSCOPE_DEGS_E3:
    case BTHomeObjectType::CONDUCTIVITY_USCM:
    case BTHomeObjectType::COUNT_I16:
    case BTHomeObjectType::CURRENT_A_I16_E3:
    case BTHomeObjectType::DIRECTION_DEG_E2:
    case BTHomeObjectType::PRECIPITATION_MM_E1:
    case BTHomeObjectType::ROTATIONAL_SPEED_RPM:
      return 2;

    // 3 Bytes (uint24)
    case BTHomeObjectType::PRESSURE_HPA_E2:
    case BTHomeObjectType::ILLUMINANCE_LX_E2:
    case BTHomeObjectType::ENERGY_KWH_E3:
    case BTHomeObjectType::POWER_W_E2:
    case BTHomeObjectType::DURATION_S_E3:
    case BTHomeObjectType::GAS_M3_U24_E3:
      return 3;

    // 4 Bytes (uint32 / sint32)
    case BTHomeObjectType::COUNT_U32:
    case BTHomeObjectType::GAS_M3_U32_E3:
    case BTHomeObjectType::ENERGY_KWH_U32_E3:
    case BTHomeObjectType::VOLUME_L_U32_E3:
    case BTHomeObjectType::WATER_L_E3:
    case BTHomeObjectType::TIMESTAMP:
    case BTHomeObjectType::VOLUME_STORAGE_L_E3:
    case BTHomeObjectType::COUNT_I32:
    case BTHomeObjectType::POWER_W_I32_E2:
    case BTHomeObjectType::SPEED_MS_I32_E6:
    case BTHomeObjectType::ACCELERATION_MSS_I32_E6:
      return 4;

    // Variable length or Unknown
    case BTHomeObjectType::TEXT:
    case BTHomeObjectType::RAW:
    default:
      return 0;
  }
}

float bthome_scaling_factor(BTHomeObjectType type) {
  switch (type) {
    // 0.000001f scaling
    case BTHomeObjectType::SPEED_MS_I32_E6:
    case BTHomeObjectType::ACCELERATION_MSS_I32_E6:
      return 0.000001f;

    // 0.001f scaling
    case BTHomeObjectType::ENERGY_KWH_E3:
    case BTHomeObjectType::VOLTAGE_V_E3:
    case BTHomeObjectType::DURATION_S_E3:
    case BTHomeObjectType::CURRENT_A_E3:
    case BTHomeObjectType::VOLUME_FLOW_M3HR_E3:
    case BTHomeObjectType::GAS_M3_U24_E3:
    case BTHomeObjectType::GAS_M3_U32_E3:
    case BTHomeObjectType::ENERGY_KWH_U32_E3:
    case BTHomeObjectType::VOLUME_L_U32_E3:
    case BTHomeObjectType::WATER_L_E3:
    case BTHomeObjectType::ACCELERATION_MSS_E3:
    case BTHomeObjectType::GYROSCOPE_DEGS_E3:
    case BTHomeObjectType::VOLUME_STORAGE_L_E3:
    case BTHomeObjectType::CURRENT_A_I16_E3:
      return 0.001f;

    // 0.01f scaling
    case BTHomeObjectType::TEMPERATURE_C_E2:
    case BTHomeObjectType::HUMIDITY_PCT_E2:
    case BTHomeObjectType::PRESSURE_HPA_E2:
    case BTHomeObjectType::ILLUMINANCE_LX_E2:
    case BTHomeObjectType::MASS_KG_E2:
    case BTHomeObjectType::MASS_LB_E2:
    case BTHomeObjectType::DEWPOINT_C_E2:
    case BTHomeObjectType::POWER_W_E2:
    case BTHomeObjectType::MOISTURE_PCT_E2:
    case BTHomeObjectType::SPEED_MS_E2:
    case BTHomeObjectType::POWER_W_I32_E2:
    case BTHomeObjectType::DIRECTION_DEG_E2:
      return 0.01f;

    // 0.1f scaling
    case BTHomeObjectType::ROTATION_DEG_E1:
    case BTHomeObjectType::TEMPERATURE_C_E1:
    case BTHomeObjectType::UV_INDEX_E1:
    case BTHomeObjectType::VOLUME_L_E1:
    case BTHomeObjectType::VOLTAGE_V_E1:
    case BTHomeObjectType::PRECIPITATION_MM_E1:
    case BTHomeObjectType::DISTANCE_M_E1:
      return 0.1f;

    // Unique scaling
    case BTHomeObjectType::TEMPERATURE_C_I8_0_35:
      return 0.35f;

    default:
      return 1.0f;
  }
}

bool bthome_is_signed(BTHomeObjectType type) {
  switch (type) {
    case BTHomeObjectType::ACCELERATION_MSS_I32_E6:
    case BTHomeObjectType::SPEED_MS_I32_E6:
    case BTHomeObjectType::TEMPERATURE_C_E2:
    case BTHomeObjectType::DEWPOINT_C_E2:
    case BTHomeObjectType::ROTATION_DEG_E1:
    case BTHomeObjectType::TEMPERATURE_C_E1:
    case BTHomeObjectType::TEMPERATURE_C_I8:
    case BTHomeObjectType::TEMPERATURE_C_I8_0_35:
    case BTHomeObjectType::COUNT_I8:
    case BTHomeObjectType::COUNT_I16:
    case BTHomeObjectType::COUNT_I32:
    case BTHomeObjectType::POWER_W_I32_E2:
    case BTHomeObjectType::CURRENT_A_I16_E3:
      return true;
    default:
      return false;
  }
}

MacAddress::MacAddress(const uint8_t *addr) { *this = addr; }

MacAddress::MacAddress(uint64_t addr) {
  for (int i = sizeof(this->addr_) - 1; i >= 0; i--) {
    this->addr_[i] = addr & 0xFF;
    addr >>= 8;
  }
}

MacAddress::MacAddress(const MacAddressPtr &addr) : MacAddress((const uint8_t *) addr) {}

MacAddress &MacAddress::operator=(const uint8_t *addr) {
  std::memcpy(this->addr_, addr, sizeof(this->addr_));
  return *this;
}

MacAddress &MacAddress::operator=(MacAddressPtr addr) {
  this->operator=((const uint8_t *) addr);
  return *this;
}

MacAddress::operator const uint8_t *() const { return this->addr_; }

bool MacAddress::operator==(const MacAddress &other) const { return MacAddressPtr(*this) == MacAddressPtr(other); }

bool MacAddress::operator==(const MacAddressPtr &other) const { return MacAddressPtr(*this) == other; }

bool MacAddressPtr::operator==(const MacAddressPtr &other) const {
  return std::memcmp(this->addr_, other.addr_, MAC_ADDRESS_SIZE) == 0;
}

bool MacAddressPtr::operator==(const MacAddress &other) const { return *this == MacAddressPtr(other); }
bool MacAddressPtr::operator==(std::nullptr_t) const { return this->addr_ == nullptr; }

const char *MacAddressPtr::c_str() const {
  static char buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  format_mac_addr_upper(this->addr_, buf);
  return buf;
}

const char *MacAddress::c_str() const { return MacAddressPtr(*this).c_str(); }

}  // namespace bthome
}  // namespace esphome
