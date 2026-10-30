#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <map>
#include <set>

#include "esphome/components/vbus/vbus_cs_plus_policy.h"

namespace esphome::vbus::cs_plus::testing {
namespace {

class CSPlusPolicyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    for (size_t i = 0; i < SETTING_COUNT; ++i) {
      const auto spec = setting_at(i);
      if (!spec) {
        FAIL() << "Missing setting metadata";
      }
      this->values_[spec->index] = spec->minimum;
    }
    this->values_[1] = 1;
    this->values_[10] = 0;
  }
  int32_t get_(uint16_t index) const { return this->values_.at(index); }
  bool allowed_(uint16_t index, int32_t value) const {
    const auto spec = find_setting(index);
    return spec.has_value() && setting_allowed(*spec, value, [this](uint16_t id) { return this->get_(id); });
  }
  bool visible_(uint16_t index) const {
    const auto spec = find_setting(index);
    return spec.has_value() && setting_visible(*spec, [this](uint16_t id) { return this->get_(id); });
  }
  std::map<uint16_t, int32_t> values_;
};

TEST_F(CSPlusPolicyTest, MetadataHas84UniqueSettingsAndCompleteBoundedDependencies) {
  std::set<uint16_t> indices;
  size_t numbers = 0;
  size_t selects = 0;
  uint16_t previous = 0;
  for (size_t i = 0; i < SETTING_COUNT; ++i) {
    const auto spec = setting_at(i);
    if (!spec) {
      FAIL() << "Missing setting metadata";
    }
    ASSERT_TRUE(indices.insert(spec->index).second);
    EXPECT_GT(spec->index, previous);
    previous = spec->index;
    const auto found = find_setting(spec->index);
    if (!found) {
      FAIL() << "Missing setting metadata";
    }
    EXPECT_EQ(found->minimum, spec->minimum);
    EXPECT_EQ(found->maximum, spec->maximum);
    EXPECT_GT(spec->step, 0);
    EXPECT_GT(spec->scale, 0);
    EXPECT_LE(spec->dependency_count, spec->dependencies.size());
    std::set<uint16_t> dependencies;
    for (size_t j = 0; j < spec->dependency_count; ++j) {
      EXPECT_TRUE(dependencies.insert(spec->dependencies[j]).second);
      EXPECT_TRUE(find_setting(spec->dependencies[j]).has_value());
    }
    EXPECT_TRUE(dependencies.contains(spec->index));
    EXPECT_TRUE(dependencies.contains(1));
    EXPECT_TRUE(dependencies.contains(10));
    spec->enum_mask == 0 ? ++numbers : ++selects;
  }
  EXPECT_EQ(numbers, 58u);
  EXPECT_EQ(selects, 26u);
  EXPECT_FALSE(setting_at(84).has_value());
  EXPECT_FALSE(setting_at(std::numeric_limits<size_t>::max()).has_value());
  for (uint16_t invalid : {0, 3, 0x7fff, 0xffff})
    EXPECT_FALSE(find_setting(invalid).has_value());
}

TEST_F(CSPlusPolicyTest, LookupMatchesTheCanonicalSetForEvery16BitIdentifier) {
  for (uint32_t index = 0; index <= 0xffff; ++index) {
    const auto found = find_setting(index);
    EXPECT_EQ(found.has_value(), this->values_.contains(index)) << index;
    if (found.has_value())
      EXPECT_EQ(found->index, index);
  }
}

TEST_F(CSPlusPolicyTest, EverySettingRejectsOutOfRangeAndOffStepRawValues) {
  for (size_t i = 0; i < SETTING_COUNT; ++i) {
    const auto spec = setting_at(i);
    if (!spec) {
      FAIL() << "Missing setting metadata";
    }
    SCOPED_TRACE(spec->index);
    EXPECT_TRUE(setting_shape_valid(*spec, spec->minimum));
    EXPECT_FALSE(setting_shape_valid(*spec, spec->minimum - 1));
    EXPECT_FALSE(setting_shape_valid(*spec, spec->maximum + 1));
    EXPECT_FALSE(setting_shape_valid(*spec, std::numeric_limits<int32_t>::min()));
    EXPECT_FALSE(setting_shape_valid(*spec, std::numeric_limits<int32_t>::max()));
    if (spec->step > 1)
      EXPECT_FALSE(setting_shape_valid(*spec, spec->minimum + 1));
  }
}

TEST_F(CSPlusPolicyTest, RejectsEverySettingWithInvalidSystemScheme) {
  for (int32_t scheme : {-1, 0, 11, 32, std::numeric_limits<int32_t>::max()}) {
    this->values_[1] = scheme;
    for (size_t i = 0; i < SETTING_COUNT; ++i) {
      const auto spec = setting_at(i);
      if (!spec) {
        FAIL() << "Missing setting metadata";
      }
      EXPECT_FALSE(this->visible_(spec->index));
    }
  }
}

TEST_F(CSPlusPolicyTest, PumpLimitsRequireTheMatchingControl) {
  this->values_[1] = 2;
  for (uint16_t minimum : {48, 49}) {
    const uint16_t maximum = minimum + 2;
    const uint16_t control = minimum == 48 ? 12549 : 12550;
    this->values_[control] = 0;
    EXPECT_FALSE(this->visible_(minimum));
    EXPECT_FALSE(this->visible_(maximum));
    this->values_[control] = 2;
    EXPECT_TRUE(this->visible_(minimum));
    EXPECT_TRUE(this->visible_(maximum));
  }
}

TEST_F(CSPlusPolicyTest, DrainbackHidesCoolingAndShowsItsOwnSettings) {
  this->values_[461] = 1;
  EXPECT_TRUE(this->visible_(455));
  EXPECT_FALSE(this->visible_(2771));
  this->values_[2768] = 1;
  EXPECT_FALSE(this->visible_(455));
  EXPECT_TRUE(this->visible_(2771));
}

TEST_F(CSPlusPolicyTest, HeatMeterAndFluidSettingsRequireTheirFunctions) {
  this->values_[81] = 0;
  EXPECT_FALSE(this->visible_(90));
  EXPECT_FALSE(this->visible_(91));
  this->values_[81] = 1;
  EXPECT_TRUE(this->visible_(90));
  this->values_[90] = 0;
  EXPECT_FALSE(this->visible_(91));
  this->values_[90] = 1;
  EXPECT_TRUE(this->visible_(91));
  this->values_[90] = 2;
  EXPECT_TRUE(this->visible_(91));
  this->values_[90] = 3;
  EXPECT_FALSE(this->visible_(91));
}

TEST_F(CSPlusPolicyTest, DisinfectionAndTubeCollectorChildrenRequireTheirFunctions) {
  this->values_[1] = 3;
  for (uint16_t index : {2561, 2563, 2565, 2566}) {
    EXPECT_FALSE(this->visible_(index));
    this->values_[2560] = 1;
    EXPECT_TRUE(this->visible_(index));
    this->values_[2560] = 0;
  }
  for (uint16_t index : {3904, 3905, 3906, 3907}) {
    EXPECT_FALSE(this->visible_(index));
    this->values_[3897] = 1;
    EXPECT_TRUE(this->visible_(index));
    this->values_[3897] = 0;
  }
}

TEST_F(CSPlusPolicyTest, DeltaOnOffAndNominalRetainStrictOrderingOnAllThreeCircuits) {
  const std::array<uint16_t, 3> on{385, 388, 391};
  const std::array<uint16_t, 3> off{401, 404, 407};
  const std::array<uint16_t, 3> nominal{411, 414, 417};
  for (size_t i = 0; i < on.size(); ++i) {
    this->values_[1] = i == 0 ? 1 : i == 1 ? 4 : 2;
    this->values_[on[i]] = 100;
    this->values_[off[i]] = 40;
    this->values_[nominal[i]] = 120;
    EXPECT_TRUE(this->allowed_(on[i], 45));
    EXPECT_TRUE(this->allowed_(on[i], 115));
    EXPECT_FALSE(this->allowed_(on[i], 40));
    EXPECT_FALSE(this->allowed_(on[i], 120));
    EXPECT_TRUE(this->allowed_(off[i], 95));
    EXPECT_FALSE(this->allowed_(off[i], 100));
    EXPECT_TRUE(this->allowed_(nominal[i], 105));
    EXPECT_FALSE(this->allowed_(nominal[i], 100));
  }
}

TEST_F(CSPlusPolicyTest, WritesRespectPumpLimitsAndStrictOrdering) {
  this->values_[1] = 2;
  for (uint16_t minimum : {48, 49}) {
    const uint16_t maximum = minimum + 2;
    const uint16_t control = minimum == 48 ? 12549 : 12550;
    this->values_[minimum] = 35;
    this->values_[maximum] = 65;
    this->values_[control] = 0;
    EXPECT_FALSE(this->visible_(minimum));
    EXPECT_FALSE(this->allowed_(minimum, 30));
    this->values_[control] = 2;
    EXPECT_TRUE(this->visible_(minimum));
    EXPECT_TRUE(this->allowed_(minimum, 60));
    EXPECT_FALSE(this->allowed_(minimum, 65));
    EXPECT_TRUE(this->allowed_(maximum, 40));
    EXPECT_FALSE(this->allowed_(maximum, 35));
  }
}

TEST_F(CSPlusPolicyTest, WritesRespectDrainbackCoolingAndEmergencyLimits) {
  this->values_[461] = 1;
  this->values_[455] = 1100;
  this->values_[448] = 1300;
  EXPECT_TRUE(this->allowed_(455, 1100));
  EXPECT_FALSE(this->allowed_(455, 1200));
  EXPECT_TRUE(this->allowed_(448, 1210));
  EXPECT_FALSE(this->allowed_(448, 1200));
  this->values_[2768] = 1;
  EXPECT_FALSE(this->visible_(455));
  EXPECT_FALSE(this->allowed_(455, 1100));
  EXPECT_TRUE(this->allowed_(448, 1200));
  EXPECT_FALSE(this->allowed_(448, 1210));
  EXPECT_TRUE(this->visible_(2771));
  EXPECT_TRUE(this->allowed_(2771, 50));
}

TEST_F(CSPlusPolicyTest, WritesRespectFlowMeterAndFluidDependencies) {
  this->values_[81] = 0;
  EXPECT_FALSE(this->visible_(90));
  this->values_[81] = 1;
  this->values_[74] = 3;
  EXPECT_TRUE(this->allowed_(90, 0));
  EXPECT_FALSE(this->allowed_(90, 1));
  this->values_[90] = 1;
  EXPECT_FALSE(this->allowed_(74, 3));
  EXPECT_TRUE(this->visible_(91));
  this->values_[1] = 2;
  EXPECT_TRUE(this->allowed_(85, 2));
  EXPECT_FALSE(this->allowed_(85, 1));
  this->values_[1] = 6;
  EXPECT_TRUE(this->allowed_(85, 1));
  EXPECT_FALSE(this->allowed_(85, 0));
}

}  // namespace
}  // namespace esphome::vbus::cs_plus::testing
