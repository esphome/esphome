
#include <gtest/gtest.h>
#include "mocks/mock_switch.h"
#include "mocks/mock_sensor.h"
#include "mocks/mock_custom_entity.h"
#include "esphome/core/component_iterator.h"

// TODO add test for custo components

namespace esphome {
namespace test {

using ::testing::Return;
using ::testing::An;

struct Visitor {
  virtual bool visit(Switch *obj) = 0;
  virtual bool visit(Sensor *obj) = 0;
  virtual bool visit(CustomEntity *obj) = 0;
};

struct MockVisitor : public Visitor {
  MockVisitor() {
    ON_CALL(*this, visit(An<Switch *>())).WillByDefault(Return(true));
    ON_CALL(*this, visit(An<Sensor *>())).WillByDefault(Return(true));
    ON_CALL(*this, visit(An<CustomEntity *>())).WillByDefault(Return(true));
  }

  MOCK_METHOD(bool, visit, (Switch * obj), (override));        // NOLINT
  MOCK_METHOD(bool, visit, (Sensor * obj), (override));        // NOLINT
  MOCK_METHOD(bool, visit, (CustomEntity * obj), (override));  // NOLINT
};

class ComponentIteratorTest : public ::testing::Test, public ComponentIterator {
 protected:
  void SetUp() override {  // NOLINT
    // ASSERT_EQ(App.get_entities().size(), 0);
    register_entity_<Switch>(sw_1_);
    register_entity_<Switch>(sw_2_, true);
    register_entity_<Switch>(sw_3_);
    register_entity_<Sensor>(sensor_1_);
    register_entity_<Sensor>(sensor_2_, true);
    register_entity_<Sensor>(sensor_3_, true);
    // ASSERT_EQ(App.get_entities().size(), std::max(SWITCH, SENSOR) + 1);
    register_entity_<CustomEntity>(custom_1_);
    register_entity_<CustomEntity>(custom_2_, true);
    on_entity_callback([this](Switch *obj) { return this->visitor_.visit(obj); });
    on_entity_callback([this](Sensor *obj) { return this->visitor_.visit(obj); });
    on_entity_callback([this](CustomEntity *obj) { return this->visitor_.visit(obj); });
  }

  template<typename T> static void clear(const T &arg) {
    const_cast<T &>(arg).clear();
    ASSERT_EQ(arg.size(), 0);
  }

  void TearDown() override {
    std::apply([](auto &&...args) { (clear(args), ...); }, App.get_entities());
  }

  template<typename Entity, typename T> void register_entity_(T &t, bool internal = false) {
    t.set_internal(internal);
    App.register_entity(static_cast<Entity *>(&t));
  }

  bool on_end() override {
    has_next_ = false;
    return true;
  }

  void visit_all_(bool include_internal) {
    begin(include_internal);
    while (has_next_) {
      advance();
    }
  }

  MockSwitch sw_1_;
  MockSwitch sw_2_;
  MockSwitch sw_3_;
  MockSensor sensor_1_;
  MockSensor sensor_2_;
  MockSensor sensor_3_;
  MockCustomEntity custom_1_;
  MockCustomEntity custom_2_;

  MockVisitor visitor_;
  bool has_next_{true};
};

TEST_F(ComponentIteratorTest, Empty) {}  // NOLINT

TEST_F(ComponentIteratorTest, All) {  // NOLINT
  EXPECT_CALL(visitor_, visit(&sw_1_));
  EXPECT_CALL(visitor_, visit(&sw_2_)).Times(0);
  EXPECT_CALL(visitor_, visit(&sw_3_));
  EXPECT_CALL(visitor_, visit(&sensor_1_));
  EXPECT_CALL(visitor_, visit(&sensor_2_)).Times(0);
  EXPECT_CALL(visitor_, visit(&sensor_3_)).Times(0);
  EXPECT_CALL(visitor_, visit(&custom_1_));
  EXPECT_CALL(visitor_, visit(&custom_2_)).Times(0);
  visit_all_(false);
}

TEST_F(ComponentIteratorTest, WithInternal) {  // NOLINT
  EXPECT_CALL(visitor_, visit(&sw_1_));
  EXPECT_CALL(visitor_, visit(&sw_2_));
  EXPECT_CALL(visitor_, visit(&sw_3_));
  EXPECT_CALL(visitor_, visit(&sensor_1_));
  EXPECT_CALL(visitor_, visit(&sensor_2_));
  EXPECT_CALL(visitor_, visit(&sensor_3_));
  EXPECT_CALL(visitor_, visit(&custom_1_));
  EXPECT_CALL(visitor_, visit(&custom_2_));
  visit_all_(true);
}

}  // namespace test
}  // namespace esphome
