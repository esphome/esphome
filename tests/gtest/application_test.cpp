
#include <gtest/gtest.h>
#include "mocks/mock_switch.h"
#include "mocks/mock_sensor.h"
#include "mocks/mock_custom_entiry.h"
#include "esphome/core/application.h"

namespace esphome {
Application App;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
namespace test {

class ApplicationTest : public ::testing::Test {
 protected:
  void SetUp() override {  // NOLINT
    ASSERT_EQ(App.get_entities_all_types().size(), 0);
    register_entity_(sw_1_);
    register_entity_(sw_2_, true);
    register_entity_(sw_3_);
    register_entity_(sensor_1_);
    register_entity_(sensor_2_, true);
    register_entity_(sensor_3_, true);
    ASSERT_EQ(App.get_entities_all_types().size(), std::max(SWITCH, SENSOR) + 1);
  }

  template<typename T> void register_entity_(T &t, bool internal = false) {
    t.set_internal(internal);
    t.set_name(std::to_string(id_++));
    App.register_entity(&t);
  }

  void TearDown() override {  // NOLINT
    const_cast<std::vector<std::vector<EntityBase *>> &>(App.get_entities_all_types()).clear();
    ASSERT_EQ(App.get_entities_all_types().size(), 0);
  }

  MockSwitch sw_1_;
  MockSwitch sw_2_;
  MockSwitch sw_3_;
  MockSensor sensor_1_;
  MockSensor sensor_2_;
  MockSensor sensor_3_;
  int id_{0};
};

TEST_F(ApplicationTest, GetEntity) {  // NOLINT
  EXPECT_EQ(App.get_entities<Sensor>().size(), 3);
  EXPECT_EQ(App.get_entities<Switch>().size(), 3);
  EXPECT_EQ(App.get_entities<CustomEntity>().size(), 0);
  MockCustomEntity ce_1;
  App.register_entity(&ce_1);
  EXPECT_EQ(App.get_entities<CustomEntity>().size(), 1);
  MockCustomEntity ce_2;
  App.register_entity(&ce_2);
  EXPECT_EQ(App.get_entities<CustomEntity>().size(), 2);
}

TEST_F(ApplicationTest, GetEntityByKey) {  // NOLINT
  EXPECT_NE(App.get_entity_by_key<Switch>(sw_1_.get_object_id_hash()), nullptr);
  EXPECT_EQ(App.get_entity_by_key<Sensor>(sw_1_.get_object_id_hash()), nullptr);
  EXPECT_EQ(App.get_entity_by_key<Switch>(sw_2_.get_object_id_hash()), nullptr);
  EXPECT_NE(App.get_entity_by_key<Switch>(sw_2_.get_object_id_hash(), true), nullptr);
}

}  // namespace test
}  // namespace esphome
