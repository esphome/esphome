#include "common.h"

#include <cmath>
#include <new>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::mill_panelheater_gen2::testing {

class MillPanelHeaterGen2TestEnvironment final : public ::testing::Environment {
 public:
  void SetUp() override { new (&App) Application(); }
  void TearDown() override { App.~Application(); }
};

[[maybe_unused]] const auto *const TEST_ENVIRONMENT =
    ::testing::AddGlobalTestEnvironment(new MillPanelHeaterGen2TestEnvironment());

TEST(MillPanelHeaterGen2Test, InitialClimateStateIsUnknownUntilFirstStatusFrame) {
  TestableMillPanelHeaterGen2 heater;

  EXPECT_TRUE(std::isnan(heater.target_temperature));
  EXPECT_TRUE(std::isnan(heater.current_temperature));
  EXPECT_EQ(heater.mode, climate::CLIMATE_MODE_OFF);
  EXPECT_EQ(heater.action, climate::CLIMATE_ACTION_OFF);
}

TEST(MillPanelHeaterGen2Test, SendsPowerOnFrame) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);

  heater.send_power_command(0x01);

  const std::vector<uint8_t> expected{
      0x5A, 0x00, 0x10, 0x06, 0x00, 0x47, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5E, 0x5B,
  };
  EXPECT_EQ(uart.tx, expected);
}

TEST(MillPanelHeaterGen2Test, SendsTemperatureSixDegreeFrame) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);

  heater.send_temperature_command(0x06);

  const std::vector<uint8_t> expected{
      0x5A, 0x00, 0x10, 0x22, 0x00, 0x46, 0x01, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0x5B,
  };
  EXPECT_EQ(uart.tx, expected);
}

TEST(MillPanelHeaterGen2Test, TemperatureControlWaitsForStatusConfirmation) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);
  heater.target_temperature = 10.0f;
  heater.current_temperature = 21.0f;
  heater.mode = climate::CLIMATE_MODE_HEAT;
  heater.action = climate::CLIMATE_ACTION_IDLE;

  auto call = heater.make_call();
  call.set_target_temperature(11.0f);
  heater.control(call);

  const std::vector<uint8_t> expected{
      0x5A, 0x00, 0x10, 0x22, 0x00, 0x46, 0x01, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x84, 0x5B,
  };
  EXPECT_EQ(uart.tx, expected);
  EXPECT_FLOAT_EQ(heater.target_temperature, 10.0f);
  EXPECT_FLOAT_EQ(heater.current_temperature, 21.0f);
  EXPECT_EQ(heater.mode, climate::CLIMATE_MODE_HEAT);
  EXPECT_EQ(heater.action, climate::CLIMATE_ACTION_IDLE);
}

TEST(MillPanelHeaterGen2Test, TemperatureControlAcceptsSupportedRangeEndpoints) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);

  auto minimum_call = heater.make_call();
  minimum_call.set_target_temperature(5.0f);
  heater.control(minimum_call);

  auto maximum_call = heater.make_call();
  maximum_call.set_target_temperature(35.0f);
  heater.control(maximum_call);

  const std::vector<uint8_t> expected{
      0x5A, 0x00, 0x10, 0x22, 0x00, 0x46, 0x01, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x5B,
      0x5A, 0x00, 0x10, 0x22, 0x00, 0x46, 0x01, 0x00, 0x23, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9C, 0x5B,
  };
  EXPECT_EQ(uart.tx, expected);
}

TEST(MillPanelHeaterGen2Test, TemperatureControlRoundsToWholeDegreeProtocolStep) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);

  auto call = heater.make_call();
  call.set_target_temperature(11.6f);
  heater.control(call);

  const std::vector<uint8_t> expected{
      0x5A, 0x00, 0x10, 0x22, 0x00, 0x46, 0x01, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x85, 0x5B,
  };
  EXPECT_EQ(uart.tx, expected);
}

TEST(MillPanelHeaterGen2Test, TemperatureControlRejectsValuesOutsideSupportedRange) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);

  auto below_minimum_call = heater.make_call();
  below_minimum_call.set_target_temperature(4.0f);
  heater.control(below_minimum_call);

  auto above_maximum_call = heater.make_call();
  above_maximum_call.set_target_temperature(36.0f);
  heater.control(above_maximum_call);

  EXPECT_TRUE(uart.tx.empty());
}

TEST(MillPanelHeaterGen2Test, PowerOffControlWaitsForStatusConfirmation) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);
  heater.target_temperature = 11.0f;
  heater.current_temperature = 21.0f;
  heater.mode = climate::CLIMATE_MODE_HEAT;
  heater.action = climate::CLIMATE_ACTION_IDLE;

  auto call = heater.make_call();
  call.set_mode(climate::CLIMATE_MODE_OFF);
  heater.control(call);

  const std::vector<uint8_t> expected{
      0x5A, 0x00, 0x10, 0x06, 0x00, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5D, 0x5B,
  };
  EXPECT_EQ(uart.tx, expected);
  EXPECT_FLOAT_EQ(heater.target_temperature, 11.0f);
  EXPECT_FLOAT_EQ(heater.current_temperature, 21.0f);
  EXPECT_EQ(heater.mode, climate::CLIMATE_MODE_HEAT);
  EXPECT_EQ(heater.action, climate::CLIMATE_ACTION_IDLE);
}

TEST(MillPanelHeaterGen2Test, PowerOnControlWaitsForStatusConfirmation) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);
  heater.target_temperature = 11.0f;
  heater.current_temperature = 21.0f;
  heater.mode = climate::CLIMATE_MODE_OFF;
  heater.action = climate::CLIMATE_ACTION_OFF;

  auto call = heater.make_call();
  call.set_mode(climate::CLIMATE_MODE_HEAT);
  heater.control(call);

  const std::vector<uint8_t> expected{
      0x5A, 0x00, 0x10, 0x06, 0x00, 0x47, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5E, 0x5B,
  };
  EXPECT_EQ(uart.tx, expected);
  EXPECT_FLOAT_EQ(heater.target_temperature, 11.0f);
  EXPECT_FLOAT_EQ(heater.current_temperature, 21.0f);
  EXPECT_EQ(heater.mode, climate::CLIMATE_MODE_OFF);
  EXPECT_EQ(heater.action, climate::CLIMATE_ACTION_OFF);
}

TEST(MillPanelHeaterGen2Test, CombinedControlSendsModeBeforeTemperatureAndWaitsForStatusConfirmation) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);
  heater.target_temperature = 11.0f;
  heater.current_temperature = 21.0f;
  heater.mode = climate::CLIMATE_MODE_OFF;
  heater.action = climate::CLIMATE_ACTION_OFF;

  auto call = heater.make_call();
  call.set_mode(climate::CLIMATE_MODE_HEAT);
  call.set_target_temperature(12.0f);
  heater.control(call);

  const std::vector<uint8_t> expected{
      0x5A, 0x00, 0x10, 0x06, 0x00, 0x47, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5E, 0x5B,
      0x5A, 0x00, 0x10, 0x22, 0x00, 0x46, 0x01, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x85, 0x5B,
  };
  EXPECT_EQ(uart.tx, expected);
  EXPECT_FLOAT_EQ(heater.target_temperature, 11.0f);
  EXPECT_FLOAT_EQ(heater.current_temperature, 21.0f);
  EXPECT_EQ(heater.mode, climate::CLIMATE_MODE_OFF);
  EXPECT_EQ(heater.action, climate::CLIMATE_ACTION_OFF);
}

TEST(MillPanelHeaterGen2Test, UpdatesStateFromStatusFrame) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);
  heater.target_temperature = 22.0f;
  uart.rx = {
      0x5A, 0x00, 0x11, 0x00, 0x00, 0xC9, 0x00, 0x05, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xF4, 0x5B,
  };

  while (uart.available() != 0) {
    heater.loop();
  }

  EXPECT_FLOAT_EQ(heater.target_temperature, 5.0f);
  EXPECT_FLOAT_EQ(heater.current_temperature, 20.0f);
  EXPECT_EQ(heater.mode, climate::CLIMATE_MODE_HEAT);
  EXPECT_EQ(heater.action, climate::CLIMATE_ACTION_IDLE);
}

TEST(MillPanelHeaterGen2Test, AcceptsZeroCurrentTemperature) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);
  heater.current_temperature = 20.0f;
  uart.rx = {
      0x5A, 0x00, 0x11, 0x00, 0x00, 0xC9, 0x00, 0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x5B,
  };

  while (uart.available() != 0) {
    heater.loop();
  }

  EXPECT_FLOAT_EQ(heater.current_temperature, 0.0f);
}

TEST(MillPanelHeaterGen2Test, ParsesTenDegreeTargetAsData) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);
  heater.target_temperature = 22.0f;
  uart.rx = {
      0x5A, 0x00, 0x11, 0x00, 0x00, 0xC9, 0x00, 0x0A, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xF9, 0x5B,
  };

  while (uart.available() != 0) {
    heater.loop();
  }

  EXPECT_FLOAT_EQ(heater.target_temperature, 10.0f);
  EXPECT_FLOAT_EQ(heater.current_temperature, 20.0f);
  EXPECT_EQ(heater.mode, climate::CLIMATE_MODE_HEAT);
  EXPECT_EQ(heater.action, climate::CLIMATE_ACTION_IDLE);
}

TEST(MillPanelHeaterGen2Test, AcceptsMaximumTargetTemperatureFromStatusFrame) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);
  heater.target_temperature = 22.0f;
  uart.rx = {
      0x5A, 0x00, 0x11, 0x00, 0x00, 0xC9, 0x00, 0x23, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x12, 0x5B,
  };

  while (uart.available() != 0) {
    heater.loop();
  }

  EXPECT_FLOAT_EQ(heater.target_temperature, 35.0f);
  EXPECT_FLOAT_EQ(heater.current_temperature, 20.0f);
}

TEST(MillPanelHeaterGen2Test, RejectsTargetTemperatureOutsideSupportedRange) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);
  heater.target_temperature = 22.0f;
  heater.current_temperature = 21.0f;
  uart.rx = {
      0x5A, 0x00, 0x11, 0x00, 0x00, 0xC9, 0x00, 0x04, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xF3, 0x5B,
      0x5A, 0x00, 0x11, 0x00, 0x00, 0xC9, 0x00, 0x24, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x13, 0x5B,
  };

  while (uart.available() != 0) {
    heater.loop();
  }

  EXPECT_FLOAT_EQ(heater.target_temperature, 22.0f);
  EXPECT_FLOAT_EQ(heater.current_temperature, 21.0f);
}

TEST(MillPanelHeaterGen2Test, RejectsUnsupportedActionValue) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);
  heater.target_temperature = 22.0f;
  heater.current_temperature = 21.0f;
  heater.mode = climate::CLIMATE_MODE_OFF;
  heater.action = climate::CLIMATE_ACTION_OFF;
  uart.rx = {
      0x5A, 0x00, 0x11, 0x00, 0x00, 0xC9, 0x00, 0x0A, 0x14, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0xFB, 0x5B,
  };

  while (uart.available() != 0) {
    heater.loop();
  }

  EXPECT_FLOAT_EQ(heater.target_temperature, 22.0f);
  EXPECT_FLOAT_EQ(heater.current_temperature, 21.0f);
  EXPECT_EQ(heater.mode, climate::CLIMATE_MODE_OFF);
  EXPECT_EQ(heater.action, climate::CLIMATE_ACTION_OFF);
}

TEST(MillPanelHeaterGen2Test, AcceptsHeatingActionValue) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);
  heater.mode = climate::CLIMATE_MODE_OFF;
  heater.action = climate::CLIMATE_ACTION_OFF;
  uart.rx = {
      0x5A, 0x00, 0x11, 0x00, 0x00, 0xC9, 0x00, 0x0A, 0x14, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0xFA, 0x5B,
  };

  while (uart.available() != 0) {
    heater.loop();
  }

  EXPECT_EQ(heater.mode, climate::CLIMATE_MODE_HEAT);
  EXPECT_EQ(heater.action, climate::CLIMATE_ACTION_HEATING);
}

TEST(MillPanelHeaterGen2Test, PublishesPowerForEveryStatusFrame) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  sensor::Sensor power_sensor;
  std::vector<float> published_power;
  heater.set_uart_parent(&uart);
  heater.set_power_sensor(&power_sensor);
  heater.set_rated_power(900.0f);
  power_sensor.add_on_state_callback([&published_power](float value) { published_power.push_back(value); });
  uart.rx = {
      0x5A, 0x00, 0x11, 0x00, 0x00, 0xC9, 0x00, 0x0A, 0x14, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0xFA, 0x5B,
      0x5A, 0x00, 0x11, 0x00, 0x00, 0xC9, 0x00, 0x0A, 0x14, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0xFA, 0x5B,
  };

  while (uart.available() != 0) {
    heater.loop();
  }

  EXPECT_THAT(published_power, ::testing::ElementsAre(900.0f, 900.0f));
}

TEST(MillPanelHeaterGen2Test, RejectsInvalidChecksum) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);
  heater.target_temperature = 22.0f;
  heater.current_temperature = 21.0f;
  uart.rx = {
      0x5A, 0x00, 0x11, 0x00, 0x00, 0xC9, 0x00, 0x05, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xF5, 0x5B,
  };

  while (uart.available() != 0) {
    heater.loop();
  }

  EXPECT_FLOAT_EQ(heater.target_temperature, 22.0f);
  EXPECT_FLOAT_EQ(heater.current_temperature, 21.0f);
}

TEST(MillPanelHeaterGen2Test, RejectsLineFeedAsFrameTerminator) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);
  heater.target_temperature = 22.0f;
  heater.current_temperature = 21.0f;
  uart.rx = {
      0x5A, 0x00, 0x11, 0x00, 0x00, 0xC9, 0x00, 0x05, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xF4, 0x0A,
  };

  while (uart.available() != 0) {
    heater.loop();
  }

  EXPECT_FLOAT_EQ(heater.target_temperature, 22.0f);
  EXPECT_FLOAT_EQ(heater.current_temperature, 21.0f);
}

TEST(MillPanelHeaterGen2Test, RecoversFromIncompleteWifiButtonSequence) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);
  heater.target_temperature = 22.0f;
  heater.current_temperature = 21.0f;
  uart.rx = {0x5A, 0x00, 0x0A};

  while (uart.available() != 0) {
    heater.loop();
  }

  delay(101);  // NOLINT
  const std::vector<uint8_t> status_frame{
      0x5A, 0x00, 0x11, 0x00, 0x00, 0xC9, 0x00, 0x05, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xF4, 0x5B,
  };
  uart.rx.insert(uart.rx.end(), status_frame.begin(), status_frame.end());

  while (uart.available() != 0) {
    heater.loop();
  }

  EXPECT_FLOAT_EQ(heater.target_temperature, 5.0f);
  EXPECT_FLOAT_EQ(heater.current_temperature, 20.0f);
}

TEST(MillPanelHeaterGen2Test, KeepsOffModeAndActionConsistent) {
  MockUARTComponent uart;
  TestableMillPanelHeaterGen2 heater;
  heater.set_uart_parent(&uart);
  heater.mode = climate::CLIMATE_MODE_HEAT;
  heater.action = climate::CLIMATE_ACTION_HEATING;
  uart.rx = {
      0x5A, 0x00, 0x11, 0x00, 0x00, 0xC9, 0x00, 0x05, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF3, 0x5B,
  };

  while (uart.available() != 0) {
    heater.loop();
  }

  EXPECT_EQ(heater.mode, climate::CLIMATE_MODE_OFF);
  EXPECT_EQ(heater.action, climate::CLIMATE_ACTION_OFF);
}

}  // namespace esphome::mill_panelheater_gen2::testing
