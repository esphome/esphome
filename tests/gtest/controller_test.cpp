
#include <gtest/gtest.h>
#include "esphome/core/controller.h"
#include "mocks/mock_switch.h"
#include "mocks/mock_sensor.h"

namespace esphome {
namespace test {

using ::testing::_;

struct Listener {
  virtual void send(Switch *obj, bool state) = 0;
  virtual void send(Sensor *obj, float state) = 0;
};

struct MockListener : public Listener {
  MOCK_METHOD(void, send, (Switch * obj, bool state), (override));   // NOLINT
  MOCK_METHOD(void, send, (Sensor * obj, float state), (override));  // NOLINT
};

class ControllerTest : public ::testing::Test, public esphome::Controller {
 protected:
  void SetUp() override {  // NOLINT
    ASSERT_EQ(App.get_entity_all().size(), 0);
    register_entity_(sw_1_);
    register_entity_(sw_2_, true);
    register_entity_(sw_3_);
    register_entity_(sensor_1_);
    register_entity_(sensor_2_, true);
    register_entity_(sensor_3_, true);
    ASSERT_EQ(App.get_entity_all().size(), std::max(SWITCH, SENSOR) + 1);
    setup_controller(include_internal_);
    Controller::add_on_state_callback([this](Switch *obj, bool state) { this->listener_.send(obj, state); });
    Controller::add_on_state_callback([this](Sensor *obj, float state) { this->listener_.send(obj, state); });
  }

  template<typename T> void register_entity_(T &t, bool internal = false) {
    t.set_internal(internal);
    App.register_entity(&t);
    if (!internal || include_internal_) {
      EXPECT_CALL(t, add_on_state_callback(_));
    }
  }

  void TearDown() override {  // NOLINT
    const_cast<std::vector<std::vector<EntityBase *>> &>(App.get_entity_all()).clear();
    ASSERT_EQ(App.get_entity_all().size(), 0);
  }

  MockSwitch sw_1_;
  MockSwitch sw_2_;
  MockSwitch sw_3_;
  MockSensor sensor_1_;
  MockSensor sensor_2_;
  MockSensor sensor_3_;
  bool include_internal_{false};
  MockListener listener_;
};

class ControllerTestWithInternal : public ControllerTest {
  void SetUp() override {
    include_internal_ = true;
    ControllerTest::SetUp();
  }
};

TEST_F(ControllerTest, RegisterCallbacks) {}  // NOLINT

TEST_F(ControllerTest, InvokeCallbacks) {  // NOLINT
  EXPECT_CALL(listener_, send(&sw_1_, true));
  EXPECT_CALL(listener_, send(&sw_1_, false));
  EXPECT_CALL(listener_, send(&sw_3_, true));
  EXPECT_CALL(listener_, send(&sw_3_, false));
  sw_1_.set(true);
  sw_1_.set(false);
  sw_2_.set(true);
  sw_2_.set(false);
  sw_3_.set(true);
  sw_3_.set(false);
  EXPECT_CALL(listener_, send(&sensor_1_, true));
  EXPECT_CALL(listener_, send(&sensor_1_, false));
  sensor_1_.set(true);
  sensor_1_.set(false);
  sensor_2_.set(true);
  sensor_2_.set(false);
  sensor_3_.set(true);
  sensor_3_.set(false);
}

TEST_F(ControllerTestWithInternal, RegisterCallbacks) {}  // NOLINT

TEST_F(ControllerTestWithInternal, InvokeCallbacks) {  // NOLINT
  EXPECT_CALL(listener_, send(&sw_1_, true));
  EXPECT_CALL(listener_, send(&sw_1_, false));
  EXPECT_CALL(listener_, send(&sw_2_, true));
  EXPECT_CALL(listener_, send(&sw_2_, false));
  EXPECT_CALL(listener_, send(&sw_3_, true));
  EXPECT_CALL(listener_, send(&sw_3_, false));
  sw_1_.set(true);
  sw_1_.set(false);
  sw_2_.set(true);
  sw_2_.set(false);
  sw_3_.set(true);
  sw_3_.set(false);
  EXPECT_CALL(listener_, send(&sensor_1_, true));
  EXPECT_CALL(listener_, send(&sensor_1_, false));
  EXPECT_CALL(listener_, send(&sensor_2_, true));
  EXPECT_CALL(listener_, send(&sensor_2_, false));
  EXPECT_CALL(listener_, send(&sensor_3_, true));
  EXPECT_CALL(listener_, send(&sensor_3_, false));
  sensor_1_.set(true);
  sensor_1_.set(false);
  sensor_2_.set(true);
  sensor_2_.set(false);
  sensor_3_.set(true);
  sensor_3_.set(false);
}

}  // namespace test
}  // namespace esphome
