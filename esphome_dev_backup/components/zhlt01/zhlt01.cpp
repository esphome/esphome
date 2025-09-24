#include "zhlt01.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zhlt01 {

static const char *const TAG = "zhlt01.climate";

void ZHLT01Climate::transmit_state() {
  uint8_t ir_message[12] = {0};

  // Byte 1 : Timer
  ir_message[1] = 0x00;  // Timer off

  // Byte 3 : Turbo mode
  if (this->preset.value() == climate::CLIMATE_PRESET_BOOST) {
    ir_message[3] = AC1_FAN_TURBO;
  }

  // Byte 5 : Last pressed button
  ir_message[5] = 0x00;  // fixed as power button

  // Byte 7 : Power | Swing | Fan
  // -- Power
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    ir_message[7] = AC1_POWER_OFF;
  } else {
    ir_message[7] = AC1_POWER_ON;
  }

  // -- Swing
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_OFF:
      ir_message[7] |= AC1_HDIR_FIXED | AC1_VDIR_FIXED;
      break;
    case climate::CLIMATE_SWING_HORIZONTAL:
      ir_message[7] |= AC1_HDIR_SWING | AC1_VDIR_FIXED;
      break;
    case climate::CLIMATE_SWING_VERTICAL:
      ir_message[7] |= AC1_HDIR_FIXED | AC1_VDIR_SWING;
      break;
    case climate::CLIMATE_SWING_BOTH:
      ir_message[7] |= AC1_HDIR_SWING | AC1_VDIR_SWING;
      break;
    default:
      break;
  }

  // -- Fan
  switch (this->preset.value()) {
    case climate::CLIMATE_PRESET_BOOST:
      ir_message[7] |= AC1_FAN3;
      break;
    case climate::CLIMATE_PRESET_SLEEP:
      ir_message[7] |= AC1_FAN_SILENT;
      break;
    default:
      switch (this->fan_mode.value()) {
        case climate::CLIMATE_FAN_LOW:
          ir_message[7] |= AC1_FAN1;
          break;
        case climate::CLIMATE_FAN_MEDIUM:
          ir_message[7] |= AC1_FAN2;
          break;
        case climate::CLIMATE_FAN_HIGH:
          ir_message[7] |= AC1_FAN3;
          break;
        case climate::CLIMATE_FAN_AUTO:
          ir_message[7] |= AC1_FAN_AUTO;
          break;
        default:
          break;
      }
  }

  // Byte 9 : AC Mode | Temperature
  // -- AC Mode
  switch (this->mode) {
    case climate::CLIMATE_MODE_AUTO:
    case climate::CLIMATE_MODE_HEAT_COOL:
      ir_message[9] = AC1_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_COOL:
      ir_message[9] = AC1_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      ir_message[9] = AC1_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_DRY:
      ir_message[9] = AC1_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      ir_message[9] = AC1_MODE_FAN;
      break;
    default:
      break;
  }

  // -- Temperature
  ir_message[9] |= (uint8_t) (this->target_temperature - 16.0f);

  // Byte 11 : Remote control ID
  ir_message[11] = 0xD5;

  // Set checksum bytes
  for (int i = 0; i < 12; i += 2) {
    ir_message[i] = ~ir_message[i + 1];
  }

  // Send the code
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(38000);  // 38 kHz PWM

  // Header
  data->mark(AC1_HDR_MARK);
  data->space(AC1_HDR_SPACE);

  // Data
  for (uint8_t i : ir_message) {
    for (uint8_t j = 0; j < 8; j++) {
      data->mark(AC1_BIT_MARK);
      bool bit = i & (1 << j);
      data->space(bit ? AC1_ONE_SPACE : AC1_ZERO_SPACE);
    }
  }

  // Footer
  data->mark(AC1_BIT_MARK);
  data->space(0);

  transmit.perform();
}

bool ZHLT01Climate::on_receive(remote_base::RemoteReceiveData data) {
  // Validate header
  if (!data.expect_item(AC1_HDR_MARK, AC1_HDR_SPACE)) {
    ESP_LOGV(TAG, "Header fail");
    return false;
  }

  // Decode IR message
  uint8_t ir_message[12] = {0};
  // Read all bytes
  for (int i = 0; i < 12; i++) {
    // Read bit
    for (int j = 0; j < 8; j++) {
      if (data.expect_item(AC1_BIT_MARK, AC1_ONE_SPACE)) {
        ir_message[i] |= 1 << j;
      } else if (!data.expect_item(AC1_BIT_MARK, AC1_ZERO_SPACE)) {
        ESP_LOGV(TAG, "Byte %d bit %d fail", i, j);
        return false;
      }
    }
    ESP_LOGVV(TAG, "Byte %d %02X", i, ir_message[i]);
  }

  // Validate footer
  if (!data.expect_mark(AC1_BIT_MARK)) {
    ESP_LOGV(TAG, "Footer fail");
    return false;
  }

  // Validate checksum
  for (int i = 0; i < 12; i += 2) {
    if (ir_message[i] != (uint8_t) (~ir_message[i + 1])) {
      ESP_LOGV(TAG, "Byte %d checksum incorrect (%02X != %02X)", i, ir_message[i], (uint8_t) (~ir_message[i + 1]));
      return false;
    }
  }

  // Validate remote control ID
  if (ir_message[11] != 0xD5) {
    ESP_LOGV(TAG, "Invalid remote control ID");
    return false;
  }

  // All is good to go

  if ((ir_message[7] & AC1_POWER_ON) == 0) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    // Vertical swing
    if ((ir_message[7] & 0x0C) == AC1_VDIR_FIXED) {
      if ((ir_message[7] & 0x10) == AC1_HDIR_FIXED) {
        this->swing_mode = climate::CLIMATE_SWING_OFF;
      } else {
        this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
      }
    } else {
      if ((ir_message[7] & 0x10) == AC1_HDIR_FIXED) {
        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
      } else {
        this->swing_mode = climate::CLIMATE_SWING_BOTH;
      }
    }

    // Preset + Fan speed
    if ((ir_message[3] & AC1_FAN_TURBO) == AC1_FAN_TURBO) {
      this->preset = climate::CLIMATE_PRESET_BOOST;
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
    } else if ((ir_message[7] & 0xE1) == AC1_FAN_SILENT) {
      this->preset = climate::CLIMATE_PRESET_SLEEP;
      this->fan_mode = climate::CLIMATE_FAN_LOW;
    } else if ((ir_message[7] & 0xE1) == AC1_FAN_AUTO) {
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
    } else if ((ir_message[7] & 0xE1) == AC1_FAN1) {
      this->fan_mode = climate::CLIMATE_FAN_LOW;
    } else if ((ir_message[7] & 0xE1) == AC1_FAN2) {
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
    } else if ((ir_message[7] & 0xE1) == AC1_FAN3) {
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
    }

    // AC Mode
    if ((ir_message[9] & 0xE0) == AC1_MODE_COOL) {
      this->mode = climate::CLIMATE_MODE_COOL;
    } else if ((ir_message[9] & 0xE0) == AC1_MODE_HEAT) {
      this->mode = climate::CLIMATE_MODE_HEAT;
    } else if ((ir_message[9] & 0xE0) == AC1_MODE_DRY) {
      this->mode = climate::CLIMATE_MODE_DRY;
    } else if ((ir_message[9] & 0xE0) == AC1_MODE_FAN) {
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
    } else {
      this->mode = climate::CLIMATE_MODE_AUTO;
    }

    // Taregt Temperature
    this->target_temperature = (ir_message[9] & 0x1F) + 16.0f;
  }

  this->publish_state();
  return true;
}

}  // namespace zhlt01
}  // namespace esphome
