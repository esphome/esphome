#include "helpers.h"

#include <cstring>

namespace esphome {
namespace bthome {

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
