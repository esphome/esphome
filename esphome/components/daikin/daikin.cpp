#include "daikin.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

namespace esphome::daikin {

ESPHOME_LOG_TAG(TAG, "daikin.climate");

static constexpr uint32_t BOOST_TIMEOUT_ID = 0;

void DaikinClimate::setup() {
  climate_ir::ClimateIR::setup();
  // A boost preset restored from flash gets its timer too
  this->update_boost_timer_(false);
}

void DaikinClimate::control(const climate::ClimateCall &call) {
  const bool was_boost = this->preset == climate::CLIMATE_PRESET_BOOST;
  climate_ir::ClimateIR::control(call);
  this->update_boost_timer_(was_boost);
}

void DaikinClimate::transmit_state() {
  uint8_t remote_state[35] = {0x11, 0xDA, 0x27, 0x00, 0xC5, 0x00, 0x00, 0x00, 0x11, 0xDA, 0x27, 0x00,
                              0x42, 0x49, 0x05, 0xA2, 0x11, 0xDA, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x06, 0x60, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00};

  this->set_presets_(remote_state);
  remote_state[21] = this->operation_mode_();
  remote_state[22] = this->temperature_();
  uint16_t fan_speed = this->fan_speed_();
  remote_state[24] = fan_speed >> 8;
  remote_state[25] = fan_speed & 0xff;

  remote_state[7] = sum_bytes(remote_state, 7);
  remote_state[34] = sum_bytes(remote_state + 16, 18);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(DAIKIN_IR_FREQUENCY);

  data->mark(DAIKIN_HEADER_MARK);
  data->space(DAIKIN_HEADER_SPACE);
  data->item_bytes_lsb<DAIKIN_BIT_TIMINGS>(remote_state, 8);
  data->mark(DAIKIN_BIT_MARK);
  data->space(DAIKIN_MESSAGE_SPACE);
  data->mark(DAIKIN_HEADER_MARK);
  data->space(DAIKIN_HEADER_SPACE);

  data->item_bytes_lsb<DAIKIN_BIT_TIMINGS>(remote_state + 8, 8);
  data->mark(DAIKIN_BIT_MARK);
  data->space(DAIKIN_MESSAGE_SPACE);
  data->mark(DAIKIN_HEADER_MARK);
  data->space(DAIKIN_HEADER_SPACE);

  data->item_bytes_lsb<DAIKIN_BIT_TIMINGS>(remote_state + 16, 19);
  data->mark(DAIKIN_BIT_MARK);
  data->space(0);

  transmit.perform();
}

uint8_t DaikinClimate::operation_mode_() const {
  uint8_t operating_mode = DAIKIN_MODE_ON;
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      operating_mode |= DAIKIN_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      operating_mode |= DAIKIN_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_HEAT:
      operating_mode |= DAIKIN_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      operating_mode |= DAIKIN_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      operating_mode |= DAIKIN_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      operating_mode = DAIKIN_MODE_OFF;
      break;
  }

  return operating_mode;
}

uint16_t DaikinClimate::fan_speed_() const {
  uint16_t fan_speed;
  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_ON)) {
    case climate::CLIMATE_FAN_QUIET:
      fan_speed = DAIKIN_FAN_SILENT << 8;
      break;
    case climate::CLIMATE_FAN_LOW:
      fan_speed = DAIKIN_FAN_1 << 8;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed = DAIKIN_FAN_3 << 8;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed = DAIKIN_FAN_5 << 8;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      fan_speed = DAIKIN_FAN_AUTO << 8;
  }

  // If swing is enabled switch first 4 bits to 1111
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
      fan_speed |= 0x0F00;
      break;
    case climate::CLIMATE_SWING_HORIZONTAL:
      fan_speed |= 0x000F;
      break;
    case climate::CLIMATE_SWING_BOTH:
      fan_speed |= 0x0F0F;
      break;
    default:
      break;
  }
  return fan_speed;
}

uint8_t DaikinClimate::temperature_() const {
  // Force special temperatures depending on the mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_FAN_ONLY:
      return 0x32;
    case climate::CLIMATE_MODE_DRY:
      return 0xc0;
    default:
      return static_cast<uint8_t>(
          roundf(clamp<float>(this->target_temperature, DAIKIN_TEMP_MIN, DAIKIN_TEMP_MAX) * 2.0f));
  }
}

void DaikinClimate::set_presets_(uint8_t *remote_state) const {
  switch (this->preset.value_or(climate::CLIMATE_PRESET_NONE)) {
    case climate::CLIMATE_PRESET_BOOST:
      remote_state[29] = DAIKIN_PRESET_POWERFUL_ON;
      break;
    case climate::CLIMATE_PRESET_SLEEP:
      remote_state[29] = DAIKIN_PRESET_QUIET_ON;
      break;
    case climate::CLIMATE_PRESET_ECO:
      remote_state[32] = DAIKIN_PRESET_ECONO_ON;
      break;
    case climate::CLIMATE_PRESET_ACTIVITY:
      remote_state[32] = DAIKIN_PRESET_SENSOR_ON;
      break;
    case climate::CLIMATE_PRESET_COMFORT:
      remote_state[6] = DAIKIN_PRESET_COMFORT_ON;  // the only preset carried by the first frame
      break;
    default:
      break;
  }
}

void DaikinClimate::update_boost_timer_(bool was_boost) {
  const bool is_boost = this->preset == climate::CLIMATE_PRESET_BOOST;
  if (is_boost && !was_boost) {
    this->set_timeout(BOOST_TIMEOUT_ID, DAIKIN_BOOST_DURATION_MS, [this]() { this->cancel_boost_mode_(); });
  } else if (was_boost && !is_boost) {
    this->cancel_timeout(BOOST_TIMEOUT_ID);
  }
}

void DaikinClimate::cancel_boost_mode_() {
  if (this->preset != climate::CLIMATE_PRESET_BOOST)
    return;
  ESP_LOGD(TAG, "Boost ended");
  this->preset = climate::CLIMATE_PRESET_NONE;
  this->publish_state();
}

bool DaikinClimate::parse_state_frame_(const uint8_t frame[]) {
  const bool was_boost = this->preset == climate::CLIMATE_PRESET_BOOST;
  uint8_t mode = frame[5];
  // Temperature is given in degrees celcius * 2
  // only update for states that use the temperature
  uint8_t temperature = frame[6];
  if (mode & DAIKIN_MODE_ON) {
    switch (mode & 0xF0) {
      case DAIKIN_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        this->target_temperature = static_cast<float>(temperature * 0.5f);
        break;
      case DAIKIN_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case DAIKIN_MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        this->target_temperature = static_cast<float>(temperature * 0.5f);
        break;
      case DAIKIN_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        this->target_temperature = static_cast<float>(temperature * 0.5f);
        break;
      case DAIKIN_MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
    }
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
  }
  uint8_t fan_mode = frame[8];
  uint8_t swing_mode = frame[9];
  if (fan_mode & 0xF && swing_mode & 0xF) {
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  } else if (fan_mode & 0xF) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else if (swing_mode & 0xF) {
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }
  switch (fan_mode & 0xF0) {
    case DAIKIN_FAN_1:
    case DAIKIN_FAN_2:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case DAIKIN_FAN_3:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case DAIKIN_FAN_4:
    case DAIKIN_FAN_5:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case DAIKIN_FAN_AUTO:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
    case DAIKIN_FAN_SILENT:
      this->fan_mode = climate::CLIMATE_FAN_QUIET;
      break;
  }

  // Comfort lives in the first frame, recorded as pending until this frame proves the transmission valid
  // A comfort frame whose own state frame never came must not attach to a later transmission
  const uint32_t now = App.get_loop_component_start_time();
  const PendingComfort comfort = now - this->pending_comfort_at_ <= DAIKIN_TRANSMISSION_MS
                                     ? this->pending_comfort_
                                     : PendingComfort::PENDING_COMFORT_UNKNOWN;
  this->pending_comfort_ = PendingComfort::PENDING_COMFORT_UNKNOWN;
  if (frame[13] & DAIKIN_PRESET_POWERFUL_ON) {
    this->preset = climate::CLIMATE_PRESET_BOOST;
  } else if (frame[13] & DAIKIN_PRESET_QUIET_ON) {
    this->preset = climate::CLIMATE_PRESET_SLEEP;
  } else if (frame[16] & DAIKIN_PRESET_ECONO_ON) {
    this->preset = climate::CLIMATE_PRESET_ECO;
  } else if (frame[16] & DAIKIN_PRESET_SENSOR_ON) {
    this->preset = climate::CLIMATE_PRESET_ACTIVITY;
  } else if (comfort == PendingComfort::PENDING_COMFORT_ON) {
    this->preset = climate::CLIMATE_PRESET_COMFORT;
  } else if (comfort == PendingComfort::PENDING_COMFORT_OFF || this->preset != climate::CLIMATE_PRESET_COMFORT) {
    // Without a comfort frame in this transmission a current comfort preset is left alone
    this->preset = climate::CLIMATE_PRESET_NONE;
  }
  this->update_boost_timer_(was_boost);

  this->publish_state();
  return true;
}

bool DaikinClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t frame[DAIKIN_STATE_FRAME_SIZE];
  if (!data.expect_item(DAIKIN_HEADER_MARK, DAIKIN_HEADER_SPACE)) {
    return false;
  }

  // Frame header, then the frame type that sets the frame size
  if (!data.expect_bytes_lsb<DAIKIN_BIT_TIMINGS>(frame, 5) || frame[0] != 0x11 || frame[1] != 0xDA ||
      frame[2] != 0x27 || frame[3] != 0x00) {
    return false;
  }
  uint8_t frame_size = DAIKIN_STATE_FRAME_SIZE;
  if (frame[4] == DAIKIN_FRAME_TYPE_COMFORT) {
    frame_size = DAIKIN_COMFORT_FRAME_SIZE;
  } else if (frame[4] != DAIKIN_FRAME_TYPE_STATE) {
    return false;
  }
  if (!data.expect_bytes_lsb<DAIKIN_BIT_TIMINGS>(frame + 5, frame_size - 5)) {
    return false;
  }
  if (frame[frame_size - 1] != sum_bytes(frame, frame_size - 1)) {
    return false;
  }

  if (frame_size == DAIKIN_COMFORT_FRAME_SIZE) {
    // Nothing changes yet: the state frame that follows applies it once it has passed its own checksum
    this->pending_comfort_ = (frame[6] & DAIKIN_PRESET_COMFORT_ON) ? PendingComfort::PENDING_COMFORT_ON
                                                                   : PendingComfort::PENDING_COMFORT_OFF;
    this->pending_comfort_at_ = App.get_loop_component_start_time();
    return true;
  }
  return this->parse_state_frame_(frame);
}

}  // namespace esphome::daikin
