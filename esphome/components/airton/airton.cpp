#include "airton.h"
#include "esphome/core/log.h"

namespace esphome {
namespace airton {

static const char *const TAG = "airton.climate";

void AirtonClimate::set_sleep_mode_state(bool state, bool send_ir = false) {
  if (state != this->settings_.sleep_state) {
    this->settings_.sleep_state = state;
#ifdef USE_SWITCH
    this->sleep_mode_switch_->publish_state(state);
#endif
    this->airton_rtc_.save(&this->settings_);
    if (send_ir)
      this->transmit_state();
  }
}

bool AirtonClimate::get_sleep_mode_state() const { return this->settings_.sleep_state; }

void AirtonClimate::set_display_state(bool state, bool send_ir = false) {
  if (state != this->settings_.display_state) {
    this->settings_.display_state = state;
#ifdef USE_SWITCH
    this->display_switch_->publish_state(state);
#endif
    this->airton_rtc_.save(&this->settings_);
    if (send_ir)
      this->transmit_state();
  }
}

bool AirtonClimate::get_display_state() const { return this->settings_.display_state; }

void AirtonClimate::set_vertical_direction_state(VerticalDirection state) {
  if (state.to_uint8() != this->settings_.vertical_direction_state.to_uint8()) {
    this->settings_.vertical_direction_state = state;
#ifdef USE_SELECT
    this->vertical_direction_select_->publish_state(state.to_string());
#endif
    this->airton_rtc_.save(&this->settings_);
  }
}
void AirtonClimate::set_vertical_direction_state(const std::string &state) {
  if (state != this->settings_.vertical_direction_state.to_string()) {
    VerticalDirection new_state;
    new_state.set_direction(state);
    this->settings_.vertical_direction_state = new_state.get_direction();
#ifdef USE_SELECT
    this->vertical_direction_select_->publish_state(state);
#endif
    this->airton_rtc_.save(&this->settings_);
    // This overloaded function is called only from the select component, upon changing selection
    // Therefore, we transmit the updated state after saving it
    this->transmit_state();
  }
}

VerticalDirection AirtonClimate::get_vertical_direction_state() const {
  return this->settings_.vertical_direction_state;
}

#ifdef USE_SWITCH
void AirtonClimate::set_sleep_mode_switch(switch_::Switch *sw) {
  this->sleep_mode_switch_ = sw;
  if (this->sleep_mode_switch_ != nullptr) {
    this->sleep_mode_switch_->publish_state(this->get_sleep_mode_state());
  }
}
void AirtonClimate::set_display_switch(switch_::Switch *sw) {
  this->display_switch_ = sw;
  if (this->display_switch_ != nullptr) {
    this->display_switch_->publish_state(this->get_display_state());
  }
}
#endif  // USE_SWITCH

#ifdef USE_SELECT
void AirtonClimate::set_vertical_direction_select(select::Select *sel) {
  this->vertical_direction_select_ = sel;
  if (this->vertical_direction_select_ != nullptr) {
    this->vertical_direction_select_->publish_state(this->get_vertical_direction_state().to_string());
  }
}
#endif  // USE_SELECT

uint8_t AirtonClimate::get_previous_mode_() { return previous_mode_; }

void AirtonClimate::set_previous_mode_(uint8_t mode) { previous_mode_ = mode; }

void AirtonClimate::transmit_state() {
  // Sampled valid state
  // Power: On, Mode: 2 (Dry), Fan: 1 (Quiet), Temp: 20C, Swing(V): On, Econo: Off, Turbo: Off, Light: On, Health: On,
  // Sleep: Off. 0x74C461041A11D3
  uint8_t remote_state[AIRTON_STATE_FRAME_SIZE] = {0};

  // Header
  remote_state[0] = 0xD3;
  remote_state[1] = 0x11;

  remote_state[2] = 0;
  remote_state[2] |= this->operation_mode_();
  remote_state[2] |= (this->fan_speed_() << 4);
  remote_state[2] |= (this->turbo_control_() << 7);

  remote_state[3] = 0;
  remote_state[3] |= this->temperature_();

  remote_state[4] = 0;
  remote_state[4] |= this->get_vertical_direction_state().to_uint8();

  remote_state[5] = this->operation_settings_();

  remote_state[6] = 0;
  remote_state[6] |= this->checksum_(remote_state);

  ESP_LOGV(TAG, "Sending: %02X %02X %02X %02X %02X %02X %02X", remote_state[6], remote_state[5], remote_state[4],
           remote_state[3], remote_state[2], remote_state[1], remote_state[0]);

  // Build payload inside 'data'
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(AIRTON_IR_FREQUENCY);

  // Header
  data->mark(AIRTON_HEADER_MARK);
  data->space(AIRTON_HEADER_SPACE);

  // Data
  for (uint8_t payload_byte : remote_state) {
    for (uint8_t payload_bit_cursor = 0; payload_bit_cursor < 8; payload_bit_cursor++) {
      data->mark(AIRTON_BIT_MARK);
      bool bit = payload_byte & (1 << payload_bit_cursor);
      data->space(bit ? AIRTON_ONE_SPACE : AIRTON_ZERO_SPACE);
    }
  }

  // Footer
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
      operating_mode = 0b0111 & this->get_previous_mode_();  // Set previous mode with power state bit off
  }
  this->set_previous_mode_(operating_mode);
  return operating_mode;
}

uint16_t AirtonClimate::fan_speed_() {
  uint16_t fan_speed;
  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_ON)) {
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

bool AirtonClimate::turbo_control_() {
  bool turbo_control = false;  // My remote seems to always have this set to 0
  return turbo_control;
}

uint8_t AirtonClimate::temperature_() {
  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT_COOL:
      // Fixed 25C setpoint in Auto mode
      return 9;
    default:
      uint8_t temperature = (uint8_t) roundf(clamp<float>(this->target_temperature, AIRTON_TEMP_MIN, AIRTON_TEMP_MAX));
      // Set correct temperature integer, offset by 16
      return temperature - 16;
  }
}

// The bits of this packet's byte have the following meanings (from MSB to LSB)
// Light, Health, Unknown, HeatOn, Unknown, NotAutoOn, Sleep, Econo
uint8_t AirtonClimate::operation_settings_() {
  uint8_t settings = 0;
  if (this->mode == climate::CLIMATE_MODE_HEAT) {  // Set heating bit if on the corresponding mode
    settings |= (1 << 4);
  }
  if (this->get_display_state()) {  // Set LED display
    settings |= (1 << 7);
  }
  if (this->get_sleep_mode_state()) {  // Set sleep mode
    settings |= (1 << 1);
  }
  settings |= 0b01000100;  // Set Health and NotAutoOn bits as per default
  return settings;
}

// From IRutils.h of IRremoteESP8266 library
uint8_t AirtonClimate::sum_bytes_(const uint8_t *const start, const uint16_t length) {
  uint8_t checksum = 0;
  const uint8_t *ptr;
  for (ptr = start; ptr - start < length; ptr++)
    checksum += *ptr;
  return checksum;
}
// From IRutils.h of IRremoteESP8266 library
uint8_t AirtonClimate::checksum_(const uint8_t *r_state) {
  uint8_t checksum = (uint8_t) (0x7F - this->sum_bytes_(r_state, 6)) ^ 0x2C;
  return checksum;
}

bool AirtonClimate::parse_state_frame_(uint8_t const frame[]) {
  uint8_t mode = frame[2];
  if (mode & 0b00001000) {        // Check if power state bit is set
    switch (mode & 0b00000111) {  // Mask anything but the least significant 3 bits
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
  uint8_t fan_mode = (frame[2] & 0b01110000) >> 4;  // Mask anything but bits 5-7, then shift them to the right

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

  uint8_t temperature = frame[3];
  this->target_temperature =
      (temperature & 0b00001111) + 16;  // Mask the higher half of the byte (unused), add back the offset

  uint8_t swing_mode = frame[4] & 0b00001111;  // Mask the higher nibble
  if (swing_mode == (uint8_t) VerticalDirection::VERTICAL_DIRECTION_OFF) {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  }
  this->set_vertical_direction_state(static_cast<VerticalDirection::Direction>(swing_mode));

  uint8_t display_light = frame[5] & 0b10000000;  // Mask anything but the MSB
  this->set_display_state(display_light != 0);

  uint8_t sleep_mode = frame[5] & 0b00000010;  // Mask anything but the second bit
  this->set_sleep_mode_state(sleep_mode != 0);

  this->publish_state();
  return true;
}

bool AirtonClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t remote_state[AIRTON_STATE_FRAME_SIZE] = {};
  // Check header encoding
  if (!data.expect_item(AIRTON_HEADER_MARK, AIRTON_HEADER_SPACE)) {
    ESP_LOGV(TAG, "Wrong header encoding detected!");
    return false;
  }

  // Build state bytes array from raw data received
  for (int i = 0; i < AIRTON_STATE_FRAME_SIZE; i++) {
    for (int j = 0; j < 8; j++) {
      if (data.expect_item(AIRTON_BIT_MARK, AIRTON_ONE_SPACE)) {
        remote_state[i] |= 1 << j;
      } else if (!data.expect_item(AIRTON_BIT_MARK, AIRTON_ZERO_SPACE)) {
        ESP_LOGV(TAG, "Wrong modulation encoding for: Byte %d, bit %d", i, j);
        return false;
      }
    }
  }

  // Check header contents
  if (remote_state[0] != 0xD3 || remote_state[1] != 0x11) {
    ESP_LOGV(TAG, "Wrong header contents: %02X %02X", remote_state[1], remote_state[0]);
    return false;
  }

  // Verify received packet checksum
  uint8_t checksum = this->checksum_(remote_state);
  if (remote_state[AIRTON_STATE_FRAME_SIZE - 1] != checksum) {
    ESP_LOGV(TAG, "Checksum error:\n calculated - %02X\n received - %02X", checksum,
             remote_state[AIRTON_STATE_FRAME_SIZE - 1]);
    return false;
  }

  // Parse the payload
  ESP_LOGV(TAG, "Received: %02X %02X %02X %02X %02X %02X %02X", remote_state[6], remote_state[5], remote_state[4],
           remote_state[3], remote_state[2], remote_state[1], remote_state[0]);
  return this->parse_state_frame_(remote_state);
}

}  // namespace airton
}  // namespace esphome
