#include <gtest/gtest.h>

#include "esphome/components/modbus/modbus.h"
#include "esphome/components/modbus_controller/modbus_controller.h"

namespace esphome::modbus_controller::testing {

class DummySensorItem : public SensorItem {
 public:
  void parse_and_publish(const std::vector<uint8_t> &data) override { (void) data; }
};

class TestModbusController : public ModbusController {
 public:
  using ModbusController::create_register_ranges_;
  using ModbusController::validate_command_response_;
};

TEST(ModbusControllerTest, RejectsInvalidReadPayloadLength) {
  modbus::Modbus bus;
  bus.set_role(modbus::ModbusRole::CLIENT);

  TestModbusController controller;
  controller.set_parent(&bus);
  controller.set_address(0x01);
  bus.register_device(&controller);

  DummySensorItem sensor;
  sensor.register_type = modbus::ModbusRegisterType::HOLDING;
  sensor.start_address = 0x0010;
  sensor.register_count = 1;
  controller.add_sensor_item(&sensor);

  ASSERT_EQ(controller.create_register_ranges_(), 1);

  auto command = ModbusCommandItem::create_read_command(&controller, modbus::ModbusRegisterType::HOLDING, 0x0010, 1);

  EXPECT_TRUE(controller.validate_command_response_(command, {0x12, 0x34}));
  EXPECT_FALSE(controller.validate_command_response_(command, {0x12, 0x34, 0x56, 0x78}));
}

TEST(ModbusControllerTest, UsesResponseSizeOverrideForCustomPayloadValidation) {
  modbus::Modbus bus;
  bus.set_role(modbus::ModbusRole::CLIENT);

  TestModbusController controller;
  controller.set_parent(&bus);
  controller.set_address(0x01);
  bus.register_device(&controller);

  DummySensorItem sensor;
  sensor.register_type = modbus::ModbusRegisterType::CUSTOM;
  sensor.start_address = 0x0B56;
  sensor.register_count = 1;
  sensor.response_bytes = 14;
  controller.add_sensor_item(&sensor);

  ASSERT_EQ(controller.create_register_ranges_(), 1);

  auto command = ModbusCommandItem::create_custom_command(&controller, std::vector<uint8_t>{0x44, 0xCE, 0xAA});
  command.register_type = modbus::ModbusRegisterType::CUSTOM;
  command.register_address = 0x0B56;
  command.register_count = 1;

  EXPECT_TRUE(controller.validate_command_response_(command, std::vector<uint8_t>(14, 0x00)));
  EXPECT_FALSE(controller.validate_command_response_(command, std::vector<uint8_t>(8, 0x00)));
}

}  // namespace esphome::modbus_controller::testing
