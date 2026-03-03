#pragma once
#include "esphome/core/helpers.h"

#include <cstdint>
#include <cstddef>
#include <array>
#include "ble.h"

namespace esphome {
namespace bthome {

using EncryptionKey = std::array<uint8_t, 16>;

struct BTHomeHeader {
  uint8_t encrypted : 1;      // bit 0: encrypted data
  uint8_t : 1;                // bit 1: reserved
  uint8_t trigger_based : 1;  // bit 2: irregular advertisement interval
  uint8_t : 2;                // bits 3-4: reserved
  uint8_t version : 3;        // bits 5-7: BTHome version (currently 1 or 2)
};

static_assert(sizeof(BTHomeHeader) == 1, "BTHomeHeader must be 1 byte");
static constexpr uint8_t BTHOME_SVC_UUID_LOW = 0xD2;   // BTHome service UUID low byte  (0xFCD2)
static constexpr uint8_t BTHOME_SVC_UUID_HIGH = 0xFC;  // BTHome service UUID high byte
static constexpr uint16_t BTHOME_UUID16 = (BTHOME_SVC_UUID_HIGH << 8) | BTHOME_SVC_UUID_LOW;
static constexpr uint8_t BTHOME_VERSION_2 = 0x02;
static constexpr size_t BTHOME_MIC_SIZE = 4;
static constexpr size_t BTHOME_COUNTER_SIZE = 4;
static constexpr size_t BTHOME_MAX_PAYLOAD =
    BLE_ADV_MAX_SIZE - BLE_FLAGS_SIZE - BLE_ADV_HEADER_SIZE - sizeof(esphome::bthome::BTHomeHeader);
static constexpr size_t BTHOME_MAX_ENCRYPTED_PAYLOAD = BTHOME_MAX_PAYLOAD - BTHOME_COUNTER_SIZE - BTHOME_MIC_SIZE;

enum class BTHomeObjectType : uint8_t {
  ACCELERATION_MSS_E3 = 0x51,
  ACCELERATION_MSS_I32_E6 = 0x63,
  BATTERY_PCT = 0x01,
  CHANNEL = 0x60,
  CO2_PPM = 0x12,
  CONDUCTIVITY_USCM = 0x56,
  COUNT_U8 = 0x09,
  COUNT_U16 = 0x3D,
  COUNT_U32 = 0x3E,
  COUNT_I8 = 0x59,
  COUNT_I16 = 0x5A,
  COUNT_I32 = 0x5B,
  CURRENT_A_E3 = 0x43,
  CURRENT_A_I16_E3 = 0x5D,
  DEWPOINT_C_E2 = 0x08,
  DIRECTION_DEG_E2 = 0x5E,
  DISTANCE_MM = 0x40,
  DISTANCE_M_E1 = 0x41,
  DURATION_S_E3 = 0x42,
  ENERGY_KWH_E3 = 0x0A,
  ENERGY_KWH_U32_E3 = 0x4D,
  GAS_M3_U24_E3 = 0x4B,
  GAS_M3_U32_E3 = 0x4C,
  GYROSCOPE_DEGS_E3 = 0x52,
  HUMIDITY_PCT_E2 = 0x03,
  HUMIDITY_PCT_U8 = 0x2E,
  ILLUMINANCE_LX_E2 = 0x05,
  MASS_KG_E2 = 0x06,
  MASS_LB_E2 = 0x07,
  MOISTURE_PCT_E2 = 0x14,
  MOISTURE_PCT_U8 = 0x2F,
  PACKET_ID = 0x00,
  PM10_UGM3 = 0x0E,
  PM25_UGM3 = 0x0D,
  POWER_W_E2 = 0x0B,
  POWER_W_I32_E2 = 0x5C,
  PRECIPITATION_MM_E1 = 0x5F,
  PRESSURE_HPA_E2 = 0x04,
  RAW = 0x54,
  ROTATION_DEG_E1 = 0x3F,
  ROTATIONAL_SPEED_RPM = 0x61,
  SPEED_MS_E2 = 0x44,
  SPEED_MS_I32_E6 = 0x62,
  TEMPERATURE_C_E2 = 0x02,
  TEMPERATURE_C_E1 = 0x45,
  TEMPERATURE_C_I8 = 0x57,
  TEMPERATURE_C_I8_0_35 = 0x58,
  TEXT = 0x53,
  TIMESTAMP = 0x50,
  TVOC_UGM3 = 0x13,
  UV_INDEX_E1 = 0x46,
  VOLTAGE_V_E3 = 0x0C,
  VOLTAGE_V_E1 = 0x4A,
  VOLUME_FLOW_M3HR_E3 = 0x49,
  VOLUME_L_E1 = 0x47,
  VOLUME_ML = 0x48,
  VOLUME_L_U32_E3 = 0x4E,
  VOLUME_STORAGE_L_E3 = 0x55,
  WATER_L_E3 = 0x4F,

  // Binary sensors:

  BATTERY_CHARGING = 0x16,
  BATTERY_LOW = 0x15,
  CO_DETECTED = 0x17,
  COLD_DETECTED = 0x18,
  CONNECTIVITY_CONNECTED = 0x19,
  DOOR_OPEN = 0x1A,
  GARAGE_DOOR_OPEN = 0x1B,
  GAS_DETECTED = 0x1C,
  GENERIC_BOOLEAN = 0x0F,
  HEAT_DETECTED = 0x1D,
  LIGHT_DETECTED = 0x1E,
  LOCK_UNLOCKED = 0x1F,
  MOISTURE_WET = 0x20,
  MOTION_DETECTED = 0x21,
  MOVING_ACTIVE = 0x22,
  OCCUPANCY_DETECTED = 0x23,
  OPENING_OPEN = 0x11,
  PLUG_PLUGGED_IN = 0x24,
  POWER_ON = 0x10,
  PRESENCE_HOME = 0x25,
  PROBLEM_DETECTED = 0x26,
  RUNNING_ACTIVE = 0x27,
  SAFETY_SAFE = 0x28,
  SMOKE_DETECTED = 0x29,
  SOUND_DETECTED = 0x2A,
  TAMPER_ACTIVE = 0x2B,
  VIBRATION_DETECTED = 0x2C,
  WINDOW_OPEN = 0x2D,
};

// Free functions for object type metadata (used by both decoder and encoder)
size_t get_bthome_value_length(BTHomeObjectType obj_type);
float bthome_scaling_factor(BTHomeObjectType type);
bool bthome_is_signed(BTHomeObjectType type);

class MacAddressPtr;
class __attribute__((packed)) MacAddress {
 public:
  MacAddress() = default;
  MacAddress(const uint8_t *addr);
  MacAddress(uint64_t addr);
  MacAddress(const MacAddressPtr &addr);

  MacAddress &operator=(const uint8_t *addr);
  MacAddress &operator=(MacAddressPtr addr);

  operator const uint8_t *() const;

  bool operator==(const MacAddress &other) const;
  bool operator==(const MacAddressPtr &other) const;

  const char *c_str() const;

 protected:
  uint8_t addr_[MAC_ADDRESS_SIZE]{};
};

class MacAddressPtr {
 public:
  MacAddressPtr() = default;
  MacAddressPtr(const uint8_t *addr) : addr_(addr) {}
  MacAddressPtr(const MacAddress &addr) : MacAddressPtr((const uint8_t *) (addr)) {}

  operator const uint8_t *() const { return this->addr_; }

  bool operator==(const MacAddressPtr &other) const;
  bool operator==(const MacAddress &other) const;
  bool operator==(std::nullptr_t) const;

  const char *c_str() const;

 protected:
  const uint8_t *addr_{nullptr};
};

}  // namespace bthome
}  // namespace esphome
