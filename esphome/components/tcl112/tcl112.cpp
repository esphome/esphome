#include "tcl112.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tcl112 {

static const char *TAG = "tcl112.climate";

const uint16_t TCL112_STATE_LENGTH = 14;
const uint16_t TCL112_BITS = TCL112_STATE_LENGTH * 8;

const uint8_t TCL112_HEAT = 1;
const uint8_t TCL112_DRY = 2;
const uint8_t TCL112_COOL = 3;
const uint8_t TCL112_FAN = 7;
const uint8_t TCL112_AUTO = 8;

const uint8_t TCL112_POWER_MASK = 0x04;

const uint8_t TCL112_HALF_DEGREE = 0b00100000;
const float TCL112_TEMP_MAX = 31.0;
const float TCL112_TEMP_MIN = 16.0;

const uint16_t TCL112_HEADER_MARK = 3000;
const uint16_t TCL112_HEADER_SPACE = 1650;
const uint16_t TCL112_BIT_MARK = 500;
const uint16_t TCL112_ONE_SPACE = 1050;
const uint16_t TCL112_ZERO_SPACE = 325;
const uint32_t TCL112_GAP = TCL112_HEADER_SPACE;

climate::ClimateTraits Tcl112Climate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(this->sensor_ != nullptr);
  traits.set_supports_auto_mode(true);
  traits.set_supports_cool_mode(this->supports_cool_);
  traits.set_supports_heat_mode(this->supports_heat_);
  traits.set_supports_two_point_target_temperature(false);
  traits.set_supports_away(false);
  traits.set_visual_min_temperature(TCL112_TEMP_MIN);
  traits.set_visual_max_temperature(TCL112_TEMP_MAX);
  traits.set_visual_temperature_step(.5f);
  return traits;
}

void Tcl112Climate::setup() {
  if (this->sensor_) {
    this->sensor_->add_on_state_callback([this](float state) {
      this->current_temperature = state;
      // current temperature changed, publish state
      this->publish_state();
    });
    this->current_temperature = this->sensor_->state;
  } else
    this->current_temperature = NAN;
  // restore set points
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    // restore from defaults
    this->mode = climate::CLIMATE_MODE_OFF;
    this->target_temperature = 24;
  }
}

void Tcl112Climate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();

  this->transmit_state_();
  this->publish_state();
}

void Tcl112Climate::transmit_state_() {
  uint8_t remote_state[TCL112_STATE_LENGTH] = {0};

  // A known good state. (On, Cool, 24C)
  remote_state[0] = 0x23;
  remote_state[1] = 0xCB;
  remote_state[2] = 0x26;
  remote_state[3] = 0x01;
  remote_state[5] = 0x24;
  remote_state[6] = 0x03;
  remote_state[7] = 0x07;
  remote_state[8] = 0x40;

  // Set mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_AUTO:
      remote_state[6] &= 0xF0;
      remote_state[6] |= TCL112_AUTO;
      break;
    case climate::CLIMATE_MODE_COOL:
      remote_state[6] &= 0xF0;
      remote_state[6] |= TCL112_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state[6] &= 0xF0;
      remote_state[6] |= TCL112_HEAT;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      remote_state[5] &= ~TCL112_POWER_MASK;
      break;
  }

  // Set temperature
  // Make sure we have desired temp in the correct range.
  float safecelsius = std::max(this->target_temperature, TCL112_TEMP_MIN);
  safecelsius = std::min(safecelsius, TCL112_TEMP_MAX);
  // Convert to integer nr. of half degrees.
  auto half_degrees = static_cast<uint8_t>(safecelsius * 2);
  if (half_degrees & 1)                      // Do we have a half degree celsius?
    remote_state[12] |= TCL112_HALF_DEGREE;  // Add 0.5 degrees
  else
    remote_state[12] &= ~TCL112_HALF_DEGREE;  // Clear the half degree.
  remote_state[7] &= 0xF0;                    // Clear temp bits.
  remote_state[7] |= ((uint8_t) TCL112_TEMP_MAX - half_degrees / 2);

  // Calculate & set the checksum for the current internal state of the remote.
  // Stored the checksum value in the last byte.
  for (uint8_t checksum_byte = 0; checksum_byte < TCL112_STATE_LENGTH - 1; checksum_byte++)
    remote_state[TCL112_STATE_LENGTH - 1] += remote_state[checksum_byte];

  ESP_LOGV(TAG, "Sending tcl code: %u", remote_state[7]);

  auto transmit = this->transmitter_->transmit();
  auto data = transmit.get_data();

  data->set_carrier_frequency(38000);

  // Header
  data->mark(TCL112_HEADER_MARK);
  data->space(TCL112_HEADER_SPACE);
  // Data
  for (uint8_t i : remote_state)
    for (uint8_t j = 0; j < 8; j++) {
      data->mark(TCL112_BIT_MARK);
      bool bit = i & (1 << j);
      data->space(bit ? TCL112_ONE_SPACE : TCL112_ZERO_SPACE);
    }
  // Footer
  data->mark(TCL112_BIT_MARK);
  data->space(TCL112_GAP);

  transmit.perform();
}

}  // namespace tcl112
}  // namespace esphome
