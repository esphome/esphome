#include "alpha3_profile.h"

namespace esphome::alpha3 {
namespace {

constexpr uint32_t capability_mask(Capability capability) { return static_cast<uint32_t>(capability); }

constexpr uint32_t MODEL_B_READ_CAPABILITIES =
    capability_mask(Capability::CAPABILITY_HYDRAULIC) | capability_mask(Capability::CAPABILITY_ELECTRICAL) |
    capability_mask(Capability::CAPABILITY_HISTORY) | capability_mask(Capability::CAPABILITY_ENERGY) |
    capability_mask(Capability::CAPABILITY_STATUS) | capability_mask(Capability::CAPABILITY_SETPOINT_LIMITS);
constexpr uint32_t MODEL_B_WRITE_CAPABILITIES = capability_mask(Capability::CAPABILITY_WRITE_OPERATION) |
                                                capability_mask(Capability::CAPABILITY_WRITE_CONTROL) |
                                                capability_mask(Capability::CAPABILITY_WRITE_SETPOINT);
constexpr uint32_t PREFIX_COMPATIBLE_READ_CAPABILITIES =
    capability_mask(Capability::CAPABILITY_ELECTRICAL) | capability_mask(Capability::CAPABILITY_HISTORY) |
    capability_mask(Capability::CAPABILITY_ENERGY) | capability_mask(Capability::CAPABILITY_STATUS) |
    capability_mask(Capability::CAPABILITY_SETPOINT_LIMITS);

constexpr std::array<ObjectSchema, 15> MODEL_B_SCHEMAS{{
    {ObjectKind::OBJECT_KIND_HYDRAULIC_MODEL_B, {93, 289}, 304, {{{1, 24, false}, {0, 0, false}}}, 1, true, false},
    {ObjectKind::OBJECT_KIND_ELECTRICAL, {87, 69}, 256, {{{1, 37, false}, {2, 41, false}}}, 2, true, false},
    {ObjectKind::OBJECT_KIND_HISTORY, {93, 1}, 248, {{{2, 28, false}, {0, 0, false}}}, 1, true, false},
    {ObjectKind::OBJECT_KIND_ENERGY, {87, 1}, 232, {{{1, 8, true}, {0, 0, false}}}, 1, true, false},
    {ObjectKind::OBJECT_KIND_LOCAL_OPERATION, {86, 6}, 303, {{{1, 7, false}, {0, 0, false}}}, 1, true, false},
    {ObjectKind::OBJECT_KIND_PRIORITIZED_OPERATION, {86, 7}, 303, {{{1, 7, false}, {0, 0, false}}}, 1, true, false},
    {ObjectKind::OBJECT_KIND_REALIZED_OPERATION, {86, 8}, 303, {{{1, 7, false}, {0, 0, false}}}, 1, true, false},
    {ObjectKind::OBJECT_KIND_LOCAL_CONTROL, {86, 10}, 303, {{{1, 7, false}, {0, 0, false}}}, 1, true, true},
    {ObjectKind::OBJECT_KIND_CS_FACTORY_LIMITS, {86, 13}, 301, {{{1, 28, false}, {0, 0, false}}}, 1, true, false},
    {ObjectKind::OBJECT_KIND_CS_USER_CONFIG, {86, 14}, 302, {{{1, 18, false}, {0, 0, false}}}, 1, true, true},
    {ObjectKind::OBJECT_KIND_CP_FACTORY_LIMITS, {86, 15}, 301, {{{1, 28, false}, {0, 0, false}}}, 1, true, false},
    {ObjectKind::OBJECT_KIND_CP_USER_CONFIG, {86, 16}, 302, {{{1, 18, false}, {0, 0, false}}}, 1, true, true},
    {ObjectKind::OBJECT_KIND_PP_FACTORY_LIMITS, {86, 17}, 301, {{{1, 28, false}, {0, 0, false}}}, 1, true, false},
    {ObjectKind::OBJECT_KIND_PP_USER_CONFIG, {86, 18}, 302, {{{1, 18, false}, {0, 0, false}}}, 1, true, true},
    {ObjectKind::OBJECT_KIND_OPERATION_CONFIG, {86, 100}, 340, {{{1, 4, false}, {0, 0, false}}}, 1, true, true},
}};

constexpr DeviceProfile MODEL_B_PROFILE{
    "ALPHA3 Model B",
    52,
    1,
    0,
    MODEL_B_SCHEMAS.data(),
    static_cast<uint8_t>(MODEL_B_SCHEMAS.size()),
    MODEL_B_READ_CAPABILITIES,
    MODEL_B_WRITE_CAPABILITIES,
    CONTROL_MODE_VALUES,
    static_cast<uint8_t>(CONTROL_MODE_VALUES.size()),
    PASCALS_PER_METER,
};

bool has_capability(uint32_t capabilities, Capability capability) {
  const uint32_t requested = capability_mask(capability);
  return requested != 0 && (capabilities & requested) == requested;
}

Capability read_capability_for_object_(ObjectKind kind) {
  switch (kind) {
    case ObjectKind::OBJECT_KIND_HYDRAULIC_MODEL_B:
      return Capability::CAPABILITY_HYDRAULIC;
    case ObjectKind::OBJECT_KIND_ELECTRICAL:
      return Capability::CAPABILITY_ELECTRICAL;
    case ObjectKind::OBJECT_KIND_HISTORY:
      return Capability::CAPABILITY_HISTORY;
    case ObjectKind::OBJECT_KIND_ENERGY:
      return Capability::CAPABILITY_ENERGY;
    case ObjectKind::OBJECT_KIND_LOCAL_OPERATION:
    case ObjectKind::OBJECT_KIND_PRIORITIZED_OPERATION:
    case ObjectKind::OBJECT_KIND_REALIZED_OPERATION:
    case ObjectKind::OBJECT_KIND_LOCAL_CONTROL:
      return Capability::CAPABILITY_STATUS;
    case ObjectKind::OBJECT_KIND_CS_FACTORY_LIMITS:
    case ObjectKind::OBJECT_KIND_CS_USER_CONFIG:
    case ObjectKind::OBJECT_KIND_CP_FACTORY_LIMITS:
    case ObjectKind::OBJECT_KIND_CP_USER_CONFIG:
    case ObjectKind::OBJECT_KIND_PP_FACTORY_LIMITS:
    case ObjectKind::OBJECT_KIND_PP_USER_CONFIG:
      return Capability::CAPABILITY_SETPOINT_LIMITS;
    case ObjectKind::OBJECT_KIND_HYDRAULIC_EXTENDED:
    case ObjectKind::OBJECT_KIND_OPERATION_CONFIG:
      return Capability::CAPABILITY_NONE;
  }
  return Capability::CAPABILITY_NONE;
}

}  // namespace

ProfileMatch match_profile(const DeviceIdentity &identity) {
  if (!identity.complete() || identity.family != MODEL_B_PROFILE.family || identity.type != MODEL_B_PROFILE.type)
    return {};

  const bool exact = identity.version == MODEL_B_PROFILE.version;
  return {&MODEL_B_PROFILE, exact, exact};
}

const ObjectSchema *find_schema(const DeviceProfile &profile, ObjectKind kind) {
  for (uint8_t index = 0; index < profile.schema_count; index++) {
    const ObjectSchema &schema = profile.schemas[index];
    if (schema.kind == kind)
      return &schema;
  }
  return nullptr;
}

bool has_read_capability(const ProfileMatch &match, Capability capability) {
  if (match.profile == nullptr)
    return false;
  const uint32_t capabilities = match.exact ? match.profile->read_capabilities : PREFIX_COMPATIBLE_READ_CAPABILITIES;
  return has_capability(capabilities, capability);
}

bool has_write_capability(const ProfileMatch &match, Capability capability) {
  return match.profile != nullptr && match.writable && has_capability(match.profile->write_capabilities, capability);
}

bool can_read_object(const ProfileMatch &match, ObjectKind kind) {
  if (match.profile == nullptr)
    return false;
  const ObjectSchema *schema = find_schema(*match.profile, kind);
  if (schema == nullptr || !schema->readable)
    return false;
  if (match.exact)
    return true;
  return has_read_capability(match, read_capability_for_object_(kind));
}

}  // namespace esphome::alpha3
