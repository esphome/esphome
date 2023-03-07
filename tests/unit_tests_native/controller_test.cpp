
#include <gtest/gtest.h>
#include "esphome/core/controller.h"
#include "mocks/mock_switch.h"
#include "mocks/mock_sensor.h"
#include "mocks/mock_custom_entity.h"

namespace esphome {
namespace test {

using ::testing::_;

struct Listener {
  virtual void send(Switch *obj, bool state) = 0;
  virtual void send(Sensor *obj, float state) = 0;
  virtual void send(CustomEntity *obj, bool state) = 0;
};

struct MockListener : public Listener {
  MOCK_METHOD(void, send, (Switch * obj, bool state), (override));        // NOLINT
  MOCK_METHOD(void, send, (Sensor * obj, float state), (override));       // NOLINT
  MOCK_METHOD(void, send, (CustomEntity * obj, bool state), (override));  // NOLINT
};

class ControllerTest : public ::testing::Test, public esphome::Controller {
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
    Controller::add_on_state_callback([this](Switch *obj) { this->listener_.send(obj, obj->state); },
                                      include_internal_);
    Controller::add_on_state_callback([this](Sensor *obj) { this->listener_.send(obj, obj->state); },
                                      include_internal_);
    Controller::add_on_state_callback([this](CustomEntity *obj) { this->listener_.send(obj, obj->state); },
                                      include_internal_);
  }

  template<typename Entity, typename T> void register_entity_(T &t, bool internal = false) {
    t.set_internal(internal);
    App.register_entity(static_cast<Entity *>(&t));
    if (!internal || include_internal_) {
      EXPECT_CALL(t, add_on_state_callback(_));
    }
  }

  template<typename T> static void clear(const T &arg) {
    const_cast<T &>(arg).clear();
    ASSERT_EQ(arg.size(), 0);
  }

  template<typename Tuple, size_t... Indices> void clear_impl(const Tuple &tuple, index_sequence<Indices...>) {
    int dummy[] = {0, ((void) clear(std::get<Indices>(tuple)), 0)...};
    (void) dummy;
  }

  template<typename... Args> void clear(const std::tuple<Args...> &tuple) {
    clear_impl(tuple, make_index_sequence<sizeof...(Args)>{});
  }

  void TearDown() override { clear(App.get_entities()); }

  MockSwitch sw_1_;
  MockSwitch sw_2_;
  MockSwitch sw_3_;
  MockSensor sensor_1_;
  MockSensor sensor_2_;
  MockSensor sensor_3_;
  MockCustomEntity custom_1_;
  MockCustomEntity custom_2_;
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
  EXPECT_CALL(listener_, send(&sw_2_, _)).Times(0);
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
  EXPECT_CALL(listener_, send(&sensor_2_, _)).Times(0);
  EXPECT_CALL(listener_, send(&sensor_3_, _)).Times(0);
  sensor_1_.set(true);
  sensor_1_.set(false);
  sensor_2_.set(true);
  sensor_2_.set(false);
  sensor_3_.set(true);
  sensor_3_.set(false);
  EXPECT_CALL(listener_, send(&custom_1_, true));
  EXPECT_CALL(listener_, send(&custom_1_, false));
  EXPECT_CALL(listener_, send(&custom_2_, _)).Times(0);
  custom_1_.set(true);
  custom_1_.set(false);
  custom_2_.set(true);
  custom_2_.set(false);
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
  EXPECT_CALL(listener_, send(&custom_1_, true));
  EXPECT_CALL(listener_, send(&custom_1_, false));
  EXPECT_CALL(listener_, send(&custom_2_, true));
  EXPECT_CALL(listener_, send(&custom_2_, false));
  custom_1_.set(true);
  custom_1_.set(false);
  custom_2_.set(true);
  custom_2_.set(false);
}

}  // namespace test
}  // namespace esphome
