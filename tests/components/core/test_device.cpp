#include "esphome/core/controller.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/device.h"

#include <gtest/gtest.h>

namespace esphome::core::testing {

class DeviceUpdateController final : public Controller {
 public:
  void on_device_update(Device *device) override {
    this->last_device = device;
    this->update_count++;
  }

  Device *last_device{nullptr};
  int update_count{0};
};

TEST(Device, AvailabilityDefaultsToTrueAndNotifiesOnlyOnChange) {
  static DeviceUpdateController controller;
  ControllerRegistry::register_controller(&controller);

  Device device;
  EXPECT_TRUE(device.is_available());
  EXPECT_EQ(controller.update_count, 0);

  device.set_available(false);
  EXPECT_FALSE(device.is_available());
  EXPECT_EQ(controller.last_device, &device);
  EXPECT_EQ(controller.update_count, 1);

  device.set_available(false);
  EXPECT_EQ(controller.update_count, 1);

  device.set_available(true);
  EXPECT_TRUE(device.is_available());
  EXPECT_EQ(controller.last_device, &device);
  EXPECT_EQ(controller.update_count, 2);
}

}  // namespace esphome::core::testing
