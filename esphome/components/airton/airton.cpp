#include "airton.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::airton {

ESPHOME_LOG_TAG(TAG, "airton.climate");

// Distinct from the climate's own RESTORE_STATE_VERSION, which shares the entity's preference key
static constexpr uint32_t RESTORE_SETTINGS_VERSION = 0x2E86A1F3UL;

// From IRutils.h of IRremoteESP8266 library
static uint8_t checksum(const uint8_t *frame) {
  return static_cast<uint8_t>(0x7F - sum_bytes(frame, AIRTON_STATE_FRAME_SIZE - 1)) ^ 0x2C;
}

void AirtonClimate::setup() {
  climate_ir::ClimateIR::setup();
  // The flap position is the one source of truth for the swing mode the climate state restored
  this->settings_pref_ = this->make_entity_preference<AirtonSettings>(RESTORE_SETTINGS_VERSION);
  this->settings_pref_.load(&this->settings_);
  this->sync_swing_mode_();
  this->publish_settings_();
  this->publish_state();
}

void AirtonClimate::publish_settings_() {
#ifdef USE_SWITCH
  if (this->sleep_mode_switch_ != nullptr)
    this->sleep_mode_switch_->publish_state(this->settings_.sleep_state);
  if (this->display_switch_ != nullptr)
    this->display_switch_->publish_state(this->settings_.display_state);
#endif
#ifdef USE_SELECT
  if (this->vertical_direction_select_ != nullptr)
    this->vertical_direction_select_->publish_state(static_cast<size_t>(this->settings_.vertical_direction));
#endif
}

void AirtonClimate::save_settings_() { this->settings_pref_.save(&this->settings_); }

void AirtonClimate::sync_swing_mode_() {
  this->swing_mode = this->settings_.vertical_direction == VerticalDirection::VERTICAL_DIRECTION_SWING
                         ? climate::CLIMATE_SWING_VERTICAL
                         : climate::CLIMATE_SWING_OFF;
}

// The unit has no sleep mode while it only fans or picks the mode itself
bool AirtonClimate::sleep_mode_allowed_() const {
  return this->mode != climate::CLIMATE_MODE_FAN_ONLY && this->mode != climate::CLIMATE_MODE_HEAT_COOL;
}

// Each setter stores and saves before it publishes: an automation on the entity runs inside
// publish_state() and may set the value again, which must not be undone afterwards
bool AirtonClimate::set_sleep_mode_state(bool state) {
  state = state && this->sleep_mode_allowed_();
  const bool changed = state != this->settings_.sleep_state;
  if (changed) {
    this->settings_.sleep_state = state;
    this->save_settings_();
  }
#ifdef USE_SWITCH
  if (this->sleep_mode_switch_ != nullptr)
    this->sleep_mode_switch_->publish_state(state);
#endif
  return changed;
}

bool AirtonClimate::set_display_state(bool state) {
  const bool changed = state != this->settings_.display_state;
  if (changed) {
    this->settings_.display_state = state;
    this->save_settings_();
  }
#ifdef USE_SWITCH
  if (this->display_switch_ != nullptr)
    this->display_switch_->publish_state(state);
#endif
  return changed;
}

bool AirtonClimate::set_vertical_direction_state(VerticalDirection direction) {
  const bool changed = direction != this->settings_.vertical_direction;
  if (changed) {
    this->settings_.vertical_direction = direction;
    this->sync_swing_mode_();
    this->save_settings_();
  }
#ifdef USE_SELECT
  if (this->vertical_direction_select_ != nullptr)
    this->vertical_direction_select_->publish_state(static_cast<size_t>(direction));
#endif
  return changed;
}

void AirtonClimate::send_settings() {
  this->transmit_state();
  this->publish_state();
}

void AirtonClimate::control(const climate::ClimateCall &call) {
  // ClimateIR::control() sends and publishes the whole state afterwards
  // Only a change of swing mode moves the flap, so a repeated swing-off keeps a fixed position
  if (auto swing_mode = call.get_swing_mode(); swing_mode.has_value() && *swing_mode != this->swing_mode) {
    this->set_vertical_direction_state(*swing_mode == climate::CLIMATE_SWING_VERTICAL
                                           ? VerticalDirection::VERTICAL_DIRECTION_SWING
                                           : VerticalDirection::VERTICAL_DIRECTION_OFF);
  }
  ClimateIR::control(call);
}

void AirtonClimate::transmit_state() {
  // Entering fan-only or automatic mode drops sleep mode, as the remote does
  if (!this->sleep_mode_allowed_())
    this->set_sleep_mode_state(false);
  // Sampled valid state
  // Power: On, Mode: 2 (Dry), Fan: 1 (Quiet), Temp: 20C, Swing(V): On, Econo: Off, Turbo: Off, Light: On, Health: On,
  // Sleep: Off. 0x74C461041A11D3
  // The turbo bit above the fan speed stays clear, as the remote sends it
  uint8_t remote_state[AIRTON_STATE_FRAME_SIZE] = {
      AIRTON_HEADER_BYTE0,
      AIRTON_HEADER_BYTE1,
      static_cast<uint8_t>(this->operation_mode_() | (this->fan_speed_() << 4)),
      this->temperature_(),
      static_cast<uint8_t>(this->settings_.vertical_direction),
      this->operation_settings_(),
      0,
  };
  remote_state[6] = checksum(remote_state);

  ESP_LOGV(TAG, "Sending: %02X %02X %02X %02X %02X %02X %02X", remote_state[6], remote_state[5], remote_state[4],
           remote_state[3], remote_state[2], remote_state[1], remote_state[0]);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(AIRTON_IR_FREQUENCY);
  data->mark(AIRTON_HEADER_MARK);
  data->space(AIRTON_HEADER_SPACE);
  data->item_bytes_lsb(remote_state, AIRTON_STATE_FRAME_SIZE, AIRTON_BIT_MARK, AIRTON_ONE_SPACE, AIRTON_ZERO_SPACE);

  data->mark(AIRTON_BIT_MARK);
  data->space(AIRTON_MESSAGE_SPACE);

  transmit.perform();
}

uint8_t AirtonClimate::operation_mode_() {
  uint8_t operating_mode = 0b1000;  // First bit is for power state
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      operating_mode |= AIRTON_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      operating_mode |= AIRTON_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_HEAT:
      operating_mode |= AIRTON_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      operating_mode |= AIRTON_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      operating_mode |= AIRTON_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      operating_mode = 0b0111 & this->previous_mode_;  // Set previous mode with power state bit off
  }
  this->previous_mode_ = operating_mode;
  return operating_mode;
}

uint8_t AirtonClimate::fan_speed_() {
  uint8_t fan_speed;
  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_LOW:
      fan_speed = AIRTON_FAN_1;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed = AIRTON_FAN_3;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed = AIRTON_FAN_5;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      fan_speed = AIRTON_FAN_AUTO;
  }
  return fan_speed;
}

uint8_t AirtonClimate::temperature_() {
  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT_COOL:
      return AIRTON_AUTO_MODE_TEMP - AIRTON_TEMP_MIN;
    default:
      return static_cast<uint8_t>(roundf(clamp<float>(this->target_temperature, AIRTON_TEMP_MIN, AIRTON_TEMP_MAX))) -
             AIRTON_TEMP_MIN;
  }
}

// The bits of this packet's byte have the following meanings (from MSB to LSB)
// Light, Health, Unknown, HeatOn, Unknown, NotAutoOn, Sleep, Econo
uint8_t AirtonClimate::operation_settings_() {
  uint8_t settings = 0;
  if (this->mode == climate::CLIMATE_MODE_HEAT) {  // Set heating bit if on the corresponding mode
    settings |= (1 << 4);
  }
  if (this->settings_.display_state) {
    settings |= (1 << 7);
  }
  if (this->settings_.sleep_state) {
    settings |= (1 << 1);
  }
  settings |= 0b01000000;  // Health, as the remote always sends it
  if (this->mode != climate::CLIMATE_MODE_HEAT_COOL) {
    settings |= 0b00000100;  // NotAutoOn: clear only while the unit picks the mode itself
  }
  return settings;
}

void AirtonClimate::parse_state_frame_(const uint8_t *frame) {
  uint8_t mode = frame[2];
  // A later "off" repeats the mode the remote last sent, as the remote itself does
  this->previous_mode_ = mode & 0b00001111;
  if (mode & 0b00001000) {  // The power bit
    switch (mode & 0b00000111) {
      case AIRTON_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case AIRTON_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case AIRTON_MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case AIRTON_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      case AIRTON_MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
    }
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
  }
  uint8_t fan_mode = (frame[2] & 0b01110000) >> 4;

  switch (fan_mode) {
    case AIRTON_FAN_1:
    case AIRTON_FAN_2:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case AIRTON_FAN_3:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case AIRTON_FAN_4:
    case AIRTON_FAN_5:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case AIRTON_FAN_AUTO:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  this->target_temperature = (frame[3] & 0b00001111) + AIRTON_TEMP_MIN;

  const uint8_t direction = frame[4] & 0b00001111;
  if (direction <= static_cast<uint8_t>(VerticalDirection::VERTICAL_DIRECTION_DOWN)) {
    this->set_vertical_direction_state(static_cast<VerticalDirection>(direction));
  } else {
    ESP_LOGV(TAG, "Unknown vertical direction %u", direction);
  }
  this->set_display_state((frame[5] & 0b10000000) != 0);
  this->set_sleep_mode_state((frame[5] & 0b00000010) != 0);

  this->publish_state();
}

bool AirtonClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t remote_state[AIRTON_STATE_FRAME_SIZE];
  if (!data.expect_item(AIRTON_HEADER_MARK, AIRTON_HEADER_SPACE)) {
    ESP_LOGV(TAG, "Wrong header encoding detected!");
    return false;
  }
  if (!data.expect_bytes_lsb(remote_state, AIRTON_STATE_FRAME_SIZE, AIRTON_BIT_MARK, AIRTON_ONE_SPACE,
                             AIRTON_ZERO_SPACE))
    return false;

  if (remote_state[0] != AIRTON_HEADER_BYTE0 || remote_state[1] != AIRTON_HEADER_BYTE1) {
    ESP_LOGV(TAG, "Wrong header contents: %02X %02X", remote_state[1], remote_state[0]);
    return false;
  }
  const uint8_t expected = checksum(remote_state);
  if (remote_state[AIRTON_STATE_FRAME_SIZE - 1] != expected) {
    ESP_LOGV(TAG, "Checksum error: calculated %02X, received %02X", expected,
             remote_state[AIRTON_STATE_FRAME_SIZE - 1]);
    return false;
  }
  ESP_LOGV(TAG, "Received: %02X %02X %02X %02X %02X %02X %02X", remote_state[6], remote_state[5], remote_state[4],
           remote_state[3], remote_state[2], remote_state[1], remote_state[0]);
  this->parse_state_frame_(remote_state);
  return true;
}

}  // namespace esphome::airton
