#include "zhlt01.h"
#include "esphome/core/log.h"

namespace esphome::zhlt01 {

ESPHOME_LOG_TAG(TAG, "zhlt01.climate");

static constexpr uint8_t ZHLT01_MESSAGE_LENGTH = 12;

using climate_ir::find_ir_code;
using climate_ir::from_ir_code;
using climate_ir::IRCode;
using climate_ir::to_ir_code;

// The first entry of each table is the fallback, except that a received fan code not listed is ignored
static constexpr IRCode MODES[] PROGMEM = {
    {climate::CLIMATE_MODE_AUTO, AC1_MODE_AUTO},    {climate::CLIMATE_MODE_COOL, AC1_MODE_COOL},
    {climate::CLIMATE_MODE_HEAT, AC1_MODE_HEAT},    {climate::CLIMATE_MODE_DRY, AC1_MODE_DRY},
    {climate::CLIMATE_MODE_FAN_ONLY, AC1_MODE_FAN},
};
static constexpr IRCode FANS[] PROGMEM = {
    {climate::CLIMATE_FAN_AUTO, AC1_FAN_AUTO},
    {climate::CLIMATE_FAN_LOW, AC1_FAN1},
    {climate::CLIMATE_FAN_MEDIUM, AC1_FAN2},
    {climate::CLIMATE_FAN_HIGH, AC1_FAN3},
};

void ZHLT01Climate::transmit_state() {
  uint8_t ir_message[ZHLT01_MESSAGE_LENGTH] = {0};

  // Byte 1 : Timer
  ir_message[1] = 0x00;  // Timer off

  // Byte 3 : Turbo mode
  if (this->preset.value_or(climate::CLIMATE_PRESET_NONE) == climate::CLIMATE_PRESET_BOOST) {
    ir_message[3] = AC1_FAN_TURBO;
  }

  // Byte 5 : Last pressed button
  ir_message[5] = 0x00;  // fixed as power button

  // Byte 7 : Power | Swing | Fan
  // -- Power
  ir_message[7] = this->mode == climate::CLIMATE_MODE_OFF ? AC1_POWER_OFF : AC1_POWER_ON;

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
  switch (this->preset.value_or(climate::CLIMATE_PRESET_NONE)) {
    case climate::CLIMATE_PRESET_BOOST:
      ir_message[7] |= AC1_FAN3;
      break;
    case climate::CLIMATE_PRESET_SLEEP:
      ir_message[7] |= AC1_FAN_SILENT;
      break;
    default:
      ir_message[7] |= to_ir_code(FANS, this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO));
  }

  // Byte 9 : AC Mode | Temperature
  // -- AC Mode
  ir_message[9] = to_ir_code(MODES, this->mode);

  // -- Temperature
  ir_message[9] |= (uint8_t) (this->target_temperature - 16.0f);

  // Byte 11 : Remote control ID
  ir_message[11] = 0xD5;

  // Set checksum bytes
  for (int i = 0; i < ZHLT01_MESSAGE_LENGTH; i += 2) {
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
  data->item_bytes<AC1_BIT_TIMINGS>(ir_message, sizeof(ir_message));

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
  uint8_t ir_message[ZHLT01_MESSAGE_LENGTH];
  if (!data.expect_bytes<AC1_BIT_TIMINGS>(ir_message, sizeof(ir_message))) {
    ESP_LOGV(TAG, "Data fail");
    return false;
  }

  // Validate footer
  if (!data.expect_mark(AC1_BIT_MARK)) {
    ESP_LOGV(TAG, "Footer fail");
    return false;
  }

  // Validate checksum
  for (int i = 0; i < ZHLT01_MESSAGE_LENGTH; i += 2) {
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
    } else if (const auto entry = find_ir_code(FANS, ir_message[7] & 0xE1, true)) {
      this->fan_mode = static_cast<climate::ClimateFanMode>(entry->value);
    }

    // AC Mode
    this->mode = from_ir_code<climate::ClimateMode>(MODES, ir_message[9] & 0xE0);

    // Target Temperature
    this->target_temperature = (ir_message[9] & 0x1F) + 16.0f;
  }

  this->publish_state();
  return true;
}

}  // namespace esphome::zhlt01
