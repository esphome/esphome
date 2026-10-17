#include "bq76972.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::bq76972 {

ESPHOME_LOG_TAG(TAG, "bq76972");

static constexpr uint8_t BQ76972_REG_SUBCOMMAND = 0x3E;  // 0x3E low byte, 0x3F high byte
static constexpr uint8_t BQ76972_REG_TRANSFER_BUFFER = 0x40;
static constexpr uint8_t BQ76972_REG_CHECKSUM = 0x60;  // 0x60 checksum, 0x61 length
static constexpr uint8_t BQ76972_CMD_BAT_STATUS = 0x12;
static constexpr uint8_t BQ76972_CMD_CELL_START = 0x14;
static constexpr uint8_t BQ76972_CMD_STACK_VOLTAGE = 0x34;
static constexpr uint8_t BQ76972_CMD_TEMP_START = 0x68;
static constexpr uint16_t BQ76972_SUBCMD_ENTER_CFG_UPDATE = 0x0090;
static constexpr uint16_t BQ76972_SUBCMD_EXIT_CFG_UPDATE = 0x0092;
static constexpr uint16_t BQ76972_MEM_REG12_CONFIG = 0x9236;
static constexpr uint16_t BQ76972_MEM_REG0_CONFIG = 0x9237;
static constexpr uint16_t BQ76972_MEM_I2C_ADDRESS = 0x923A;
static constexpr uint16_t BQ76972_MEM_SWAP_COMM_MODE = 0x29BC;
// Pin config of temperature reading N (1 = CFETOFF ... 9 = DDSG) is at this base + N
static constexpr uint16_t BQ76972_MEM_TEMP_PIN_CONFIG_BASE = 0x92F9;
static constexpr size_t BQ76972_TS1_INDEX = 4;  // a thermistor input by default
static constexpr uint8_t BQ76972_PIN_CONFIG_THERMISTOR = 0x07;

// 16 words with a CRC byte after every data byte
static constexpr size_t BQ76972_MAX_TRANSFER = 64;
static constexpr uint8_t BQ76972_MAX_POLLS = 50;

static float raw_to_celsius(uint16_t raw) { return static_cast<int16_t>(raw) * 0.1f - 273.15f; }

bool BQ76972Component::read_block_(uint8_t start_register, uint8_t *data, size_t len) {
  if (!this->crc_mode_)
    return this->read_bytes(start_register, data, len);

  // With CRC enabled, every data byte is followed by a CRC byte
  std::array<uint8_t, BQ76972_MAX_TRANSFER> buffer;
  if (len == 0 || len * 2 > buffer.size())
    return false;
  if (!this->read_bytes(start_register, buffer.data(), len * 2))
    return false;

  // The first CRC also covers the write address, register and read address
  const uint8_t crc0_input[4] = {static_cast<uint8_t>(this->address_ << 1), start_register,
                                 static_cast<uint8_t>((this->address_ << 1) | 1), buffer[0]};
  if (crc8(crc0_input, 4, 0x00, 0x07, true) != buffer[1]) {
    ESP_LOGD(TAG, "CRC mismatch at register 0x%02X byte 0", start_register);
    return false;
  }
  data[0] = buffer[0];
  for (size_t i = 1; i < len; ++i) {
    if (crc8(&buffer[i * 2], 1, 0x00, 0x07, true) != buffer[i * 2 + 1]) {
      ESP_LOGD(TAG, "CRC mismatch at register 0x%02X byte %u", start_register, static_cast<unsigned>(i));
      return false;
    }
    data[i] = buffer[i * 2];
  }
  return true;
}

bool BQ76972Component::write_block_(uint8_t start_register, const uint8_t *data, size_t len) {
  if (!this->crc_mode_)
    return this->write_bytes(start_register, data, len);

  std::array<uint8_t, BQ76972_MAX_TRANSFER> buffer;
  if (len == 0 || len * 2 > buffer.size())
    return false;

  // The first CRC also covers the write address and register
  const uint8_t crc0_input[3] = {static_cast<uint8_t>(this->address_ << 1), start_register, data[0]};
  buffer[0] = data[0];
  buffer[1] = crc8(crc0_input, 3, 0x00, 0x07, true);
  for (size_t i = 1; i < len; ++i) {
    buffer[i * 2] = data[i];
    buffer[i * 2 + 1] = crc8(&data[i], 1, 0x00, 0x07, true);
  }
  return this->write_bytes(start_register, buffer.data(), len * 2);
}

bool BQ76972Component::read_words_(uint8_t start_register, uint16_t *data, size_t num_words) {
  std::array<uint8_t, BQ76972_MAX_TRANSFER / 2> bytes;
  if (num_words * 2 > bytes.size() || !this->read_block_(start_register, bytes.data(), num_words * 2))
    return false;
  for (size_t i = 0; i < num_words; i++)
    data[i] = encode_uint16(bytes[2 * i + 1], bytes[2 * i]);
  return true;
}

bool BQ76972Component::send_subcommand_(uint16_t subcommand) {
  const uint8_t bytes[2] = {static_cast<uint8_t>(subcommand & 0xFF), static_cast<uint8_t>(subcommand >> 8)};
  return this->write_block_(BQ76972_REG_SUBCOMMAND, bytes, 2);
}

bool BQ76972Component::wait_for_subcommand_() {
  uint8_t bytes[2];
  for (uint8_t i = 0; i < BQ76972_MAX_POLLS; i++) {
    if (!this->read_block_(BQ76972_REG_SUBCOMMAND, bytes, 2))
      return false;
    if (bytes[0] != 0xFF && bytes[1] != 0xFF)
      return true;
    delay(1);
  }
  return false;
}

bool BQ76972Component::wait_for_cfgupdate_() {
  uint16_t status;
  for (uint8_t i = 0; i < BQ76972_MAX_POLLS; i++) {
    if (!this->read_words_(BQ76972_CMD_BAT_STATUS, &status, 1))
      return false;
    if (status & 0x0001)
      return true;
    delay(1);
  }
  return false;
}

bool BQ76972Component::read_subcommand_(uint16_t subcommand, uint8_t *data, size_t len) {
  return this->send_subcommand_(subcommand) && this->wait_for_subcommand_() &&
         this->read_block_(BQ76972_REG_TRANSFER_BUFFER, data, len);
}

bool BQ76972Component::write_subcommand_(uint16_t subcommand, const uint8_t *data, size_t len) {
  if (!this->send_subcommand_(subcommand))
    return false;
  if (len == 0)
    return true;
  if (!this->write_block_(BQ76972_REG_TRANSFER_BUFFER, data, len))
    return false;

  // Inverted 8-bit sum of 0x3E, 0x3F and the data; length counts 0x3E, 0x3F, 0x60, 0x61 and the data
  uint8_t sum = static_cast<uint8_t>(subcommand & 0xFF) + static_cast<uint8_t>(subcommand >> 8);
  for (size_t i = 0; i < len; ++i)
    sum += data[i];
  const uint8_t tail[2] = {static_cast<uint8_t>(~sum), static_cast<uint8_t>(4 + len)};
  return this->write_block_(BQ76972_REG_CHECKSUM, tail, 2);
}

bool BQ76972Component::enter_cfg_update_() {
  if (!this->write_subcommand_(BQ76972_SUBCMD_ENTER_CFG_UPDATE, nullptr, 0))
    return false;
  delay(10);
  return this->wait_for_cfgupdate_();
}

bool BQ76972Component::exit_cfg_update_() {
  return this->write_subcommand_(BQ76972_SUBCMD_EXIT_CFG_UPDATE, nullptr, 0);
}

bool BQ76972Component::regs_disabled_(bool &out) {
  uint8_t reg12;
  uint8_t reg0;
  if (!this->read_subcommand_(BQ76972_MEM_REG12_CONFIG, &reg12, 1) ||
      !this->read_subcommand_(BQ76972_MEM_REG0_CONFIG, &reg0, 1))
    return false;
  out = reg12 == 0 && (reg0 & 0x01) == 0;
  return true;
}

const LogString *BQ76972Component::configure_() {
  uint16_t internal_temp;
  if (!this->read_words_(BQ76972_CMD_TEMP_START, &internal_temp, 1))
    return LOG_STR(ESP_LOG_MSG_COMM_FAIL);

  // Only write the regulator config when it differs, see
  // https://e2e.ti.com/support/power-management-group/power-management/f/power-management-forum/1583718/bq76972-undocumented-4-minute-config_update-auto-exit-timeout
  bool regs_disabled = true;
  if (this->reg_disable_ && !this->regs_disabled_(regs_disabled))
    return LOG_STR("Reading regulator config failed");
  const bool write_regs = !regs_disabled;

  bool write_thermistors = false;
#ifdef USE_SENSOR
  for (size_t i = 1; i < BQ76972_NUM_TEMPERATURES; i++) {
    if (i != BQ76972_TS1_INDEX && this->temperature_sensors_[i] != nullptr)
      write_thermistors = true;
  }
#endif

  if (write_regs || write_thermistors) {
    if (!this->enter_cfg_update_())
      return LOG_STR("Entering config update mode failed");
    const LogString *error = this->write_config_(write_regs);
    // The chip stops measuring in config update mode, so leave it even after a failed write
    if (!this->exit_cfg_update_() && error == nullptr)
      error = LOG_STR("Exiting config update mode failed");
    if (error != nullptr)
      return error;
    if (write_regs && (!this->regs_disabled_(regs_disabled) || !regs_disabled))
      return LOG_STR("Disabling regulators failed");
  }

  ESP_LOGD(TAG, "Internal temperature: %.1f°C", raw_to_celsius(internal_temp));
  return nullptr;
}

const LogString *BQ76972Component::write_config_(bool write_regs) {
  if (write_regs) {
    const uint8_t zero = 0;
    if (!this->write_subcommand_(BQ76972_MEM_REG12_CONFIG, &zero, 1) ||
        !this->write_subcommand_(BQ76972_MEM_REG0_CONFIG, &zero, 1))
      return LOG_STR("Writing regulator config failed");
  }
#ifdef USE_SENSOR
  for (size_t i = 1; i < BQ76972_NUM_TEMPERATURES; i++) {
    if (i == BQ76972_TS1_INDEX || this->temperature_sensors_[i] == nullptr)
      continue;
    if (!this->write_subcommand_(BQ76972_MEM_TEMP_PIN_CONFIG_BASE + i, &BQ76972_PIN_CONFIG_THERMISTOR, 1))
      return LOG_STR("Writing thermistor pin config failed");
  }
#endif
  return nullptr;
}

void BQ76972Component::setup() {
  if (const LogString *error = this->configure_(); error != nullptr)
    this->mark_failed(error);
}

void BQ76972Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BQ76972:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG,
                "  CRC Mode: %s\n"
                "  Regulators disabled: %s",
                this->crc_mode_ ? LOG_STR_LITERAL("Enabled") : LOG_STR_LITERAL("Disabled"), YESNO(this->reg_disable_));
}

void BQ76972Component::program_i2c_address(uint8_t address) {
  if (this->is_failed())
    return;
  const uint8_t shifted = address << 1;
  if (!this->enter_cfg_update_()) {
    this->status_set_warning(LOG_STR("Programming I2C address failed"));
    return;
  }
  // The new address only takes effect with the swap, so after a failed write the chip still answers at the
  // old one and must still be taken out of config update mode
  const bool written = this->write_subcommand_(BQ76972_MEM_I2C_ADDRESS, &shifted, 1) &&
                       this->write_subcommand_(BQ76972_MEM_SWAP_COMM_MODE, nullptr, 0);
  if (written) {
    delay(10);
    this->set_i2c_address(address);
  }
  if (!this->exit_cfg_update_() || !written) {
    this->status_set_warning(LOG_STR("Programming I2C address failed"));
    return;
  }
  ESP_LOGI(TAG, "I2C address set to 0x%02X", address);
}

#ifdef USE_SENSOR
template<size_t N> static bool any_set(const std::array<sensor::Sensor *, N> &sensors) {
  for (auto *sensor : sensors) {
    if (sensor != nullptr)
      return true;
  }
  return false;
}

void BQ76972Component::update() {
  // Each group publishes as soon as its read succeeds, so one failed read does not hold back the others
  bool ok = true;
  uint16_t words[BQ76972_NUM_CELLS];
  if (this->stack_voltage_sensor_ != nullptr) {
    if (this->read_words_(BQ76972_CMD_STACK_VOLTAGE, words, 1)) {
      if (words[0] != 0)
        this->stack_voltage_sensor_->publish_state(words[0] * 0.01f);  // 10 mV units
    } else {
      ok = false;
    }
  }
  if (any_set(this->cell_sensors_)) {
    if (this->read_words_(BQ76972_CMD_CELL_START, words, BQ76972_NUM_CELLS)) {
      for (size_t i = 0; i < BQ76972_NUM_CELLS; i++) {
        if (this->cell_sensors_[i] != nullptr && words[i] != 0)
          this->cell_sensors_[i]->publish_state(words[i] * 0.001f);  // mV
      }
    } else {
      ok = false;
    }
  }
  if (any_set(this->temperature_sensors_)) {
    if (this->read_words_(BQ76972_CMD_TEMP_START, words, BQ76972_NUM_TEMPERATURES)) {
      for (size_t i = 0; i < BQ76972_NUM_TEMPERATURES; i++) {
        if (this->temperature_sensors_[i] != nullptr)
          this->temperature_sensors_[i]->publish_state(raw_to_celsius(words[i]));
      }
    } else {
      ok = false;
    }
  }
  if (ok) {
    this->status_clear_warning();
  } else {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
  }
}

#else
void BQ76972Component::update() {}
#endif

}  // namespace esphome::bq76972
