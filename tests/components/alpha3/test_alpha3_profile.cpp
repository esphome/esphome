#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome/components/alpha3/alpha3_profile.h"

namespace esphome::alpha3::testing {
namespace {

struct SchemaExpectation {
  ObjectKind kind;
  ObjectAddress address;
  uint16_t object_type;
  std::array<ObjectVersionSpec, 2> versions;
  uint8_t version_count;
  bool readable;
  bool writable;
};

constexpr std::array<SchemaExpectation, 15> MODEL_B_SCHEMAS{{
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

constexpr std::array<uint8_t, 6> EXPECTED_CONTROL_MODES{{0, 1, 2, 13, 14, 15}};

}  // namespace

TEST(Alpha3ProfileMatch, ExactIdentityEnablesDeclaredReadAndWriteCapabilities) {
  const ProfileMatch match = match_profile({52, 1, 0, 0x07});
  ASSERT_NE(match.profile, nullptr);
  EXPECT_STREQ(match.profile->name, "ALPHA3 Model B");
  EXPECT_TRUE(match.exact);
  EXPECT_TRUE(match.writable);
  EXPECT_TRUE(has_read_capability(match, Capability::CAPABILITY_HYDRAULIC));
  EXPECT_TRUE(has_read_capability(match, Capability::CAPABILITY_ELECTRICAL));
  EXPECT_TRUE(has_read_capability(match, Capability::CAPABILITY_HISTORY));
  EXPECT_TRUE(has_read_capability(match, Capability::CAPABILITY_ENERGY));
  EXPECT_TRUE(has_read_capability(match, Capability::CAPABILITY_STATUS));
  EXPECT_TRUE(has_read_capability(match, Capability::CAPABILITY_SETPOINT_LIMITS));
  EXPECT_TRUE(has_write_capability(match, Capability::CAPABILITY_WRITE_OPERATION));
  EXPECT_TRUE(has_write_capability(match, Capability::CAPABILITY_WRITE_CONTROL));
  EXPECT_TRUE(has_write_capability(match, Capability::CAPABILITY_WRITE_SETPOINT));
}

TEST(Alpha3ProfileMatch, NewerKnownIdentityEnablesOnlyValidatedReadCapabilities) {
  const ProfileMatch match = match_profile({52, 1, 1, 0x07});
  const ProfileMatch exact_match = match_profile({52, 1, 0, 0x07});
  ASSERT_NE(match.profile, nullptr);
  EXPECT_EQ(match.profile, exact_match.profile);
  EXPECT_FALSE(match.exact);
  EXPECT_FALSE(match.writable);
  EXPECT_FALSE(has_read_capability(match, Capability::CAPABILITY_HYDRAULIC));
  EXPECT_TRUE(has_read_capability(match, Capability::CAPABILITY_ELECTRICAL));
  EXPECT_TRUE(has_read_capability(match, Capability::CAPABILITY_HISTORY));
  EXPECT_TRUE(has_read_capability(match, Capability::CAPABILITY_ENERGY));
  EXPECT_TRUE(has_read_capability(match, Capability::CAPABILITY_STATUS));
  EXPECT_TRUE(has_read_capability(match, Capability::CAPABILITY_SETPOINT_LIMITS));
  EXPECT_FALSE(has_write_capability(match, Capability::CAPABILITY_WRITE_OPERATION));
  EXPECT_FALSE(has_write_capability(match, Capability::CAPABILITY_WRITE_CONTROL));
  EXPECT_FALSE(has_write_capability(match, Capability::CAPABILITY_WRITE_SETPOINT));
}

TEST(Alpha3ProfileReadPolicy, NewerKnownIdentityPermitsOnlyCompatibleObjectReads) {
  const ProfileMatch match = match_profile({52, 1, 1, 0x07});
  ASSERT_NE(match.profile, nullptr);

  for (const ObjectKind kind : {
           ObjectKind::OBJECT_KIND_ELECTRICAL,
           ObjectKind::OBJECT_KIND_HISTORY,
           ObjectKind::OBJECT_KIND_ENERGY,
           ObjectKind::OBJECT_KIND_LOCAL_OPERATION,
           ObjectKind::OBJECT_KIND_PRIORITIZED_OPERATION,
           ObjectKind::OBJECT_KIND_REALIZED_OPERATION,
           ObjectKind::OBJECT_KIND_LOCAL_CONTROL,
           ObjectKind::OBJECT_KIND_CS_FACTORY_LIMITS,
           ObjectKind::OBJECT_KIND_CS_USER_CONFIG,
           ObjectKind::OBJECT_KIND_CP_FACTORY_LIMITS,
           ObjectKind::OBJECT_KIND_CP_USER_CONFIG,
           ObjectKind::OBJECT_KIND_PP_FACTORY_LIMITS,
           ObjectKind::OBJECT_KIND_PP_USER_CONFIG,
       })
    EXPECT_TRUE(can_read_object(match, kind)) << static_cast<int>(kind);

  EXPECT_FALSE(can_read_object(match, ObjectKind::OBJECT_KIND_HYDRAULIC_MODEL_B));
  EXPECT_FALSE(can_read_object(match, ObjectKind::OBJECT_KIND_OPERATION_CONFIG));
  EXPECT_FALSE(has_write_capability(match, Capability::CAPABILITY_WRITE_OPERATION));
  EXPECT_FALSE(has_write_capability(match, Capability::CAPABILITY_WRITE_CONTROL));
  EXPECT_FALSE(has_write_capability(match, Capability::CAPABILITY_WRITE_SETPOINT));
}

TEST(Alpha3ProfileMatch, IncompleteAndUnknownIdentitiesExposeNoProfileOrCapabilities) {
  for (const DeviceIdentity identity : {
           DeviceIdentity{52, 1, 0, 0x03},
           DeviceIdentity{53, 1, 0, 0x07},
           DeviceIdentity{52, 27, 0, 0x07},
       }) {
    const ProfileMatch match = match_profile(identity);
    EXPECT_EQ(match.profile, nullptr);
    EXPECT_FALSE(match.exact);
    EXPECT_FALSE(match.writable);
    for (const Capability capability : {
             Capability::CAPABILITY_HYDRAULIC,
             Capability::CAPABILITY_ELECTRICAL,
             Capability::CAPABILITY_HISTORY,
             Capability::CAPABILITY_ENERGY,
             Capability::CAPABILITY_STATUS,
             Capability::CAPABILITY_SETPOINT_LIMITS,
         })
      EXPECT_FALSE(has_read_capability(match, capability));
    for (const Capability capability : {
             Capability::CAPABILITY_WRITE_OPERATION,
             Capability::CAPABILITY_WRITE_CONTROL,
             Capability::CAPABILITY_WRITE_SETPOINT,
         })
      EXPECT_FALSE(has_write_capability(match, capability));
  }
}

TEST(Alpha3ProfileSchema, ExactProfileHasTheFixedModelBSchemasAndNoExtendedHydraulicSchema) {
  const ProfileMatch match = match_profile({52, 1, 0, 0x07});
  ASSERT_NE(match.profile, nullptr);
  EXPECT_EQ(match.profile->schema_count, 15);

  for (const auto &expected : MODEL_B_SCHEMAS) {
    const ObjectSchema *actual = find_schema(*match.profile, expected.kind);
    ASSERT_NE(actual, nullptr);
    EXPECT_EQ(actual->address.data_id, expected.address.data_id);
    EXPECT_EQ(actual->address.sub_id, expected.address.sub_id);
    EXPECT_EQ(actual->object_type, expected.object_type);
    EXPECT_EQ(actual->version_count, expected.version_count);
    EXPECT_EQ(actual->readable, expected.readable);
    EXPECT_EQ(actual->writable, expected.writable);
    for (uint8_t index = 0; index < expected.version_count; index++) {
      EXPECT_EQ(actual->versions[index].version, expected.versions[index].version);
      EXPECT_EQ(actual->versions[index].payload_size, expected.versions[index].payload_size);
      EXPECT_EQ(actual->versions[index].allow_longer, expected.versions[index].allow_longer);
    }
  }

  EXPECT_EQ(EXTENDED_HYDRAULIC_SCHEMA.address.data_id, 93);
  EXPECT_EQ(EXTENDED_HYDRAULIC_SCHEMA.address.sub_id, 290);
  EXPECT_EQ(EXTENDED_HYDRAULIC_SCHEMA.object_type, 565);
  EXPECT_EQ(EXTENDED_HYDRAULIC_SCHEMA.version_count, 1);
  EXPECT_EQ(EXTENDED_HYDRAULIC_SCHEMA.versions[0].version, 2);
  EXPECT_EQ(EXTENDED_HYDRAULIC_SCHEMA.versions[0].payload_size, 36);
  EXPECT_FALSE(EXTENDED_HYDRAULIC_SCHEMA.versions[0].allow_longer);
}

TEST(Alpha3ProfileSchema, ExactProfileOnlyWritesOperationControlAndSetpointObjects) {
  const ProfileMatch match = match_profile({52, 1, 0, 0x07});
  ASSERT_NE(match.profile, nullptr);

  for (const auto &expected : MODEL_B_SCHEMAS) {
    const ObjectSchema *schema = find_schema(*match.profile, expected.kind);
    ASSERT_NE(schema, nullptr);
    EXPECT_EQ(schema->writable, expected.writable) << static_cast<int>(expected.kind);
  }
}

TEST(Alpha3ProfileSchema, ExposesExactlySixControlModesAndThePressureConversion) {
  const ProfileMatch match = match_profile({52, 1, 0, 0x07});
  ASSERT_NE(match.profile, nullptr);
  EXPECT_EQ(match.profile->control_mode_count, 6);
  EXPECT_FLOAT_EQ(match.profile->pascals_per_meter, 9804.0F);

  std::array<uint8_t, 6> iterated_modes{};
  size_t iterated_count = 0;
  for (uint8_t index = 0; index < match.profile->control_mode_count; index++)
    iterated_modes[iterated_count++] = match.profile->control_modes[index];
  EXPECT_EQ(iterated_count, 6U);
  EXPECT_EQ(iterated_modes, EXPECTED_CONTROL_MODES);
}

}  // namespace esphome::alpha3::testing
