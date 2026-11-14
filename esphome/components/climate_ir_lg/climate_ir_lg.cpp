#include "climate_ir_lg.h"
#include "esphome/core/log.h"

namespace esphome::climate_ir_lg {

ESPHOME_LOG_TAG(TAG, "climate.climate_ir_lg");

// All codes provided here are missing the checksum (last 4 bits)
// this checksum needs to be calculated before sending (look at `calc_checksum_()`)

const uint32_t LG_HEADER = 0x8800000;

// Commands
const uint32_t COMMAND_HEADER_MASK = 0xFF000;
const uint32_t COMMAND_DATA_MASK = 0x00FF0;
const uint32_t CHECKSUM_MASK = 0xF;

enum CommandBasic : uint32_t {
  HEADER_BASIC = 0x10000,
  BASIC_SWING_TOGGLE = 0x000,

  // JET MODE (only for cooling/drying/heating modes)
  // For 30 minutes: max airflow (stronger than F5 aka FAN_MAX) + PO (min/min/max temperature respectively)
  // After 30 minutes: F5 aka FAN_MAX + min/min/max temperature respectively
  BASIC_JET = 0x080,
};

enum CommandSys : uint32_t {
  HEADER_SYS = 0xC0000,

  COMMAND_OFF = 0x050,

  // Also known as 'auto-dry'
  AUTO_CLEAN_ON = 0x0B0,
  AUTO_CLEAN_OFF = 0x0C0,

  PURIFY_ON = 0x000,   // From either OFF or Mode -> Purify
  PURIFY_OFF = 0x080,  // From Mode + Purify -> Mode

  QUIET_OUTDOOR_ON = 0xA60,
  QUIET_OUTDOOR_OFF = 0xA70,

  // ENERGY CTRL (only in Cooling mode)
  COOL_ENERG_CTRL_80 = 0x7D0,   // 80%
  COOL_ENERG_CTRL_60 = 0x7E0,   // 60%
  COOL_ENERG_CTRL_40 = 0x800,   // 40%
  COOL_ENERG_CTRL_OFF = 0x7F0,  // OFF

  DISPLAY_KW = 0x460,
  LIGHT_ON_OFF = 0x0A0,

  TEMP_UNIT_F = 0x170,
  TEMP_UNIT_C = 0x160,
};

enum CommandAdvSwing : uint32_t {
  HEADER_ADV_SWING = 0x13000,

  // Only 5 bits are relevant, I got 0x13952 once - not sure what is the 8th bit so ignoring that.
  ADV_SWING_DATA_MASK = 0x1F0,

  // Commands for Advanced Vertical Control: Swing + 6 fixed positions
  VERT_FIX_1 = 0x040,  // Down
  VERT_FIX_2 = 0x050,
  VERT_FIX_3 = 0x060,
  VERT_FIX_4 = 0x070,
  VERT_FIX_5 = 0x080,
  VERT_FIX_6 = 0x090,      // Up
  VERT_SWING_ON = 0x140,   // Swing between 1 and 6
  VERT_SWING_OFF = 0x150,  // Stops immediately

  // Commands for Advanced Horizontal Control: Swing (3 modes) + 5 fixed positions
  HORI_FIX_1 = 0x0B0,  // Left
  HORI_FIX_2 = 0x0C0,
  HORI_FIX_3 = 0x0D0,
  HORI_FIX_4 = 0x0E0,
  HORI_FIX_5 = 0x0F0,           // Right
  HORI_SWING_ON_LEFT = 0x100,   // Swing between 1 and 3
  HORI_SWING_ON_RIGHT = 0x110,  // Swing between 3 and 5
  HORI_SWING_ON_FULL = 0x160,   // Swing between 1 and 5
  HORI_SWING_OFF = 0x170,       // Stops immediately
};

// Following commands contain mode, fan speed and temperature

// Modes
const uint32_t COMMAND_ON_COOL = 0x00000;
const uint32_t COMMAND_ON_DRY = 0x01000;
const uint32_t COMMAND_ON_FAN_ONLY = 0x02000;
const uint32_t COMMAND_ON_AI = 0x03000;
const uint32_t COMMAND_ON_HEAT = 0x04000;

const uint32_t COMMAND_COOL = 0x08000;
const uint32_t COMMAND_DRY = 0x09000;
const uint32_t COMMAND_FAN_ONLY = 0x0A000;
const uint32_t COMMAND_AI = 0x0B000;
const uint32_t COMMAND_HEAT = 0x0C000;

// Fan speed
const uint32_t FAN_SPEED_MASK = 0xF0;
const uint32_t FAN_AUTO = 0x50;
const uint32_t FAN_MIN = 0x00;  // AKA F1
const uint32_t FAN_F2 = 0x90;
const uint32_t FAN_MED = 0x20;  // AKA F3
const uint32_t FAN_F4 = 0xA0;
const uint32_t FAN_MAX = 0x40;  // AKA F5

// Temperature
const uint8_t TEMP_RANGE = TEMP_MAX - TEMP_MIN + 1;
const uint32_t TEMP_MASK = 0xF00;
const uint32_t TEMP_SHIFT = 8;

const uint16_t BITS = 28;

using climate_ir::find_ir_code;
using climate_ir::IRCode;
using climate_ir::to_ir_code;

// Mode commands sit in bits 12-15; the command that also powers the unit on clears the top one
static constexpr uint8_t MODE_SHIFT = 12;
static constexpr uint8_t MODE_NOT_POWER_ON = COMMAND_COOL >> MODE_SHIFT;
// A mode not listed is sent as off
static constexpr IRCode MODES[] PROGMEM = {
    {climate::CLIMATE_MODE_COOL, COMMAND_COOL >> MODE_SHIFT},
    {climate::CLIMATE_MODE_DRY, COMMAND_DRY >> MODE_SHIFT},
    {climate::CLIMATE_MODE_FAN_ONLY, COMMAND_FAN_ONLY >> MODE_SHIFT},
    {climate::CLIMATE_MODE_HEAT_COOL, COMMAND_AI >> MODE_SHIFT},
    {climate::CLIMATE_MODE_HEAT, COMMAND_HEAT >> MODE_SHIFT},
};
// F2 and F4 are only received
static constexpr IRCode FANS[] PROGMEM = {
    {climate::CLIMATE_FAN_AUTO, FAN_AUTO}, {climate::CLIMATE_FAN_HIGH, FAN_MAX}, {climate::CLIMATE_FAN_MEDIUM, FAN_MED},
    {climate::CLIMATE_FAN_LOW, FAN_MIN},   {climate::CLIMATE_FAN_LOW, FAN_F2},   {climate::CLIMATE_FAN_MEDIUM, FAN_F4},
};

void LgIrClimate::transmit_state() {
  uint32_t remote_state = LG_HEADER;

  // ESP_LOGD(TAG, "climate_lg_ir mode_before_ code: 0x%02X", this->modeBefore_);

  // Set command
  if (this->send_swing_cmd_) {
    this->send_swing_cmd_ = false;
    if (this->advanced_commands_support_) {
      switch (this->swing_mode) {
        case climate::CLIMATE_SWING_VERTICAL:
          ESP_LOGD(TAG, "setting swing vertical");
          remote_state |= CommandAdvSwing::HEADER_ADV_SWING;
          remote_state |= CommandAdvSwing::VERT_SWING_ON;
          break;
        case climate::CLIMATE_SWING_OFF:
          ESP_LOGD(TAG, "setting swing off");
          remote_state |= CommandAdvSwing::HEADER_ADV_SWING;
          remote_state |= CommandAdvSwing::VERT_SWING_OFF;
          break;
        default:
          return;
      }
    } else {  // just toggle swing when advanced_commands_support is not set
      remote_state |= HEADER_BASIC;
      remote_state |= BASIC_SWING_TOGGLE;
    }
    // The remote's swing button sends a fixed code with no fan speed or temperature bits, so the
    // frame goes out as is instead of falling through to them
    this->transmit_(remote_state);
    return;
  } else {  // Mode commands
    if (const auto entry = find_ir_code(MODES, this->mode, false)) {
      const bool climate_is_off = (this->mode_before_ == climate::CLIMATE_MODE_OFF);
      remote_state |= static_cast<uint32_t>(climate_is_off ? entry->code & ~MODE_NOT_POWER_ON : entry->code)
                      << MODE_SHIFT;
    } else {
      remote_state |= CommandSys::HEADER_SYS;
      remote_state |= CommandSys::COMMAND_OFF;
    }
  }

  this->mode_before_ = this->mode;

  ESP_LOGD(TAG, "climate_lg_ir mode code: 0x%02X", this->mode);

  // Set fan speed
  if (this->mode !=
      climate::CLIMATE_MODE_OFF) {  // https://github.com/esphome/esphome/pull/10875#issuecomment-5042765948
    remote_state |= to_ir_code(FANS, this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO), FAN_AUTO);
  }

  uint8_t temp;
  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT_COOL:
      if (!this->advanced_commands_support_) {  // Keep previous behavior
        break;
      }
      [[fallthrough]];
    case climate::CLIMATE_MODE_COOL:
    case climate::CLIMATE_MODE_HEAT:
      temp = static_cast<uint8_t>(roundf(clamp<float>(this->target_temperature, TEMP_MIN, TEMP_MAX)));
      remote_state |= (temp - 15) << TEMP_SHIFT;
      break;
    default:
      break;
  }

  this->transmit_(remote_state);
}

bool LgIrClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t nbits = 0;
  uint32_t remote_state = 0;

  if (!data.expect_item(this->header_high_, this->header_low_))
    return false;

  for (nbits = 0; nbits < 32; nbits++) {
    if (data.expect_item(this->bit_high_, this->bit_one_low_)) {
      remote_state = (remote_state << 1) | 1;
    } else if (data.expect_item(this->bit_high_, this->bit_zero_low_)) {
      remote_state = (remote_state << 1) | 0;
    } else if (nbits == BITS) {
      break;
    } else {
      return false;
    }
  }

  ESP_LOGD(TAG, "Received 0x%02" PRIX32, remote_state);
  if ((remote_state & 0xFF00000) != LG_HEADER)
    return false;

  // Decode commands
  switch (remote_state & COMMAND_HEADER_MASK) {
    case CommandSys::HEADER_SYS:
      ESP_LOGD(TAG, "Got system command! With data: 0x%02" PRIX32, remote_state & COMMAND_DATA_MASK);
      if ((remote_state & COMMAND_DATA_MASK) == CommandSys::COMMAND_OFF) {
        this->mode = climate::CLIMATE_MODE_OFF;
      } else {
        return false;
      }
      break;
    case CommandAdvSwing::HEADER_ADV_SWING:
      ESP_LOGD(TAG, "Got advanced swing command! With data: 0x%02" PRIX32,
               remote_state & CommandAdvSwing::ADV_SWING_DATA_MASK);
      switch (remote_state & CommandAdvSwing::ADV_SWING_DATA_MASK) {
        case CommandAdvSwing::VERT_SWING_ON:
          this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
          break;
        case CommandAdvSwing::VERT_SWING_OFF:
        case CommandAdvSwing::VERT_FIX_1:
        case CommandAdvSwing::VERT_FIX_2:
        case CommandAdvSwing::VERT_FIX_3:
        case CommandAdvSwing::VERT_FIX_4:
        case CommandAdvSwing::VERT_FIX_5:
        case CommandAdvSwing::VERT_FIX_6:
          this->swing_mode = climate::CLIMATE_SWING_OFF;
          break;
        default:
          return false;  // Ignore all other (horizontal) swing commands
      }

      this->publish_state();
      return true;

    case HEADER_BASIC:
      if ((remote_state & COMMAND_DATA_MASK) == BASIC_JET) {
        switch (this->mode) {
          case climate::CLIMATE_MODE_COOL:
          case climate::CLIMATE_MODE_HEAT:
          case climate::CLIMATE_MODE_DRY:
            this->target_temperature =
                this->mode == climate::CLIMATE_MODE_HEAT ? this->maximum_temperature_ : this->minimum_temperature_;
            this->fan_mode = climate::CLIMATE_FAN_HIGH;
            // When enabling PO(WER) also known as JET mode, swing is set to VERT_3, but after 30 mins it will switch
            // back to what it was before, so let's just not change it here it at all
            this->publish_state();
            return true;
          default:
            ESP_LOGD(TAG, "Got jet command, but current mode does not support it! Ignoring.");
            return false;
        }
      }

      // Keep previous behavior in case of other BASIC command
      if (this->swing_mode == climate::CLIMATE_SWING_OFF) {  // Just flip between vertical and off
        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
      } else {
        this->swing_mode = climate::CLIMATE_SWING_OFF;
      }
      this->publish_state();
      return true;
    default: {
      // Mode commands also contain fan speed and temperature, so no 'return' here
      const uint8_t command = (remote_state & COMMAND_HEADER_MASK) >> MODE_SHIFT;
      const auto mode = find_ir_code(MODES, command | MODE_NOT_POWER_ON, true);
      if (!mode.has_value()) {
        ESP_LOGD(TAG, "Got unknown command! Ignoring!");
        return false;
      }
      this->mode = static_cast<climate::ClimateMode>(mode->value);
      break;
    }
  }

  const auto fan = find_ir_code(FANS, remote_state & FAN_SPEED_MASK, true);
  if (!fan.has_value()) {
    ESP_LOGD(TAG, "Got unknown fan speed! Ignoring!");
    return false;
  }
  this->fan_mode = static_cast<climate::ClimateFanMode>(fan->value);

  // Keep previous behavior
  if (this->mode == climate::CLIMATE_MODE_HEAT_COOL && !(this->advanced_commands_support_)) {
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
  }

  // Decode temperature for modes that support it
  switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT_COOL:
    case climate::CLIMATE_MODE_COOL:
    case climate::CLIMATE_MODE_HEAT:
      this->target_temperature = ((remote_state & TEMP_MASK) >> TEMP_SHIFT) + 15;
      break;
    default:
      break;
  }

  this->mode_before_ = this->mode;
  this->publish_state();

  return true;
}

void LgIrClimate::transmit_(uint32_t value) {
  this->calc_checksum_(value);
  ESP_LOGD(TAG, "Sending climate_lg_ir code: 0x%02" PRIX32, value);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(38000);
  data->reserve(2 + BITS * 2u);

  data->item(this->header_high_, this->header_low_);

  for (uint32_t mask = 1UL << (BITS - 1); mask != 0; mask >>= 1) {
    if (value & mask) {
      data->item(this->bit_high_, this->bit_one_low_);
    } else {
      data->item(this->bit_high_, this->bit_zero_low_);
    }
  }
  data->mark(this->bit_high_);
  transmit.perform();
}

void LgIrClimate::calc_checksum_(uint32_t &value) {
  uint32_t sum = 0;
  for (uint8_t i = 1; i < 8; i++) {
    sum += (value & (CHECKSUM_MASK << (i * 4))) >> (i * 4);
  }

  value |= (sum & CHECKSUM_MASK);
}

}  // namespace esphome::climate_ir_lg
