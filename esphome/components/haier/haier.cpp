#include <cmath>
#include "haier.h"
#include "esphome/core/macros.h"

namespace esphome {
namespace haier {

static const char *const TAG = "haier";

static const uint8_t TEMPERATURE = 13;
static const uint8_t HUMIDITY = 15;

static const uint8_t MODE = 23;

static const uint8_t FAN_SPEED = 25;

static const uint8_t SWING = 27;

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

void HaierClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "Haier:");
  ESP_LOGCONFIG(TAG, "  Update interval: %u", this->get_update_interval());
  this->dump_traits_(TAG);
  this->check_uart_settings(9600);
}

void HaierClimate::loop() {
  if (this->available() >= sizeof(this->data_)) {
    this->read_array(this->data_, sizeof(this->data_));
    if (this->data_[0] != 255 || this->data_[1] != 255)
      return;

    read_state_(this->data_, sizeof(this->data_));
  }
}

void HaierClimate::update() {
  this->write_array(POLL_REQ, sizeof(POLL_REQ));
  dump_message_("Poll sent", POLL_REQ, sizeof(POLL_REQ));
}

climate::ClimateTraits HaierClimate::traits() {
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

  traits.set_supported_swing_modes(this->supported_swing_modes_);
  traits.set_supports_current_temperature(true);
  traits.set_supports_two_point_target_temperature(false);

  traits.add_supported_preset(climate::CLIMATE_PRESET_NONE);
  traits.add_supported_preset(climate::CLIMATE_PRESET_COMFORT);

  return traits;
}

void HaierClimate::read_state_(const uint8_t *data, uint8_t size) {
  dump_message_("Received state", data, size);

  uint8_t check = data[CRC];

  uint8_t crc = get_checksum_(data, size);

  if (check != crc) {
    ESP_LOGW(TAG, "Invalid checksum");
    return;
  }

  this->current_temperature = data[TEMPERATURE];

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
    default:  // other modes are unsupported
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

  if (data[POWER] & COMFORT_PRESET_MASK) {
    this->preset = climate::CLIMATE_PRESET_COMFORT;
  } else {
    this->preset = climate::CLIMATE_PRESET_NONE;
  }

  if ((data[POWER] & POWER_MASK) == 0) {
    this->mode = climate::CLIMATE_MODE_OFF;
  }

  this->publish_state();
}

void HaierClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    switch (call.get_mode().value()) {
      case climate::CLIMATE_MODE_OFF:
        send_data_(OFF_REQ, sizeof(OFF_REQ));
        break;

      case climate::CLIMATE_MODE_HEAT_COOL:
      case climate::CLIMATE_MODE_AUTO:
        data_[POWER] |= POWER_MASK;
        data_[MODE] = MODE_SMART;
        break;
      case climate::CLIMATE_MODE_HEAT:
        data_[POWER] |= POWER_MASK;
        data_[MODE] = MODE_HEAT;
        break;
      case climate::CLIMATE_MODE_COOL:
        data_[POWER] |= POWER_MASK;
        data_[MODE] = MODE_COOL;
        break;

      case climate::CLIMATE_MODE_FAN_ONLY:
        data_[POWER] |= POWER_MASK;
        data_[MODE] = MODE_ONLY_FAN;
        break;

      case climate::CLIMATE_MODE_DRY:
        data_[POWER] |= POWER_MASK;
        data_[MODE] = MODE_DRY;
        break;
    }
  }

  if (call.get_preset().has_value()) {
    if (call.get_preset().value() == climate::CLIMATE_PRESET_COMFORT) {
      data_[POWER] |= COMFORT_PRESET_MASK;
    } else {
      data_[POWER] &= ~COMFORT_PRESET_MASK;
    }
  }

  if (call.get_target_temperature().has_value()) {
    float target = call.get_target_temperature().value() - MIN_VALID_TEMPERATURE;

    data_[SET_TEMPERATURE] = (uint8_t) target;

    if ((int) target == std::lroundf(target)) {
      data_[POWER] &= ~DECIMAL_MASK;
    } else {
      data_[POWER] |= DECIMAL_MASK;
    }
  }

  if (call.get_fan_mode().has_value()) {
    switch (call.get_fan_mode().value()) {
      case climate::CLIMATE_FAN_AUTO:
        data_[FAN_SPEED] = FAN_AUTO;
        break;
      case climate::CLIMATE_FAN_LOW:
        data_[FAN_SPEED] = FAN_MIN;
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        data_[FAN_SPEED] = FAN_MIDDLE;
        break;
      case climate::CLIMATE_FAN_HIGH:
        data_[FAN_SPEED] = FAN_MAX;
        break;

      default:  // other modes are unsupported
        break;
    }
  }

  if (call.get_swing_mode().has_value()) {
    switch (call.get_swing_mode().value()) {
      case climate::CLIMATE_SWING_OFF:
        data_[SWING] = SWING_OFF;
        break;
      case climate::CLIMATE_SWING_VERTICAL:
        data_[SWING] = SWING_VERTICAL;
        break;
      case climate::CLIMATE_SWING_HORIZONTAL:
        data_[SWING] = SWING_HORIZONTAL;
        break;
      case climate::CLIMATE_SWING_BOTH:
        data_[SWING] = SWING_BOTH;
        break;
    }
  }

  // Parts of the message that must have specific values for "send" command.
  // The meaning of those values is unknown at the moment.
  data_[9] = 1;
  data_[10] = 77;
  data_[11] = 95;
  data_[17] = 0;

  // Compute checksum
  uint8_t crc = get_checksum_(data_, sizeof(data_));
  data_[CRC] = crc;

  send_data_(data_, sizeof(data_));
}

void HaierClimate::send_data_(const uint8_t *message, uint8_t size) {
  this->write_array(message, size);

  dump_message_("Sent message", message, size);
}

void HaierClimate::dump_message_(const char *title, const uint8_t *message, uint8_t size) {
  ESP_LOGV(TAG, "%s:", title);
  for (int i = 0; i < size; i++) {
    ESP_LOGV(TAG, "  byte %02d - %d", i, message[i]);
  }
}

uint8_t HaierClimate::get_checksum_(const uint8_t *message, size_t size) {
  uint8_t position = size - 1;
  uint8_t crc = 0;

  for (int i = 2; i < position; i++)
    crc += message[i];

  return crc;
}

}  // namespace haier
}  // namespace esphome
