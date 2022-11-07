#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace haier {

static const char *const TAG = "haier";

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
  uint8_t data[37];

  SwingMode supported_swing_mode;

 public:
  HaierClimate(SwingMode supported_swing_mode) : PollingComponent(5 * 1000) {
    this->supported_swing_mode = supported_swing_mode;
  }

  void setup() override {}

  void loop() override {
    if (this->available()) {
      if (this->read() != 255)
        return;
      if (this->read() != 255)
        return;

      data[0] = 255;
      data[1] = 255;

      this->read_array(data + 2, sizeof(data) - 2);

      readData();
    }
  }

  void update() override {
    this->write_array(POLL_REQ, sizeof(POLL_REQ));
    ESP_LOGV("Haier", "POLL: %s ", bytesToString(POLL_REQ, sizeof(POLL_REQ)).c_str());
  }

  void dump_config() {
    ESP_LOGCONFIG(TAG, "Haier:");
    ESP_LOGCONFIG(TAG, "  Update interval: %u", this->get_update_interval());
    dump_traits_(TAG);
    this->check_uart_settings(9600);
  }

 protected:
  climate::ClimateTraits traits() override {
    auto traits = climate::ClimateTraits();

    traits.set_visual_min_temperature(MIN_VALID_TEMPERATURE);
    traits.set_visual_max_temperature(MAX_VALID_TEMPERATURE);
    traits.set_visual_temperature_step(TEMPERATURE_STEP);

    traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT_COOL, climate::CLIMATE_MODE_COOL,
                                climate::CLIMATE_MODE_HEAT, climate::CLIMATE_MODE_FAN_ONLY, climate::CLIMATE_MODE_DRY});

    traits.set_supported_fan_modes({
        climate::CLIMATE_FAN_AUTO,
        climate::CLIMATE_FAN_LOW,
        climate::CLIMATE_FAN_MEDIUM,
        climate::CLIMATE_FAN_HIGH,
    });

    switch (supported_swing_mode) {
      case SWING_VERTICAL:
        traits.set_supported_swing_modes({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL});
        break;

      case SWING_HORIZONTAL:
        traits.set_supported_swing_modes({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_HORIZONTAL});
        break;

      case SWING_BOTH:
        traits.set_supported_swing_modes({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                                          climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH});
        break;
      default:
        break;
    }

    traits.set_supports_current_temperature(true);
    traits.set_supports_two_point_target_temperature(false);

    traits.add_supported_preset(climate::CLIMATE_PRESET_NONE);
    traits.add_supported_preset(climate::CLIMATE_PRESET_COMFORT);

    return traits;
  }

 public:
  void readData() {
    ESP_LOGV(TAG, "Readed message: %s ", bytesToString(data, sizeof(data)).c_str());

    uint8_t check = data[CRC];

    uint8_t crc = getChecksum(data, sizeof(data));

    if (check != crc) {
      ESP_LOGW(TAG, "Invalid checksum");  // TODO more info
      return;
    }

    if (MIN_VALID_TEMPERATURE < data[TEMPERATURE] && data[TEMPERATURE] < MAX_VALID_TEMPERATURE) {
      this->current_temperature = data[TEMPERATURE];
    }

    this->target_temperature = data[SET_TEMPERATURE] + MIN_VALID_TEMPERATURE;

    if (data[POWER] & DECIMAL_MASK) {
      this->target_temperature += 0.5f;
    }

    switch (data[MODE]) {
      case MODE_SMART:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      case MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case MODE_ONLY_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      default:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
    }

    switch (data[FAN_SPEED]) {
      case FAN_AUTO:
        this->fan_mode = climate::CLIMATE_FAN_AUTO;
        break;

      case FAN_MIN:
        this->fan_mode = climate::CLIMATE_FAN_LOW;
        break;

      case FAN_MIDDLE:
        this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
        break;

      case FAN_MAX:
        this->fan_mode = climate::CLIMATE_FAN_HIGH;
        break;
    }

    switch (data[SWING]) {
      case SWING_OFF:
        this->swing_mode = climate::CLIMATE_SWING_OFF;
        break;

      case SWING_VERTICAL:
        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
        break;

      case SWING_HORIZONTAL:
        this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
        break;

      case SWING_BOTH:
        this->swing_mode = climate::CLIMATE_SWING_BOTH;
        break;
    }

    if (data[POWER] & COMFORT_PRESET_MASK)
      this->preset = climate::CLIMATE_PRESET_COMFORT;
    else
      this->preset = climate::CLIMATE_PRESET_NONE;

    if ((data[POWER] & POWER_MASK) == 0) {
      this->mode = climate::CLIMATE_MODE_OFF;
    }

    this->publish_state();
  }

  void control(const climate::ClimateCall &call) override {
    if (call.get_mode().has_value()) {
      switch (call.get_mode().value()) {
        case climate::CLIMATE_MODE_OFF:
          sendData(OFF_REQ, sizeof(OFF_REQ));
          break;

        case climate::CLIMATE_MODE_HEAT_COOL:
        case climate::CLIMATE_MODE_AUTO:
          data[POWER] |= POWER_MASK;
          data[MODE] = MODE_SMART;
          break;
        case climate::CLIMATE_MODE_HEAT:
          data[POWER] |= POWER_MASK;
          data[MODE] = MODE_HEAT;
          break;
        case climate::CLIMATE_MODE_COOL:
          data[POWER] |= POWER_MASK;
          data[MODE] = MODE_COOL;
          break;

        case climate::CLIMATE_MODE_FAN_ONLY:
          data[POWER] |= POWER_MASK;
          data[MODE] = MODE_ONLY_FAN;
          break;

        case climate::CLIMATE_MODE_DRY:
          data[POWER] |= POWER_MASK;
          data[MODE] = MODE_DRY;
          break;
      }
    }

    if (call.get_preset().has_value()) {
      if (call.get_preset().value() == climate::CLIMATE_PRESET_COMFORT)
        data[POWER] |= COMFORT_PRESET_MASK;
      else
        data[POWER] &= ~COMFORT_PRESET_MASK;
    }

    if (call.get_target_temperature().has_value()) {
      float target = call.get_target_temperature().value() - MIN_VALID_TEMPERATURE;

      data[SET_TEMPERATURE] = (uint16) target;

      if ((int) target == (int) (target + 0.5))
        data[POWER] &= ~DECIMAL_MASK;
      else
        data[POWER] |= DECIMAL_MASK;
    }

    if (call.get_fan_mode().has_value()) {
      switch (call.get_fan_mode().value()) {
        case climate::CLIMATE_FAN_AUTO:
          data[FAN_SPEED] = FAN_AUTO;
          break;
        case climate::CLIMATE_FAN_LOW:
          data[FAN_SPEED] = FAN_MIN;
          break;
        case climate::CLIMATE_FAN_MEDIUM:
          data[FAN_SPEED] = FAN_MIDDLE;
          break;
        case climate::CLIMATE_FAN_HIGH:
          data[FAN_SPEED] = FAN_MAX;
          break;

        case climate::CLIMATE_FAN_ON:
        case climate::CLIMATE_FAN_OFF:
        case climate::CLIMATE_FAN_MIDDLE:
        case climate::CLIMATE_FAN_FOCUS:
        case climate::CLIMATE_FAN_DIFFUSE:
          break;
      }
    }

    if (call.get_swing_mode().has_value())
      switch (call.get_swing_mode().value()) {
        case climate::CLIMATE_SWING_OFF:
          data[SWING] = SWING_OFF;
          break;
        case climate::CLIMATE_SWING_VERTICAL:
          data[SWING] = SWING_VERTICAL;
          break;
        case climate::CLIMATE_SWING_HORIZONTAL:
          data[SWING] = SWING_HORIZONTAL;
          break;
        case climate::CLIMATE_SWING_BOTH:
          data[SWING] = SWING_BOTH;
          break;
      }

    // Values for "send"
    data[COMMAND] = 0;
    data[9] = 1;
    data[10] = 77;
    data[11] = 95;

    sendData(data, sizeof(data));
  }

  void sendData(const uint8_t *message, uint8_t size) {
    uint8_t crc = getChecksum(message, size);
    this->write_array(message, size - 1);
    this->write(crc);

    ESP_LOGV(TAG, "Sended message: %s ", bytesToString(message, size).c_str());
  }

  String bytesToString(const uint8_t *message, uint8_t size) {
    String raw;

    for (int i = 0; i < size; i++) {
      raw += "\n" + String(i) + "-" + String(message[i]);
    }
    raw.toUpperCase();

    return raw;
  }

  uint8_t getChecksum(const uint8_t *message, size_t size) {
    uint8_t position = size - 1;
    uint8_t crc = 0;

    for (int i = 2; i < position; i++)
      crc += message[i];

    return crc;
  }
};

}  // namespace haier
}  // namespace esphome
