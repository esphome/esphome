#include "ds248x.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ds248x {
static const uint8_t DS248X_COMMAND_RESET        = 0xF0;
static const uint8_t DS248X_COMMAND_SETREADPTR   = 0xE1;
static const uint8_t DS248X_COMMAND_WRITECONFIG  = 0xD2;
static const uint8_t DS248X_COMMAND_RESETWIRE    = 0xB4;
static const uint8_t DS248X_COMMAND_WRITEBYTE    = 0xA5;
static const uint8_t DS248X_COMMAND_READBYTE     = 0x96;
static const uint8_t DS248X_COMMAND_SINGLEBIT    = 0x87;
static const uint8_t DS248X_COMMAND_TRIPLET      = 0x78;

static const uint8_t DS248X_POINTER_STATUS       = 0xF0;
static const uint8_t DS248X_STATUS_BUSY          = (1 << 0);
static const uint8_t DS248X_STATUS_PPD           = (1 << 1);
static const uint8_t DS248X_STATUS_SD            = (1 << 2);
static const uint8_t DS248X_STATUS_LL            = (1 << 3);
static const uint8_t DS248X_STATUS_RST           = (1 << 4);
static const uint8_t DS248X_STATUS_SBR           = (1 << 5);
static const uint8_t DS248X_STATUS_TSB           = (1 << 6);
static const uint8_t DS248X_STATUS_DIR           = (1 << 7);

static const uint8_t DS248X_POINTER_DATA         = 0xE1;

static const uint8_t DS248X_POINTER_CONFIG       = 0xC3;
static const uint8_t DS248X_CONFIG_ACTIVE_PULLUP = (1 << 0);
static const uint8_t DS248X_CONFIG_POWER_DOWN    = (1 << 1);
static const uint8_t DS248X_CONFIG_STRONG_PULLUP = (1 << 2);
static const uint8_t DS248X_CONFIG_1WIRE_SPEED   = (1 << 3);

static const uint8_t WIRE_COMMAND_SKIP           = 0xCC;
static const uint8_t WIRE_COMMAND_SELECT         = 0x55;
static const uint8_t WIRE_COMMAND_SEARCH         = 0xF0;

static const uint8_t DS248X_ERROR_TIMEOUT        = (1 << 0);
static const uint8_t DS248X_ERROR_SHORT          = (1 << 1);
static const uint8_t DS248X_ERROR_CONFIG         = (1 << 2);

static const uint8_t DALLAS_MODEL_DS18S20        = 0x10;
static const uint8_t DALLAS_MODEL_DS1822         = 0x22;
static const uint8_t DALLAS_MODEL_DS18B20        = 0x28;
static const uint8_t DALLAS_MODEL_DS1825         = 0x3B;
static const uint8_t DALLAS_MODEL_DS28EA00       = 0x42;

static const uint8_t DALLAS_COMMAND_START_CONVERSION  = 0x44;
static const uint8_t DALLAS_COMMAND_READ_SCRATCH_PAD  = 0xBE;
static const uint8_t DALLAS_COMMAND_WRITE_SCRATCH_PAD = 0x4E;
static const uint8_t DALLAS_COMMAND_SAVE_EEPROM       = 0x48;

static const char *const TAG = "ds248x";


void DS248xComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DS248x...");

  if (this->sleep_pin_) {
    this->sleep_pin_->setup();
    this->sleep_pin_->pin_mode(esphome::gpio::FLAG_OUTPUT);
  }

  this->reset_hub();
  uint64_t address = 0;
  std::vector<uint64_t> raw_sensors;
  while(this->search(&address)) {
    raw_sensors.push_back(address);
  }

  for (auto &address : raw_sensors) {
    auto *address8 = reinterpret_cast<uint8_t *>(&address);
    if (crc8(address8, 7) != address8[7]) {
      ESP_LOGW(TAG, "Dallas device 0x%s has invalid CRC.", format_hex(address).c_str());
      continue;
    }
    if (address8[0] != DALLAS_MODEL_DS18S20 && address8[0] != DALLAS_MODEL_DS1822 &&
        address8[0] != DALLAS_MODEL_DS18B20 && address8[0] != DALLAS_MODEL_DS1825 &&
        address8[0] != DALLAS_MODEL_DS28EA00) {
      ESP_LOGW(TAG, "Unknown device type 0x%02X.", address8[0]);
      continue;
    }
    this->found_sensors_.push_back(address);
  }

  for (auto *sensor : this->sensors_) {
    if (sensor->get_index().has_value()) {
      if (*sensor->get_index() >= this->found_sensors_.size()) {
        this->status_set_error();
        continue;
      }
      sensor->set_address(this->found_sensors_[*sensor->get_index()]);
    }

    if (!sensor->setup_sensor()) {
      this->status_set_error();
    }
  }
}

void DS248xComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DS248x:");
  if (this->sleep_pin_) {
    LOG_PIN("  Sleep Pin: ", this->sleep_pin_);
  }
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with DS248x failed!");
  }
  if (this->found_sensors_.empty()) {
    ESP_LOGW(TAG, "  Found no sensors!");
  } else {
    ESP_LOGD(TAG, "  Found sensors:");
    for (auto &address : this->found_sensors_) {
      ESP_LOGD(TAG, "    0x%s", format_hex(address).c_str());
    }
  }
  LOG_UPDATE_INTERVAL(this);

  for (auto *sensor : this->sensors_) {
    LOG_SENSOR("  ", "Device", sensor);
    if (sensor->get_index().has_value()) {
      ESP_LOGCONFIG(TAG, "    Index %u", *sensor->get_index());
      if (*sensor->get_index() >= this->found_sensors_.size()) {
        ESP_LOGE(TAG, "Couldn't find sensor by index - not connected. Proceeding without it.");
        continue;
      }
    }
    ESP_LOGCONFIG(TAG, "    Address: %s", sensor->get_address_name().c_str());
    ESP_LOGCONFIG(TAG, "    Resolution: %u", sensor->get_resolution());
  }
}

void DS248xComponent::register_sensor(DS248xTemperatureSensor *sensor) { this->sensors_.push_back(sensor); }

void DS248xComponent::update() {
  this->status_clear_warning();

  if (this->enable_bus_sleep_) {
    this->write_config(this->read_config() & ~DS248X_CONFIG_POWER_DOWN);
  }

  bool result = this->reset_devices();
  if (!result) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Reset failed");
    return;
  }

  this->write_to_wire(WIRE_COMMAND_SKIP);
  this->write_to_wire(DALLAS_COMMAND_START_CONVERSION);

  if (this->enable_strong_pullup_) {
    this->write_config(this->read_config() | DS248X_CONFIG_STRONG_PULLUP);
  }

  uint16_t max_wait_time = 0;

  for (auto *sensor : this->sensors_) {
    max_wait_time += sensor->millis_to_wait_for_conversion();
  }

  this->set_timeout(TAG, max_wait_time, [this] {
    if (this->enable_strong_pullup_) {
      this->write_config(this->read_config() & ~DS248X_CONFIG_STRONG_PULLUP);
    }
    for (auto *sensor : this->sensors_) {
      bool res = sensor->read_scratch_pad();

      if (!res) {
        ESP_LOGW(TAG, "'%s' - Resetting bus for read failed!", sensor->get_name().c_str());
        sensor->publish_state(NAN);
        this->status_set_warning();
        return;
      }
      if (!sensor->check_scratch_pad()) {
        sensor->publish_state(NAN);
        this->status_set_warning();
        return;
      }

      float tempc = sensor->get_temp_c();
      ESP_LOGD(TAG, "'%s': Got Temperature=%.1f°C", sensor->get_name().c_str(), tempc);
      sensor->publish_state(tempc);
    }

    if (this->enable_bus_sleep_) {
      this->write_config(this->read_config() | DS248X_CONFIG_POWER_DOWN);
    }
  });
}

float DS248xComponent::get_setup_priority() const { return setup_priority::DATA; }

uint8_t DS248xComponent::read_config() {
  std::array<uint8_t, 2> cmd;
  cmd[0] = DS248X_COMMAND_SETREADPTR;
  cmd[1] = DS248X_POINTER_CONFIG;
  this->write(cmd.data(), sizeof(cmd));

  uint8_t cfg_byte;
  this->read(&cfg_byte, sizeof(cfg_byte));

  return cfg_byte;
}

bool DS248xComponent::write_config(uint8_t cfg) {
  std::array<uint8_t, 2> cmd;
  cmd[0] = DS248X_COMMAND_WRITECONFIG;
  cmd[1] = cfg | ((~cfg) << 4);
  this->write(cmd.data(), sizeof(cmd));

  uint8_t cfg_byte;
  this->read(&cfg_byte, sizeof(cfg_byte));

  return cfg_byte == cfg;
}

uint8_t DS248xComponent::wait_while_busy() {
  std::array<uint8_t, 2> cmd;
  cmd[0] = DS248X_COMMAND_SETREADPTR;
  cmd[1] = DS248X_POINTER_STATUS;
  this->write(cmd.data(), sizeof(cmd));

  uint8_t status;
	for(int i=1000; i>0; i--) {
      this->read(&status, sizeof(status));
		if (!(status & DS248X_STATUS_BUSY))
			break;
	}
	return status;
}


void DS248xComponent::reset_hub() {
  if (this->sleep_pin_) {
    this->sleep_pin_->digital_write(true);
  }

  uint8_t cmd = DS248X_COMMAND_RESET;
  auto result = this->write(&cmd, sizeof(cmd));

  if (this->enable_active_pullup_) {
    this->write_config(DS248X_CONFIG_ACTIVE_PULLUP);
  }

  last_device_found = false;
  searchAddress = 0;
  searchLastDiscrepancy = 0;
}


bool DS248xComponent::reset_devices() {
  auto status = wait_while_busy();
  if (status & DS248X_STATUS_BUSY) {
    ESP_LOGE(TAG, "Master never finished command");
    return false;
  }
  uint8_t config = read_config();
  if (config & DS248X_CONFIG_STRONG_PULLUP) {
    bool cfg_ok = write_config(config & ~DS248X_CONFIG_STRONG_PULLUP);

    if (!cfg_ok) {
      ESP_LOGE(TAG, "Master config write failed");
      return false;
    }
  }

  uint8_t cmd = DS248X_COMMAND_RESETWIRE;
  auto err = this->write(&cmd, sizeof(cmd));
  if (err != esphome::i2c::ERROR_OK) {
      ESP_LOGE(TAG, "Resetwire write failed %i", err);
      return false;
  }

  status = wait_while_busy();

  if (status & DS248X_STATUS_BUSY) {
    ESP_LOGE(TAG, "Master never finished command");
    return false;
  }
  if (status & DS248X_STATUS_SD) {
    ESP_LOGE(TAG, "Bus is shorted");
    return false;
  }

  if (config & DS248X_CONFIG_STRONG_PULLUP) {
    bool cfg_ok = write_config(config);

    if (!cfg_ok) {
      ESP_LOGE(TAG, "Master config write failed");
      return false;
    }
  }
  return true;
}

void DS248xComponent::write_command(uint8_t command, uint8_t data) {
  auto status = wait_while_busy();

  if (status & DS248X_STATUS_BUSY) {
    return; // TODO: error handling
  }

  std::array<uint8_t, 2> cmd;
  cmd[0] = command;
  cmd[1] = data;
  this->write(cmd.data(), sizeof(cmd));
}

void DS248xComponent::select(uint64_t address) {
  this->write_command(DS248X_COMMAND_WRITEBYTE, WIRE_COMMAND_SELECT);

  for (int i = 0; i < 8; i++) {
    this->write_command(DS248X_COMMAND_WRITEBYTE, (address >> (i*8)) & 0xff);
  }
}

void DS248xComponent::write_to_wire(uint8_t data) {
  this->write_command(DS248X_COMMAND_WRITEBYTE, data);
}

uint8_t DS248xComponent::read_from_wire() {
  auto status = wait_while_busy();

  if (status & DS248X_STATUS_BUSY) {
    return 0; // TODO: error handling
  }

  uint8_t command = DS248X_COMMAND_READBYTE;
  this->write(&command, sizeof(command));

  status = wait_while_busy();

  if (status & DS248X_STATUS_BUSY) {
    return 0; // TODO: error handling
  }

  std::array<uint8_t, 2> cmd;
  cmd[0] = DS248X_COMMAND_SETREADPTR;
  cmd[1] = DS248X_POINTER_DATA;
  this->write(cmd.data(), sizeof(cmd));

  uint8_t data_byte;
  this->read(&data_byte, sizeof(data_byte));

  return data_byte;
}

bool DS248xComponent::search(uint64_t* address) {

  if (last_device_found)
    return false;

  bool result = this->reset_devices();
  if (!result) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Reset failed");
    return false;
  }

  write_to_wire(WIRE_COMMAND_SEARCH);

  uint8_t direction;
  uint8_t last_zero = 0;
  for(uint8_t i=0;i<64;i++) {
		uint64_t searchBit = 1LL << i;

		if (i < searchLastDiscrepancy)
			direction = (searchAddress & searchBit) != 0;
		else
			direction = i == searchLastDiscrepancy;

    write_command(DS248X_COMMAND_TRIPLET, direction ? 0x80 : 0x00);

    uint8_t status = wait_while_busy();
    ESP_LOGD(TAG, "Search: i: %i dir: %i, status: %i bit: %llu", i, direction, status, searchBit);

		uint8_t id = status & DS248X_STATUS_SBR;
		uint8_t comp_id = status & DS248X_STATUS_TSB;
		direction = status & DS248X_STATUS_DIR;
    

		if (id && comp_id)
			return 0;
		else
			if (!id && !comp_id && !direction)
				last_zero = i;

		if (direction)
			searchAddress |= searchBit;
		else
			searchAddress &= ~searchBit;

	}

	searchLastDiscrepancy = last_zero;

	if (!last_zero)
		last_device_found = true;

	*address = searchAddress;

	return 1;
}

void DS248xTemperatureSensor::set_address(uint64_t address) { this->address_ = address; }
uint8_t DS248xTemperatureSensor::get_resolution() const { return this->resolution_; }
void DS248xTemperatureSensor::set_resolution(uint8_t resolution) { this->resolution_ = resolution; }
optional<uint8_t> DS248xTemperatureSensor::get_index() const { return this->index_; }
void DS248xTemperatureSensor::set_index(uint8_t index) { this->index_ = index; }
uint8_t *DS248xTemperatureSensor::get_address8() { return reinterpret_cast<uint8_t *>(&this->address_); }
const std::string &DS248xTemperatureSensor::get_address_name() {
  if (this->address_name_.empty()) {
    this->address_name_ = std::string("0x") + format_hex(this->address_);
  }

  return this->address_name_;
}

uint16_t DS248xTemperatureSensor::millis_to_wait_for_conversion() const {
  switch (this->resolution_) {
    case 9:
      return 94;
    case 10:
      return 188;
    case 11:
      return 375;
    default:
      return 750;
  }
}

bool IRAM_ATTR DS248xTemperatureSensor::read_scratch_pad() {
  bool result = this->parent_->reset_devices();
  if (!result) {
    this->parent_->status_set_warning();
    ESP_LOGE(TAG, "Reset failed");
    return false;
  }

  this->parent_->select(this->address_);
  this->parent_->write_to_wire(DALLAS_COMMAND_READ_SCRATCH_PAD);

  for (uint8_t &i : this->scratch_pad_) {
    i = this->parent_->read_from_wire();
  }

  return true;
}

bool DS248xTemperatureSensor::setup_sensor() {
  bool r = this->read_scratch_pad();

  if (!r) {
    ESP_LOGE(TAG, "Reading scratchpad failed");
    return false;
  }
  if (!this->check_scratch_pad())
    return false;

  uint8_t resolution_register_val;
  switch (this->resolution_) {
    case 12:
      resolution_register_val = 0x7F;
      break;
    case 11:
      resolution_register_val = 0x5F;
      break;
    case 10:
      resolution_register_val = 0x3F;
      break;
    case 9:
    default:
      resolution_register_val = 0x1F;
      break;
  }


  if (this->scratch_pad_[4] == resolution_register_val)
    return true;

  if (this->get_address8()[0] == DALLAS_MODEL_DS18S20) {
    // DS18S20 doesn't support resolution.
    ESP_LOGW(TAG, "DS18S20 doesn't support setting resolution.");
    return false;
  }


  bool result = this->parent_->reset_devices();
  if (!result) {
    ESP_LOGE(TAG, "Reset failed");
    return false;
  }

  this->parent_->select(this->address_);
  this->parent_->write_to_wire(DALLAS_COMMAND_WRITE_SCRATCH_PAD);
  this->parent_->write_to_wire(this->scratch_pad_[2]);  // high alarm temp
  this->parent_->write_to_wire(this->scratch_pad_[3]);  // low alarm temp
  this->parent_->write_to_wire(this->scratch_pad_[4]);  // resolution
  
  result = this->parent_->reset_devices();
  if (!result) {
    ESP_LOGE(TAG, "Reset failed");
    return false;
  }

  this->parent_->select(this->address_);
  this->parent_->write_to_wire(DALLAS_COMMAND_SAVE_EEPROM);

  delay(20);  // allow it to finish operation

  result = this->parent_->reset_devices();
  if (!result) {
    ESP_LOGE(TAG, "Reset failed");
    return false;
  }
  return true;
}

bool DS248xTemperatureSensor::check_scratch_pad() {
  bool chksum_validity = (crc8(this->scratch_pad_, 8) == this->scratch_pad_[8]);
  bool config_validity = false;

  switch (this->get_address8()[0]) {
    case DALLAS_MODEL_DS18B20:
      config_validity = ((this->scratch_pad_[4] & 0x9F) == 0x1F);
      break;
    default:
      config_validity = ((this->scratch_pad_[4] & 0x10) == 0x10);
  }

#ifdef ESPHOME_LOG_LEVEL_VERY_VERBOSE
  ESP_LOGVV(TAG, "Scratch pad: %02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X (%02X)", this->scratch_pad_[0],
            this->scratch_pad_[1], this->scratch_pad_[2], this->scratch_pad_[3], this->scratch_pad_[4],
            this->scratch_pad_[5], this->scratch_pad_[6], this->scratch_pad_[7], this->scratch_pad_[8],
            crc8(this->scratch_pad_, 8));
#endif
  if (!chksum_validity) {
    ESP_LOGW(TAG, "'%s' - Scratch pad checksum invalid!", this->get_name().c_str());
  } else if (!config_validity) {
    ESP_LOGW(TAG, "'%s' - Scratch pad config register invalid!", this->get_name().c_str());
  }
  return chksum_validity && config_validity;
}
float DS248xTemperatureSensor::get_temp_c() {
  int16_t temp = (int16_t(this->scratch_pad_[1]) << 11) | (int16_t(this->scratch_pad_[0]) << 3);
  if (this->get_address8()[0] == DALLAS_MODEL_DS18S20) {
    int diff = (this->scratch_pad_[7] - this->scratch_pad_[6]) << 7;
    temp = ((temp & 0xFFF0) << 3) - 16 + (diff / this->scratch_pad_[7]);
  }

  return temp / 128.0f;
}
std::string DS248xTemperatureSensor::unique_id() { return "dallas-" + str_lower_case(format_hex(this->address_)); }

}  // namespace ds248x
}  // namespace esphome
