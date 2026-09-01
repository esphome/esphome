#include "bq76972.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::bq76972 {

static const char *const TAG = "bq76972";
static const uint8_t BQ76972_SUBCOMMAND_LSB_REG = 0x3E;
static const uint8_t BQ76972_SUBCOMMAND_MSB_REG = 0x3F;
static const uint8_t BQ76972_CMD_BAT_STATUS = 0x12;
static const uint8_t BQ76972_GET_INT_TEMP = 0x68;
static const uint8_t BQ76972_CMD_TEMP_START = 0x68;
static const uint8_t BQ76972_CMD_CELL_START = 0x14;
static const uint8_t BQ76972_CMD_STACK_VOLTAGE = 0x34;
static const uint8_t BQ76972_SUBCMD_ENTER_CFG_UPDATE = 0x0090;
static const uint8_t BQ76972_SUBCMD_EXIT_CFG_UPDATE = 0x0092;
static const uint16_t BQ76972_MEM_REG0_CONFIG = 0x9237;
static const uint16_t BQ76972_MEM_REG12_CONFIG = 0x9236;
static const uint16_t BQ76972_MEM_I2C_ADDRESS = 0x923A;
static const uint16_t BQ76972_MEM_SWAP_COMM_MODE = 0x29BC;

struct ThermistorConfig {
  sensor::Sensor *sensor;
  uint16_t subcommand;
  const char *name;
};

uint8_t BQ76972Component::compute_crc8(const uint8_t *data, size_t len) {
  uint8_t crc = 0x00;  // Initial value is 0
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int j = 0; j < 8; ++j) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x07;  // Polynomial x^8 + x^2 + x + 1
      } else {
        crc = (crc << 1);
      }
    }
  }
  return crc;
}

bool BQ76972Component::wait_for_subcommand() {
  uint8_t reg_uint8;
  while (true) {
    if (!this->read_block(BQ76972_SUBCOMMAND_LSB_REG, &reg_uint8, 1))
      return false;
    if (reg_uint8 != 0xFF) {
      break;
    }
  }
  while (true) {
    if (!this->read_block(BQ76972_SUBCOMMAND_MSB_REG, &reg_uint8, 1))
      return false;
    if (reg_uint8 != 0xFF) {
      break;
    }
  }
  return true;
}

bool BQ76972Component::wait_for_cfgupdate() {
  uint16_t reg_uint16;
  while (true) {
    if (!this->bq76972_read_multi_16_le(BQ76972_CMD_BAT_STATUS, &reg_uint16, 1))
      return false;
    if ((reg_uint16 & 0x0001) == 0x0001) {
      break;
    }
  }
  return true;
}

bool BQ76972Component::read_block(uint8_t start_register, uint8_t *data, size_t len) {
  if (!this->crc_mode_) {
    return this->read_bytes(start_register, data, len);
  }

  // When CRC is enabled, every data byte is followed by a CRC byte
  std::vector<uint8_t> buffer(len * 2);

  if (!this->read_bytes(start_register, buffer.data(), len * 2)) {
    ESP_LOGE(TAG, "I2C block read failed at register 0x%02X", start_register);
    return false;
  }

  // Verify CRC for the FIRST data byte: Includes write address, register address, read address, and first data byte
  uint8_t crc0_input[4] = {
      static_cast<uint8_t>(this->i2c_address_ << 1),        // Write address
      start_register,                                       // Register address
      static_cast<uint8_t>((this->i2c_address_ << 1) | 1),  // Read address
      buffer[0]                                             // First data byte
  };

  if (this->compute_crc8(crc0_input, 4) != buffer[1]) {
    ESP_LOGE(TAG, "CRC mismatch on first byte of block read");
    return false;
  }
  data[0] = buffer[0];

  // Verify CRC for SUBSEQUENT data bytes: Calculated over the data byte only
  for (size_t i = 1; i < len; ++i) {
    uint8_t crc_sub[1] = {buffer[i * 2]};
    if (this->compute_crc8(crc_sub, 1) != buffer[i * 2 + 1]) {
      ESP_LOGE(TAG, "CRC mismatch on byte %d of block read", i);
      return false;
    }
    data[i] = buffer[i * 2];
  }

  return true;
}

bool BQ76972Component::write_block(uint8_t start_register, const uint8_t *data, size_t len) {
  if (!this->crc_mode_) {
    return this->write_bytes(start_register, data, len);
  }

  // When CRC is enabled, every data byte is immediately followed by its CRC
  std::vector<uint8_t> buffer(len * 2);

  // Calculate CRC for the FIRST data byte: Includes responder write address, register address, and first data byte
  uint8_t crc0_input[3] = {static_cast<uint8_t>(this->i2c_address_ << 1), start_register, data[0]};
  buffer[0] = data[0];
  buffer[1] = this->compute_crc8(crc0_input, 3);

  // Calculate CRC for SUBSEQUENT data bytes: Includes ONLY the data byte itself
  for (size_t i = 1; i < len; ++i) {
    buffer[i * 2] = data[i];
    uint8_t crc_sub[1] = {data[i]};
    buffer[i * 2 + 1] = this->compute_crc8(crc_sub, 1);
  }

  return this->write_bytes(start_register, buffer.data(), buffer.size());
}

bool BQ76972Component::bq76972_read_multi_16_le(uint8_t a_register, uint16_t *data, size_t num_words) {
  // destination and size sanity check
  if (data == nullptr || num_words == 0) {
    return false;
  }

  // With CRC: each uint16_t takes 4 bytes (Data0, CRC0, Data1, CRC1)
  // Without CRC: each uint16_t takes 2 bytes (Data0, Data1)
  size_t bytes_per_word = this->crc_mode_ ? 4 : 2;
  size_t total_bytes_to_read = num_words * bytes_per_word;

  // Use a dynamic buffer (vector) to cleanly handle variable length reads safely
  std::vector<uint8_t> buffer(total_bytes_to_read);

  if (!this->read_bytes(a_register, buffer.data(), total_bytes_to_read)) {
    return false;  // Read failed
  }

  if (this->crc_mode_) {
    for (size_t i = 0; i < num_words; i++) {
      size_t offset = i * 4;
      uint8_t data_lsb = buffer[offset + 0];
      uint8_t crc_lsb = buffer[offset + 1];
      uint8_t data_msb = buffer[offset + 2];
      uint8_t crc_msb = buffer[offset + 3];

      // 1. Check LSB CRC
      if (i == 0) {
        // The very first data byte CRC includes communication overhead addresses
        uint8_t crc0_input[4] = {
            static_cast<uint8_t>(this->i2c_address_ << 1),        // Write address
            a_register,                                           // Register address
            static_cast<uint8_t>((this->i2c_address_ << 1) | 1),  // Read address
            data_lsb                                              // First data byte (LSB)
        };
        uint8_t crc0_calc = this->compute_crc8(crc0_input, 4);
        if (crc0_calc != crc_lsb) {
          ESP_LOGE(TAG, "CRC error on Word %d LSB. Expected 0x%02X, got 0x%02X", i, crc0_calc, crc_lsb);
          return false;
        }
      } else {
        // Subsequent sequential LSB bytes compute CRC only over the data byte itself
        uint8_t crc_input[1] = {data_lsb};
        uint8_t crc_calc = this->compute_crc8(crc_input, 1);
        if (crc_calc != crc_lsb) {
          ESP_LOGE(TAG, "CRC error on Word %d LSB. Expected 0x%02X, got 0x%02X", i, crc_calc, crc_lsb);
          return false;
        }
      }

      // 2. Check MSB CRC (Always calculated over the MSB data byte only)
      uint8_t crc1_input[1] = {data_msb};
      uint8_t crc1_calc = this->compute_crc8(crc1_input, 1);
      if (crc1_calc != crc_msb) {
        ESP_LOGE(TAG, "CRC error on Word %d MSB. Expected 0x%02X, got 0x%02X", i, crc1_calc, crc_msb);
        return false;
      }

      // Store reconstituted value
      data[i] = (static_cast<uint16_t>(data_msb) << 8) | data_lsb;
    }
  } else {
    // Reconstitute words cleanly without CRC parsing
    for (size_t i = 0; i < num_words; i++) {
      size_t offset = i * 2;
      data[i] = (static_cast<uint16_t>(buffer[offset + 1]) << 8) | buffer[offset + 0];
    }
  }

  return true;
}

bool BQ76972Component::read_subcommand(uint16_t subcommand, uint8_t *data, size_t expected_len) {
  // Write the 16-bit subcommand address to 0x3E (lower) and 0x3F (upper)
  uint8_t sub_byte_l[1] = {
      static_cast<uint8_t>(subcommand & 0xFF),
  };
  uint8_t sub_byte_h[1] = {static_cast<uint8_t>((subcommand >> 8) & 0xFF)};

  if (!this->write_block(BQ76972_SUBCOMMAND_LSB_REG, sub_byte_l, 1))
    return false;
  if (!this->write_block(BQ76972_SUBCOMMAND_MSB_REG, sub_byte_h, 1))
    return false;

  // Wait for the device to process the command and populate the buffer.
  if (!this->wait_for_subcommand())
    return false;

  // Read the populated data from the transfer buffer starting at 0x40
  if (!this->read_block(0x40, data, expected_len))
    return false;

  return true;
}

bool BQ76972Component::write_subcommand(uint16_t subcommand, const uint8_t *data, size_t len) {
  // Write the 16-bit subcommand address to 0x3E (lower) and 0x3F (upper)
  uint8_t sub_byte_l[1] = {
      static_cast<uint8_t>(subcommand & 0xFF),
  };
  uint8_t sub_byte_h[1] = {static_cast<uint8_t>((subcommand >> 8) & 0xFF)};

  if (!this->write_block(BQ76972_SUBCOMMAND_LSB_REG, sub_byte_l, 1))
    return false;
  if (!this->write_block(BQ76972_SUBCOMMAND_MSB_REG, sub_byte_h, 1))
    return false;

  // Write the data into the transfer buffer starting at 0x40
  if (len > 0) {
    if (!this->write_block(0x40, data, len))
      return false;

    // Calculate checksum: 8-bit sum of 0x3E, 0x3F, and buffer data, then bitwise inverted
    uint8_t sum = sub_byte_l[0] + sub_byte_h[0];
    for (size_t i = 0; i < len; ++i) {
      sum += data[i];
    }
    uint8_t checksum = ~sum;

    // Calculate length: 4 (0x3E, 0x3F, 0x60, 0x61) + len of data
    uint8_t data_length = 4 + len;

    // Checksum and length must be written together as a word to 0x60 and 0x61
    uint8_t tail[2] = {checksum, data_length};
    if (!this->write_block(0x60, tail, 2))
      return false;
  }

  return true;
}

bool BQ76972Component::store_int_temp() {
  uint16_t internal_temp;
  if (!this->bq76972_read_multi_16_le(BQ76972_GET_INT_TEMP, &internal_temp, 1)) {
    return false;
  }

  // Internal temperature extraction
  int16_t raw_temp = static_cast<int16_t>(internal_temp);
  this->internal_temp_c_ = (raw_temp * 0.1f) - 273.15f;
  this->internal_temp_f_ = (internal_temp_c_ * 9.0f / 5.0f) + 32.0f;

  return true;
}

void BQ76972Component::setup() {
  // Read internal temperature to check comms
  if (!this->store_int_temp()) {
    ESP_LOGE(TAG, "%s: failed to communicate over I2C", this->component_id_.c_str());
    this->mark_failed();
    return;
  }

  if (this->address_number_ != nullptr) {
    this->address_number_->publish_state(this->address_);
  }

  // Disable regulators if needed
  if (this->reg_disable_) {
    // See
    // https://e2e.ti.com/support/power-management-group/power-management/f/power-management-forum/1583718/bq76972-undocumented-4-minute-config_update-auto-exit-timeout
    bool reg_already_disabled = true;

    // Check reg12 initial configuration
    uint8_t reg_uint8;
    if (!this->read_subcommand(BQ76972_MEM_REG12_CONFIG, &reg_uint8, 1)) {
      ESP_LOGE(TAG, "%s: couldn't read reg12 config", this->component_id_.c_str());
      this->mark_failed();
      return;
    }
    if (reg_uint8 != 0)
      reg_already_disabled = false;

    // Check reg0 initial configuration
    if (!this->read_subcommand(BQ76972_MEM_REG0_CONFIG, &reg_uint8, 1)) {
      ESP_LOGE(TAG, "%s: couldn't read reg0 config", this->component_id_.c_str());
      this->mark_failed();
      return;
    }
    if ((reg_uint8 & 0x01) != 0)
      reg_already_disabled = false;

    if (!reg_already_disabled) {
      // Enter CONFIG_UPDATE mode
      if (!this->write_subcommand(BQ76972_SUBCMD_ENTER_CFG_UPDATE, nullptr, 0)) {
        ESP_LOGE(TAG, "%s: couldn't enter config update mode", this->component_id_.c_str());
        this->mark_failed();
        return;
      }
      delay(10);

      // Wait for config updatemode
      if (!this->wait_for_cfgupdate()) {
        ESP_LOGE(TAG, "%s: couldn't wait for update mode", this->component_id_.c_str());
        this->mark_failed();
        return;
      }

      // Send new configuration with reg1&2 disabled
      uint8_t reg12_data[1] = {0x00};
      if (!this->write_subcommand(BQ76972_MEM_REG12_CONFIG, reg12_data, 1)) {
        ESP_LOGE(TAG, "%s: couldn't set reg12 regiser", this->component_id_.c_str());
        this->mark_failed();
        return;
      }

      // Send new configuration with reg0 disabled
      uint8_t reg0_data[1] = {0x00};
      if (!this->write_subcommand(BQ76972_MEM_REG0_CONFIG, reg0_data, 1)) {
        ESP_LOGE(TAG, "%s: couldn't set reg0 regiser", this->component_id_.c_str());
        this->mark_failed();
        return;
      }

      // Exit CONFIG_UPDATE mode
      if (!this->write_subcommand(BQ76972_SUBCMD_EXIT_CFG_UPDATE, nullptr, 0)) {
        ESP_LOGE(TAG, "%s: couldn't exit config update mode", this->component_id_.c_str());
        this->mark_failed();
        return;
      }

      // Check success: reg12
      if (!this->read_subcommand(BQ76972_MEM_REG12_CONFIG, &reg_uint8, 1) || (reg_uint8 != 0)) {
        ESP_LOGE(TAG, "%s: couldn't disable reg12", this->component_id_.c_str());
        this->mark_failed();
        return;
      }

      // Check success
      if (!this->read_subcommand(BQ76972_MEM_REG0_CONFIG, &reg_uint8, 1) || ((reg_uint8 & 0x01) != 0)) {
        ESP_LOGE(TAG, "%s: couldn't disable pre-regulator", this->component_id_.c_str());
        this->mark_failed();
        return;
      }
    }
  }

  // Thermistor configuration
  uint8_t register_for_thermistor_mes = 0x07;
  const ThermistorConfig thermistors[] = {
      {this->ts2_temp_sensor_, 0x92FE, "TS2"},         {this->ts3_temp_sensor_, 0x92FF, "TS3"},
      {this->hdq_temp_sensor_, 0x9300, "HDQ"},         {this->dchg_temp_sensor_, 0x9301, "DCHG"},
      {this->ddsg_temp_sensor_, 0x9302, "DDSG"},       {this->cfetoff_temp_sensor_, 0x92FA, "CFETOFF"},
      {this->dfetoff_temp_sensor_, 0x92FB, "DFETOFF"}, {this->alert_temp_sensor_, 0x92FC, "ALERT"},
  };

  // Enter CONFIG_UPDATE mode
  if (!this->write_subcommand(BQ76972_SUBCMD_ENTER_CFG_UPDATE, nullptr, 0)) {
    ESP_LOGE(TAG, "%s: couldn't enter config update mode", this->component_id_.c_str());
    this->mark_failed();
    return;
  }
  delay(10);

  // Wait for config updatemode
  if (!this->wait_for_cfgupdate()) {
    ESP_LOGE(TAG, "%s: couldn't wait for update mode", this->component_id_.c_str());
    this->mark_failed();
    return;
  }

  // Program registers if needed
  for (const auto &t : thermistors) {
    if (t.sensor != nullptr) {
      if (!this->write_subcommand(t.subcommand, &register_for_thermistor_mes, 1)) {
        ESP_LOGE(TAG, "%s: couldn't program %s pin input for thermistor", this->component_id_.c_str(), t.name);
        this->mark_failed();
        return;
      }
    }
  }

  // Exit CONFIG_UPDATE mode
  if (!this->write_subcommand(BQ76972_SUBCMD_EXIT_CFG_UPDATE, nullptr, 0)) {
    ESP_LOGE(TAG, "%s: couldn't exit config update mode", this->component_id_.c_str());
    this->mark_failed();
    return;
  }

  // Delayed debug info
  this->set_timeout(7000, [this]() {
    ESP_LOGI(TAG, "%s: Init completed without issues", this->component_id_.c_str());
    ESP_LOGI(TAG, "%s: Internal temperature of %.2f°C, %.2f°F", this->component_id_.c_str(), this->internal_temp_c_,
             this->internal_temp_f_);
  });
}

void BQ76972Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BQ76972 (ID: %s):", this->component_id_.c_str());
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  CRC Mode: %s", this->crc_mode_ ? "Enabled" : "Disabled");
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

void BQ76972Component::program_address_from_number() {
  if (this->address_number_ != nullptr) {
    // Enter CONFIG_UPDATE mode
    if (!this->write_subcommand(BQ76972_SUBCMD_ENTER_CFG_UPDATE, nullptr, 0)) {
      ESP_LOGE(TAG, "%s: Couldn't enter config update mode", this->component_id_.c_str());
      return;
    }
    delay(10);

    // Wait for config updatemode
    if (!this->wait_for_cfgupdate()) {
      ESP_LOGE(TAG, "%s: Couldn't wait for update mode", this->component_id_.c_str());
      return;
    }

    // Program new I²C address
    uint8_t new_i2c_address_for_bq = (static_cast<uint8_t>(this->address_number_->state) << 1);
    if (!this->write_subcommand(BQ76972_MEM_I2C_ADDRESS, &new_i2c_address_for_bq, 1)) {
      ESP_LOGE(TAG, "%s: Couldn't program new I2C address", this->component_id_.c_str());
      return;
    }

    // Issue swap comm mode
    if (!this->write_subcommand(BQ76972_MEM_SWAP_COMM_MODE, nullptr, 0)) {
      ESP_LOGE(TAG, "%s: Couldn't issue swap comm mode", this->component_id_.c_str());
      return;
    }
    delay(10);

    // Update our I²C internal logic
    this->i2c_address_ = static_cast<uint8_t>(this->address_number_->state);
    this->set_i2c_address(this->i2c_address_);

    // Exit CONFIG_UPDATE mode
    if (!this->write_subcommand(BQ76972_SUBCMD_EXIT_CFG_UPDATE, nullptr, 0)) {
      ESP_LOGE(TAG, "%s: Couldn't exit config update mode", this->component_id_.c_str());
      return;
    }
  }
}

void BQ76972Component::publish_temperature_from_buffer(uint16_t temperature_uint16t, sensor::Sensor *sens) {
  if (sens == nullptr) {
    return;  // Skip sensors that were not declared in YAML
  }

  // Combine Little-Endian byte pair into a signed 16-bit raw integer
  int16_t raw_signed = static_cast<int16_t>(temperature_uint16t);

  // Convert raw value (0.1 Kelvin) to Celsius: (Raw / 10.0) - 273.15
  float temp_celsius = (raw_signed / 10.0f) - 273.15f;

  sens->publish_state(temp_celsius);
}

void BQ76972Component::update() {
  // Stack Voltage (1 LSB = 10mV -> convert to Volts)
  uint16_t raw_stack;
  if (!this->bq76972_read_multi_16_le(BQ76972_CMD_STACK_VOLTAGE, &raw_stack, 1)) {
    ESP_LOGE(TAG, "%s: Couldn't fetch stack voltage", this->component_id_.c_str());
    return;
  }
  this->pack_voltage_mv_ = raw_stack * 10;
  if (this->stack_voltage_sensor_ != nullptr && raw_stack != 0) {
    this->stack_voltage_sensor_->publish_state((float) raw_stack * 0.01f);
  }

  // Sequential Block Read for all 16 cell voltages
  uint16_t cell_buffer[16];

  // Issue a single I2C request starting from Cell 1's memory address
  if (!this->bq76972_read_multi_16_le(BQ76972_CMD_CELL_START, cell_buffer, 16)) {
    ESP_LOGE(TAG, "%s: Couldn't fetch cell voltages", this->component_id_.c_str());
    return;
  }

  // Parse the retrieved block from local memory
  for (size_t i = 0; i < 16; i++) {
    this->cell_voltages_mv_[i] = cell_buffer[i];
    if (this->cell_sensors_[i] == nullptr)
      continue;

    if (cell_buffer[i] != 0) {
      this->cell_sensors_[i]->publish_state((float) cell_buffer[i] * 0.001f);
    }
  }

  // Sequential Block Read for all 10 temperature sensors
  uint16_t temperature_buffer[10];

  // Issue a single I2C request
  if (!this->bq76972_read_multi_16_le(BQ76972_CMD_TEMP_START, temperature_buffer, 10)) {
    ESP_LOGE(TAG, "%s: Couldn't fetch temperatures", this->component_id_.c_str());
    return;
  }

  // Publish state if needed
  this->publish_temperature_from_buffer(temperature_buffer[0], this->internal_temp_sensor_);
  this->publish_temperature_from_buffer(temperature_buffer[1], this->cfetoff_temp_sensor_);
  this->publish_temperature_from_buffer(temperature_buffer[2], this->dfetoff_temp_sensor_);
  this->publish_temperature_from_buffer(temperature_buffer[3], this->alert_temp_sensor_);
  this->publish_temperature_from_buffer(temperature_buffer[4], this->ts1_temp_sensor_);
  this->publish_temperature_from_buffer(temperature_buffer[5], this->ts2_temp_sensor_);
  this->publish_temperature_from_buffer(temperature_buffer[6], this->ts3_temp_sensor_);
  this->publish_temperature_from_buffer(temperature_buffer[7], this->hdq_temp_sensor_);
  this->publish_temperature_from_buffer(temperature_buffer[8], this->dchg_temp_sensor_);
  this->publish_temperature_from_buffer(temperature_buffer[9], this->ddsg_temp_sensor_);
}

}  // namespace esphome::bq76972
