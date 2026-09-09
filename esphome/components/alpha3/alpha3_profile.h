#pragma once

#include <array>
#include <cstdint>

#include "esphome/components/alpha3/alpha3_protocol.h"

namespace esphome::alpha3 {

enum class ObjectKind : uint8_t {
  OBJECT_KIND_HYDRAULIC_MODEL_B,
  OBJECT_KIND_HYDRAULIC_EXTENDED,
  OBJECT_KIND_ELECTRICAL,
  OBJECT_KIND_HISTORY,
  OBJECT_KIND_ENERGY,
  OBJECT_KIND_LOCAL_OPERATION,
  OBJECT_KIND_PRIORITIZED_OPERATION,
  OBJECT_KIND_REALIZED_OPERATION,
  OBJECT_KIND_LOCAL_CONTROL,
  OBJECT_KIND_CS_FACTORY_LIMITS,
  OBJECT_KIND_CS_USER_CONFIG,
  OBJECT_KIND_CP_FACTORY_LIMITS,
  OBJECT_KIND_CP_USER_CONFIG,
  OBJECT_KIND_PP_FACTORY_LIMITS,
  OBJECT_KIND_PP_USER_CONFIG,
  OBJECT_KIND_OPERATION_CONFIG,
};

enum class Capability : uint32_t {
  CAPABILITY_NONE = 0,
  CAPABILITY_HYDRAULIC = 1U << 0,
  CAPABILITY_ELECTRICAL = 1U << 1,
  CAPABILITY_HISTORY = 1U << 2,
  CAPABILITY_ENERGY = 1U << 3,
  CAPABILITY_STATUS = 1U << 4,
  CAPABILITY_SETPOINT_LIMITS = 1U << 5,
  CAPABILITY_WRITE_OPERATION = 1U << 16,
  CAPABILITY_WRITE_CONTROL = 1U << 17,
  CAPABILITY_WRITE_SETPOINT = 1U << 18,
};

struct ObjectSchema {
  ObjectKind kind;
  ObjectAddress address;
  uint16_t object_type;
  std::array<ObjectVersionSpec, 2> versions;
  uint8_t version_count;
  bool readable;
  bool writable;
};

struct DeviceIdentity {
  uint8_t family{0};
  uint8_t type{0};
  uint8_t version{0};
  uint8_t valid_mask{0};
  bool complete() const { return this->valid_mask == 0x07; }
};

struct DeviceProfile {
  const char *name;
  uint8_t family;
  uint8_t type;
  uint8_t version;
  const ObjectSchema *schemas;
  uint8_t schema_count;
  uint32_t read_capabilities;
  uint32_t write_capabilities;
  std::array<uint8_t, 6> control_modes;
  uint8_t control_mode_count;
  float pascals_per_meter;
};

struct ProfileMatch {
  const DeviceProfile *profile{nullptr};
  bool exact{false};
  bool writable{false};
};

constexpr std::array<uint8_t, 6> CONTROL_MODE_VALUES{{0, 1, 2, 13, 14, 15}};
constexpr ObjectSchema EXTENDED_HYDRAULIC_SCHEMA{
    ObjectKind::OBJECT_KIND_HYDRAULIC_EXTENDED, {93, 290}, 565, {{{2, 36, false}, {0, 0, false}}}, 1, true, false,
};

ProfileMatch match_profile(const DeviceIdentity &identity);
const ObjectSchema *find_schema(const DeviceProfile &profile, ObjectKind kind);
bool has_read_capability(const ProfileMatch &match, Capability capability);
bool has_write_capability(const ProfileMatch &match, Capability capability);
bool can_read_object(const ProfileMatch &match, ObjectKind kind);

}  // namespace esphome::alpha3
