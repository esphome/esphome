#pragma once
#include <cstdint>
#include <cstddef>
#include <array>

namespace esphome::bthome {

static constexpr uint8_t BTHOME_VERSION_2 = 0x02;

struct BTHomeHeader {
  static constexpr uint8_t ENCRYPTED_FLAG = 0x01;
  // Reserved in the current BTHome v2 spec. Older firmware sets it to put the sender's MAC address,
  // least significant byte first, at the start of the (decrypted) payload.
  static constexpr uint8_t MAC_INCLUDED_FLAG = 0x02;
  // Bit 2: trigger-based device, bits 3-4: reserved, bits 5-7: BTHome version.
  static constexpr uint8_t VERSION_SHIFT = 5;

  uint8_t data;

  bool encrypted() const { return this->data & ENCRYPTED_FLAG; }
  bool mac_included() const { return this->data & MAC_INCLUDED_FLAG; }
  uint8_t version() const { return this->data >> VERSION_SHIFT; }
};

static_assert(sizeof(BTHomeHeader) == 1, "BTHomeHeader must be 1 byte");
static constexpr uint8_t BTHOME_SVC_UUID_LOW = 0xD2;   // BTHome service UUID low byte  (0xFCD2)
static constexpr uint8_t BTHOME_SVC_UUID_HIGH = 0xFC;  // BTHome service UUID high byte
// A COMMAND object is a length byte (low 5 bits: argument length), an opcode, then the arguments.
static constexpr uint8_t BTHOME_COMMAND_ARG_LENGTH_MASK = 0x1F;
static constexpr size_t BTHOME_COMMAND_HEADER_SIZE = 2;

enum class BTHomeObjectType : uint16_t {
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
  LIGHT_LEVEL = 0x64,
  MASS_KG_E2 = 0x06,
  MASS_LB_E2 = 0x07,
  MOISTURE_PCT_E2 = 0x14,
  MOISTURE_PCT_U8 = 0x2F,
  PACKET_ID = 0x00,
  PM10_UGM3 = 0x0E,
  PM2_5_UGM3 = 0x0D,
  POWER_W_E2 = 0x0B,
  POWER_W_I32_E2 = 0x5C,
  PRECIPITATION_MM_E1 = 0x5F,
  PRESSURE_HPA_E2 = 0x04,
  RAW = 0x54,
  ROTATION_DEG_E1 = 0x3F,
  ROTATIONAL_SPEED_RPM = 0x61,
  SETTINGS_REVISION = 0x65,
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

  // Events:
  BUTTON_EVENT = 0x3A,
  COMMAND = 0x3B,
  DIMMER_EVENT = 0x3C,

  // Device information:
  DEVICE_TYPE_ID = 0xF0,
  FIRMWARE_VERSION_U32 = 0xF1,
  FIRMWARE_VERSION_U24 = 0xF2,

  // Values outside the wire-format range are derived from advertisement metadata.
  SIGNAL_STRENGTH = 0x0100,

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

// Scale factor applied to the raw integer value of an object; indexes BTHOME_SCALES.
enum class BTHomeScale : uint8_t {
  BTHOME_SCALE_1,
  BTHOME_SCALE_E1,
  BTHOME_SCALE_E2,
  BTHOME_SCALE_E3,
  BTHOME_SCALE_E6,
  BTHOME_SCALE_0_35,
};
inline constexpr float BTHOME_SCALES[] = {1.0f, 0.1f, 0.01f, 0.001f, 0.000001f, 0.35f};

// Wire format of one object type. A length of 0 means variable length (TEXT, RAW, COMMAND) or unknown type.
struct BTHomeObjectInfo {
  uint8_t length : 3;
  uint8_t is_signed : 1;
  uint8_t scale : 4;
};

namespace detail {

struct BTHomeObjectSpec {
  BTHomeObjectType type;
  uint8_t length;
  BTHomeScale scale;
  bool is_signed;
};

// Every object type a BTHome v2 packet can carry. Binary sensor types must be listed too:
// the decoder stops at the first object whose length it does not know.
inline constexpr BTHomeObjectSpec BTHOME_OBJECT_SPECS[] = {
    {BTHomeObjectType::PACKET_ID, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::BATTERY_PCT, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::TEMPERATURE_C_E2, 2, BTHomeScale::BTHOME_SCALE_E2, true},
    {BTHomeObjectType::HUMIDITY_PCT_E2, 2, BTHomeScale::BTHOME_SCALE_E2, false},
    {BTHomeObjectType::PRESSURE_HPA_E2, 3, BTHomeScale::BTHOME_SCALE_E2, false},
    {BTHomeObjectType::ILLUMINANCE_LX_E2, 3, BTHomeScale::BTHOME_SCALE_E2, false},
    {BTHomeObjectType::MASS_KG_E2, 2, BTHomeScale::BTHOME_SCALE_E2, false},
    {BTHomeObjectType::MASS_LB_E2, 2, BTHomeScale::BTHOME_SCALE_E2, false},
    {BTHomeObjectType::DEWPOINT_C_E2, 2, BTHomeScale::BTHOME_SCALE_E2, true},
    {BTHomeObjectType::COUNT_U8, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::ENERGY_KWH_E3, 3, BTHomeScale::BTHOME_SCALE_E3, false},
    {BTHomeObjectType::POWER_W_E2, 3, BTHomeScale::BTHOME_SCALE_E2, false},
    {BTHomeObjectType::VOLTAGE_V_E3, 2, BTHomeScale::BTHOME_SCALE_E3, false},
    {BTHomeObjectType::PM2_5_UGM3, 2, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::PM10_UGM3, 2, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::GENERIC_BOOLEAN, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::POWER_ON, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::OPENING_OPEN, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::CO2_PPM, 2, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::TVOC_UGM3, 2, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::MOISTURE_PCT_E2, 2, BTHomeScale::BTHOME_SCALE_E2, false},
    {BTHomeObjectType::BATTERY_LOW, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::BATTERY_CHARGING, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::CO_DETECTED, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::COLD_DETECTED, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::CONNECTIVITY_CONNECTED, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::DOOR_OPEN, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::GARAGE_DOOR_OPEN, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::GAS_DETECTED, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::HEAT_DETECTED, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::LIGHT_DETECTED, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::LOCK_UNLOCKED, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::MOISTURE_WET, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::MOTION_DETECTED, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::MOVING_ACTIVE, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::OCCUPANCY_DETECTED, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::PLUG_PLUGGED_IN, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::PRESENCE_HOME, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::PROBLEM_DETECTED, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::RUNNING_ACTIVE, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::SAFETY_SAFE, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::SMOKE_DETECTED, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::SOUND_DETECTED, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::TAMPER_ACTIVE, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::VIBRATION_DETECTED, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::WINDOW_OPEN, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::HUMIDITY_PCT_U8, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::MOISTURE_PCT_U8, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::BUTTON_EVENT, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::COMMAND, 0, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::DIMMER_EVENT, 2, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::COUNT_U16, 2, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::COUNT_U32, 4, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::ROTATION_DEG_E1, 2, BTHomeScale::BTHOME_SCALE_E1, true},
    {BTHomeObjectType::DISTANCE_MM, 2, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::DISTANCE_M_E1, 2, BTHomeScale::BTHOME_SCALE_E1, false},
    {BTHomeObjectType::DURATION_S_E3, 3, BTHomeScale::BTHOME_SCALE_E3, false},
    {BTHomeObjectType::CURRENT_A_E3, 2, BTHomeScale::BTHOME_SCALE_E3, false},
    {BTHomeObjectType::SPEED_MS_E2, 2, BTHomeScale::BTHOME_SCALE_E2, false},
    {BTHomeObjectType::TEMPERATURE_C_E1, 2, BTHomeScale::BTHOME_SCALE_E1, true},
    {BTHomeObjectType::UV_INDEX_E1, 1, BTHomeScale::BTHOME_SCALE_E1, false},
    {BTHomeObjectType::VOLUME_L_E1, 2, BTHomeScale::BTHOME_SCALE_E1, false},
    {BTHomeObjectType::VOLUME_ML, 2, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::VOLUME_FLOW_M3HR_E3, 2, BTHomeScale::BTHOME_SCALE_E3, false},
    {BTHomeObjectType::VOLTAGE_V_E1, 2, BTHomeScale::BTHOME_SCALE_E1, false},
    {BTHomeObjectType::GAS_M3_U24_E3, 3, BTHomeScale::BTHOME_SCALE_E3, false},
    {BTHomeObjectType::GAS_M3_U32_E3, 4, BTHomeScale::BTHOME_SCALE_E3, false},
    {BTHomeObjectType::ENERGY_KWH_U32_E3, 4, BTHomeScale::BTHOME_SCALE_E3, false},
    {BTHomeObjectType::VOLUME_L_U32_E3, 4, BTHomeScale::BTHOME_SCALE_E3, false},
    {BTHomeObjectType::WATER_L_E3, 4, BTHomeScale::BTHOME_SCALE_E3, false},
    {BTHomeObjectType::TIMESTAMP, 4, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::ACCELERATION_MSS_E3, 2, BTHomeScale::BTHOME_SCALE_E3, false},
    {BTHomeObjectType::GYROSCOPE_DEGS_E3, 2, BTHomeScale::BTHOME_SCALE_E3, false},
    {BTHomeObjectType::TEXT, 0, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::RAW, 0, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::VOLUME_STORAGE_L_E3, 4, BTHomeScale::BTHOME_SCALE_E3, false},
    {BTHomeObjectType::CONDUCTIVITY_USCM, 2, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::TEMPERATURE_C_I8, 1, BTHomeScale::BTHOME_SCALE_1, true},
    {BTHomeObjectType::TEMPERATURE_C_I8_0_35, 1, BTHomeScale::BTHOME_SCALE_0_35, true},
    {BTHomeObjectType::COUNT_I8, 1, BTHomeScale::BTHOME_SCALE_1, true},
    {BTHomeObjectType::COUNT_I16, 2, BTHomeScale::BTHOME_SCALE_1, true},
    {BTHomeObjectType::COUNT_I32, 4, BTHomeScale::BTHOME_SCALE_1, true},
    {BTHomeObjectType::POWER_W_I32_E2, 4, BTHomeScale::BTHOME_SCALE_E2, true},
    {BTHomeObjectType::CURRENT_A_I16_E3, 2, BTHomeScale::BTHOME_SCALE_E3, true},
    {BTHomeObjectType::DIRECTION_DEG_E2, 2, BTHomeScale::BTHOME_SCALE_E2, false},
    {BTHomeObjectType::PRECIPITATION_MM_E1, 2, BTHomeScale::BTHOME_SCALE_E1, false},
    {BTHomeObjectType::CHANNEL, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::ROTATIONAL_SPEED_RPM, 2, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::SPEED_MS_I32_E6, 4, BTHomeScale::BTHOME_SCALE_E6, true},
    {BTHomeObjectType::ACCELERATION_MSS_I32_E6, 4, BTHomeScale::BTHOME_SCALE_E6, true},
    {BTHomeObjectType::LIGHT_LEVEL, 1, BTHomeScale::BTHOME_SCALE_1, false},
    {BTHomeObjectType::SETTINGS_REVISION, 1, BTHomeScale::BTHOME_SCALE_1, false},
};

inline constexpr size_t BTHOME_OBJECT_ID_COUNT = static_cast<size_t>(BTHomeObjectType::SETTINGS_REVISION) + 1;

constexpr std::array<BTHomeObjectInfo, BTHOME_OBJECT_ID_COUNT> make_object_info_table() {
  std::array<BTHomeObjectInfo, BTHOME_OBJECT_ID_COUNT> table{};
  for (const auto &spec : BTHOME_OBJECT_SPECS) {
    table[static_cast<size_t>(spec.type)] = {spec.length, spec.is_signed, static_cast<uint8_t>(spec.scale)};
  }
  return table;
}

}  // namespace detail

inline constexpr std::array<BTHomeObjectInfo, detail::BTHOME_OBJECT_ID_COUNT> BTHOME_OBJECT_INFO =
    detail::make_object_info_table();

namespace detail {
inline constexpr uint8_t UNSCALED = static_cast<uint8_t>(BTHomeScale::BTHOME_SCALE_1);
inline constexpr uint8_t UINT24_SIZE = 3;
}  // namespace detail

constexpr BTHomeObjectInfo get_bthome_object_info(BTHomeObjectType type) {
  const auto id = static_cast<size_t>(type);
  if (id < BTHOME_OBJECT_INFO.size())
    return BTHOME_OBJECT_INFO[id];
  // Types far above the dense range are handled here so the table stays small.
  switch (type) {
    case BTHomeObjectType::DEVICE_TYPE_ID:
      return {sizeof(uint16_t), false, detail::UNSCALED};
    case BTHomeObjectType::FIRMWARE_VERSION_U32:
      return {sizeof(uint32_t), false, detail::UNSCALED};
    case BTHomeObjectType::FIRMWARE_VERSION_U24:
      return {detail::UINT24_SIZE, false, detail::UNSCALED};
    case BTHomeObjectType::SIGNAL_STRENGTH:
      return {sizeof(int8_t), true, detail::UNSCALED};
    default:
      return {};
  }
}

// Returns the value byte count for a fixed-width BTHome type, or 0 for variable-length and unknown types.
constexpr size_t get_bthome_value_length(BTHomeObjectType type) { return get_bthome_object_info(type).length; }
constexpr float bthome_scaling_factor(BTHomeObjectType type) {
  return BTHOME_SCALES[get_bthome_object_info(type).scale];
}
constexpr bool bthome_is_signed(BTHomeObjectType type) { return get_bthome_object_info(type).is_signed; }

static_assert(get_bthome_value_length(BTHomeObjectType::PRESSURE_HPA_E2) == 3);
static_assert(get_bthome_value_length(BTHomeObjectType::TEXT) == 0);
static_assert(get_bthome_value_length(BTHomeObjectType::BUTTON_EVENT) == 1);
static_assert(get_bthome_value_length(BTHomeObjectType::FIRMWARE_VERSION_U24) == 3);
static_assert(bthome_is_signed(BTHomeObjectType::TEMPERATURE_C_E2));
static_assert(bthome_scaling_factor(BTHomeObjectType::SPEED_MS_I32_E6) == 0.000001f);

}  // namespace esphome::bthome
