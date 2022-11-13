#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace haier {

static const uint8_t TEMPERATURE = 13;
static const uint8_t HUMIDITY = 15;
static const uint8_t COMMAND = 17;

static const uint8_t MODE = 23;
enum Mode : uint8_t { MODE_SMART = 0, MODE_COOL = 1, MODE_HEAT = 2, MODE_ONLY_FAN = 3, MODE_DRY = 4 };

static const uint8_t FAN_SPEED = 25;
enum FanSpeed : uint8_t { FAN_MAX = 0, FAN_MIDDLE = 1, FAN_MIN = 2, FAN_AUTO = 3 };

static const uint8_t SWING = 27;
enum SwingMode : uint8_t { SWING_OFF = 0, SWING_VERTICAL = 1, SWING_HORIZONTAL = 2, SWING_BOTH = 3 };

static const uint8_t POWER = 29;
static const uint8_t POWER_MASK = 1;

static const uint8_t SET_TEMPERATURE = 35;
static const uint8_t DECIMAL_MASK = (1 << 5);

static const uint8_t CRC = 36;

static const uint8_t COMFORT_PRESET_MASK = (1 << 3);

static const uint8_t MIN_VALID_TEMPERATURE = 16;
static const uint8_t MAX_VALID_TEMPERATURE = 50;
static const float TEMPERATURE_STEP = 0.5f;

static const uint8_t POLL_REQ[13] = {255, 255, 10, 0, 0, 0, 0, 0, 1, 1, 77, 1, 90};
static const uint8_t OFF_REQ[13] = {255, 255, 10, 0, 0, 0, 0, 0, 1, 1, 77, 3, 92};

class HaierClimate : public climate::Climate, public uart::UARTDevice, public PollingComponent {
 private:
  uint8_t data_[37];
  std::set<climate::ClimateSwingMode> supported_swing_modes_{};

 public:
  HaierClimate() : PollingComponent(5 * 1000) {}

  void setup() override {}
  void loop() override;
  void update() override;
  void dump_config() override;
  void control(const climate::ClimateCall &call) override;
  void set_supported_swing_modes(const std::set<climate::ClimateSwingMode> &modes) {
    this->supported_swing_modes_ = modes;
  }

 protected:
  climate::ClimateTraits traits() override;
  void read_state_(const uint8_t *data, uint8_t size);
  void send_data_(const uint8_t *message, uint8_t size);
  void dump_message_(const char *title, const uint8_t *message, uint8_t size);
  uint8_t get_checksum_(const uint8_t *message, size_t size);
};

}  // namespace haier
}  // namespace esphome
