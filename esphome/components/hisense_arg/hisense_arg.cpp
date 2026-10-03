#include "hisense_arg.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome::hisense_arg {

static const char *const TAG = "hisense_arg.climate";

uint8_t HisenseArgClimate::fan_speed_() const {
  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_LOW:
      return HISENSE_ARG_FAN_LOW;
    case climate::CLIMATE_FAN_MEDIUM:
      return HISENSE_ARG_FAN_MEDIUM;
    case climate::CLIMATE_FAN_HIGH:
      return HISENSE_ARG_FAN_HIGH;
    default:
      return HISENSE_ARG_FAN_AUTO;
  }
}

uint8_t HisenseArgClimate::operation_mode_() const {
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
    case climate::CLIMATE_MODE_OFF:  // Off shares cool's IR code
      return HISENSE_ARG_MODE_COOL;
    case climate::CLIMATE_MODE_HEAT:
      return HISENSE_ARG_MODE_HEAT;
    case climate::CLIMATE_MODE_DRY:
      return HISENSE_ARG_MODE_DRY;
    case climate::CLIMATE_MODE_FAN_ONLY:
      return HISENSE_ARG_MODE_FAN;
    case climate::CLIMATE_MODE_HEAT_COOL:
      return HISENSE_ARG_MODE_AUTO;
    default:
      return HISENSE_ARG_MODE_COOL;
  }
}

uint8_t HisenseArgClimate::temperature_() const {
  switch (this->mode) {
    case climate::CLIMATE_MODE_FAN_ONLY:
      return (HISENSE_ARG_TEMP_FAN - HISENSE_ARG_TEMP_MIN);
    case climate::CLIMATE_MODE_HEAT_COOL:
      return 7;  // Auto mode: fixed 0x70 in upper nibble
    default: {
      uint8_t temp =
          (uint8_t) roundf(clamp<float>(this->target_temperature, HISENSE_ARG_TEMP_MIN, HISENSE_ARG_TEMP_MAX));
      return (temp - HISENSE_ARG_TEMP_MIN);
    }
  }
}

uint8_t HisenseArgClimate::swing_() const {
  if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) {
    return HISENSE_ARG_SWING_V;
  }
  return 0x00;
}

void HisenseArgClimate::transmit_state() {
  uint8_t remote_state[HISENSE_ARG_STATE_SIZE] = {// Frame 1 (bytes 0-5)
                                                  0x83, 0x06, 0x00, 0x00, 0x00, 0x00,
                                                  // Frame 2 (bytes 6-13)
                                                  0x80, 0x00, 0x00, 0x00, 0x00, 0x80, 0x18, 0x00,
                                                  // Frame 3 (bytes 14-20)
                                                  0x00, 0x00, 0x00, 0x00, HISENSE_ARG_VARIANT_A, 0x00, 0x00};

  bool power_on = (this->mode != climate::CLIMATE_MODE_OFF);

  // --- Frame 1 (bytes 0-5) ---

  // Byte 2: fan speed + flags
  uint8_t fan = this->fan_speed_();
  if (this->mode == climate::CLIMATE_MODE_DRY) {
    // Dry mode: force fan to auto, heat: force fan to auto
    fan = HISENSE_ARG_FAN_AUTO;
  } else if (this->mode == climate::CLIMATE_MODE_FAN_ONLY && fan == HISENSE_ARG_FAN_AUTO) {
    // Fan-only with auto speed: force mid indication
    fan = HISENSE_ARG_FAN_MEDIUM;
  }
  remote_state[2] = fan;

  // Auto mode: add temperature range bits in [6:4]
  if (this->mode == climate::CLIMATE_MODE_HEAT_COOL) {
    uint8_t temp = (uint8_t) roundf(clamp<float>(this->target_temperature, HISENSE_ARG_TEMP_MIN, HISENSE_ARG_TEMP_MAX));
    uint8_t step = temp - HISENSE_ARG_TEMP_MIN;
    if (step < 6) {
      remote_state[2] |= 0x30;
    } else if (step == 6) {
      remote_state[2] |= 0x50;
    } else if (step == 8) {
      remote_state[2] |= 0x40;
    } else {
      remote_state[2] |= 0x20;
    }
  }

  // Vertical swing indicator in byte 2 bit 7
  if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) {
    remote_state[2] |= 0x80;
  }

  // Byte 3: mode (bits[2:0]) + temperature (bits[7:4])
  remote_state[3] = (this->temperature_() << 4) | this->operation_mode_();

  // Auto mode: byte[3] gets fixed 0x70 temp + mode
  if (this->mode == climate::CLIMATE_MODE_HEAT_COOL) {
    remote_state[3] = 0x70 | HISENSE_ARG_MODE_AUTO;
  }

  // Bytes 4, 5: reserved (0x00)

  // --- Frame 2 (bytes 6-13) ---

  // Byte 6: timer hours base (0x80 always set, no timer support here)
  // Byte 7: timer minutes (0x00)

  // Byte 8: swing
  remote_state[8] = this->swing_();

  // Byte 9, 10: reserved (0x00)
  // Byte 11: display flag (0x80, already set in init)
  // Byte 12: ambient temperature (default 24C = 0x18, already set in init)

  // Byte 2 bit 2: power transition flag (must be set before checksum)
  bool power_transition = (power_on != this->prev_power_on_);
  if (power_transition) {
    remote_state[2] |= HISENSE_ARG_POWER_FLAG;
  }

  // Byte 13: XOR checksum of bytes 2-12
  remote_state[13] = 0;
  for (int i = 2; i < 13; i++) {
    remote_state[13] ^= remote_state[i];
  }

  // --- Frame 3 (bytes 14-20) ---

  // Byte 15: power transition
  if (power_transition) {
    remote_state[15] = HISENSE_ARG_POWER_TOGGLE;  // Power state change (toggle)
  } else {
    remote_state[15] = HISENSE_ARG_POWER_NO_CHANGE;  // No power change, settings only
  }

  // Byte 18: variant flag (HISENSE_ARG_VARIANT_A = 0x08, already set in init)

  // Byte 20: XOR checksum of bytes 14-19
  remote_state[20] = 0;
  for (int i = 14; i < 20; i++) {
    remote_state[20] ^= remote_state[i];
  }

  // Update power state tracking
  this->prev_power_on_ = power_on;

  // --- Transmit ---
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(HISENSE_ARG_IR_FREQUENCY);

  // Header
  data->mark(HISENSE_ARG_HEADER_MARK);
  data->space(HISENSE_ARG_HEADER_SPACE);

  // Frame 1: 6 bytes (bytes 0-5)
  for (int i = 0; i < HISENSE_ARG_FRAME1_SIZE; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {
      data->mark(HISENSE_ARG_BIT_MARK);
      data->space((remote_state[i] & mask) ? HISENSE_ARG_ONE_SPACE : HISENSE_ARG_ZERO_SPACE);
    }
  }

  // Separator
  data->mark(HISENSE_ARG_BIT_MARK);
  data->space(HISENSE_ARG_SEPARATOR_SPACE);

  // Frame 2: 8 bytes (bytes 6-13)
  for (int i = HISENSE_ARG_FRAME1_SIZE; i < HISENSE_ARG_FRAME1_SIZE + HISENSE_ARG_FRAME2_SIZE; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {
      data->mark(HISENSE_ARG_BIT_MARK);
      data->space((remote_state[i] & mask) ? HISENSE_ARG_ONE_SPACE : HISENSE_ARG_ZERO_SPACE);
    }
  }

  // Separator
  data->mark(HISENSE_ARG_BIT_MARK);
  data->space(HISENSE_ARG_SEPARATOR_SPACE);

  // Frame 3: 7 bytes (bytes 14-20)
  for (int i = HISENSE_ARG_FRAME1_SIZE + HISENSE_ARG_FRAME2_SIZE; i < HISENSE_ARG_STATE_SIZE; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {
      data->mark(HISENSE_ARG_BIT_MARK);
      data->space((remote_state[i] & mask) ? HISENSE_ARG_ONE_SPACE : HISENSE_ARG_ZERO_SPACE);
    }
  }

  // Trail
  data->mark(HISENSE_ARG_BIT_MARK);
  data->space(HISENSE_ARG_TRAIL_SPACE);

  transmit.perform();
}

bool HisenseArgClimate::on_receive(remote_base::RemoteReceiveData data) {
  // Validate header
  if (!data.expect_item(HISENSE_ARG_HEADER_MARK, HISENSE_ARG_HEADER_SPACE)) {
    return false;
  }

  // Decode all 21 bytes (3 frames)
  uint8_t remote_state[HISENSE_ARG_STATE_SIZE] = {};

  // Frame 1: 6 bytes (bytes 0-5)
  for (int i = 0; i < HISENSE_ARG_FRAME1_SIZE; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {
      if (data.expect_item(HISENSE_ARG_BIT_MARK, HISENSE_ARG_ONE_SPACE)) {
        remote_state[i] |= mask;
      } else if (data.expect_item(HISENSE_ARG_BIT_MARK, HISENSE_ARG_ZERO_SPACE)) {
        // bit is 0, already cleared
      } else {
        return false;
      }
    }
  }

  // Separator after frame 1
  if (!data.expect_item(HISENSE_ARG_BIT_MARK, HISENSE_ARG_SEPARATOR_SPACE)) {
    return false;
  }

  // Frame 2: 8 bytes (bytes 6-13)
  for (int i = HISENSE_ARG_FRAME1_SIZE; i < HISENSE_ARG_FRAME1_SIZE + HISENSE_ARG_FRAME2_SIZE; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {
      if (data.expect_item(HISENSE_ARG_BIT_MARK, HISENSE_ARG_ONE_SPACE)) {
        remote_state[i] |= mask;
      } else if (data.expect_item(HISENSE_ARG_BIT_MARK, HISENSE_ARG_ZERO_SPACE)) {
        // bit is 0
      } else {
        return false;
      }
    }
  }

  // Check for frame 3 (separator + 7 more bytes) or trail
  if (data.expect_item(HISENSE_ARG_BIT_MARK, HISENSE_ARG_SEPARATOR_SPACE)) {
    // Frame 3: 7 bytes (bytes 14-20)
    for (int i = HISENSE_ARG_FRAME1_SIZE + HISENSE_ARG_FRAME2_SIZE; i < HISENSE_ARG_STATE_SIZE; i++) {
      for (uint8_t mask = 1; mask > 0; mask <<= 1) {
        if (data.expect_item(HISENSE_ARG_BIT_MARK, HISENSE_ARG_ONE_SPACE)) {
          remote_state[i] |= mask;
        } else if (data.expect_item(HISENSE_ARG_BIT_MARK, HISENSE_ARG_ZERO_SPACE)) {
          // bit is 0
        } else {
          return false;
        }
      }
    }
  }

  // Validate header bytes
  if (remote_state[0] != 0x83 || remote_state[1] != 0x06) {
    return false;
  }

  // Validate checksum (bytes 2-12 XOR = byte 13)
  uint8_t checksum = 0;
  for (int i = 2; i < 13; i++) {
    checksum ^= remote_state[i];
  }
  if (checksum != remote_state[13]) {
    return false;
  }

  return this->parse_state_frame_(remote_state);
}

bool HisenseArgClimate::parse_state_frame_(const uint8_t frame[]) {
  // Decode mode from byte[3] bits[2:0]
  uint8_t ir_mode = frame[3] & 0x07;
  // Decode temperature from byte[3] bits[7:4]
  uint8_t temp_offset = (frame[3] >> 4) & 0x0F;
  // Decode fan from byte[2] bits[1:0]
  uint8_t ir_fan = frame[2] & 0x03;
  // Decode swing from byte[8]
  uint8_t ir_swing = frame[8];

  // Determine power state from frame 3 byte[15] and byte[2] bit 2
  bool is_power_toggle = (frame[15] == HISENSE_ARG_POWER_TOGGLE) && (frame[2] & HISENSE_ARG_POWER_FLAG);

  if (is_power_toggle) {
    // Toggle: flip from previous known state
    if (this->prev_power_on_) {
      this->mode = climate::CLIMATE_MODE_OFF;
      this->prev_power_on_ = false;
    } else {
      this->prev_power_on_ = true;
      // Decode mode below
    }
  }

  // If not toggling off, decode mode
  if (this->mode != climate::CLIMATE_MODE_OFF || !is_power_toggle) {
    switch (ir_mode) {
      case HISENSE_ARG_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case HISENSE_ARG_MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case HISENSE_ARG_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case HISENSE_ARG_MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case HISENSE_ARG_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      default:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
    }
  }

  // Fan speed
  switch (ir_fan) {
    case HISENSE_ARG_FAN_LOW:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case HISENSE_ARG_FAN_MEDIUM:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case HISENSE_ARG_FAN_HIGH:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    default:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  // Temperature (skip for auto/fan modes which use fixed values)
  if (this->mode != climate::CLIMATE_MODE_HEAT_COOL && this->mode != climate::CLIMATE_MODE_FAN_ONLY) {
    this->target_temperature = temp_offset + HISENSE_ARG_TEMP_MIN;
  }

  // Swing
  if (ir_swing & HISENSE_ARG_SWING_V) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }

  this->publish_state();
  return true;
}

}  // namespace esphome::hisense_arg
