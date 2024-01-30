#include "ds248x.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ds248x {

static const char *const TAG = "ds248x";


void DS248xComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DS248x...");

  if (this->sleep_pin_) {
    this->sleep_pin_->setup();
    this->sleep_pin_->pin_mode(esphome::gpio::FLAG_OUTPUT);
  }

  this->reset_hub(); // selects channel 0 on hub
  uint64_t address = 0;
  uint8_t channel = 0;
  bool search = false;
  while((search = this->search(&address)) || channel < channel_count_ - 1) {
    if (!search) { // if condition is true here with no address found, no more devices on channel
      ESP_LOGD(TAG, "no more devices on channel %d", channel);
      channel++;
      reset_hub();
      if (!select_channel(channel)) {
        ESP_LOGW(TAG, "failed switching to channel %d while scan...", channel);
      }
      continue;
    } else {
      ESP_LOGD(TAG, "device 0x%s on channel %d", format_hex(address).c_str(), channel);
    }
    foundDevice_t device;
    device.channel = channel;
    device.address = address;

    auto *address8 = reinterpret_cast<uint8_t *>(&address);
    if (crc8(address8, 7) != address8[7]) {
      ESP_LOGW(TAG, "Dallas device 0x%s has invalid CRC.", format_hex(address).c_str());
      continue;
    }

    ESP_LOGI(TAG, "found device 0x%s (channel: %d type: %d)", format_hex(address).c_str(), channel, address8[0]);
    this->found_sensors_.push_back(device);
  }

  for (auto *sensor : this->sensors_) {
    if (sensor->get_address().has_value() && sensor->get_channel().has_value()) {
      // sensor was fully specified by config
    } else if (sensor->get_index().has_value()) {
      if (*sensor->get_index() >= this->found_sensors_.size()) {
        ESP_LOGW(TAG, "specified sensor index (%d) bigger then found devices (%d): %s", *sensor->get_index(), this->found_sensors_.size(), sensor->get_name().c_str());
        this->status_set_error();
        continue;
      }
      sensor->set_address(this->found_sensors_[*sensor->get_index()].address);
      sensor->set_channel(this->found_sensors_[*sensor->get_index()].channel);
    } else {
      bool sensorFound = false;
      for (auto fsensor : this->found_sensors_) {
        if (fsensor.address == *sensor->get_address()){
          sensor->set_channel(fsensor.channel);
          sensorFound = true;
          break;
        }
      }
      if (!sensorFound) {
        sensor->ignore();
        ESP_LOGW(TAG, "sensor %s not found at address: 0x%s", sensor->get_name().c_str(), format_hex(*sensor->get_address()).c_str());
        continue;
      }
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
    for (auto &device : this->found_sensors_) {
      ESP_LOGD(TAG, "    0x%s (channel: %d)", format_hex(device.address).c_str(), device.channel);
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
  }
}

void DS248xComponent::register_sensor(DS248xSensor *sensor) { this->sensors_.push_back(sensor); }

void DS248xComponent::update() {
  ESP_LOGV(TAG, "Start sensor update for %i sensors", sensors_.size());
  this->status_clear_warning();

  if (this->enable_bus_sleep_) {
    this->write_config(this->read_config() & ~DS248X_CONFIG_POWER_DOWN);
  }

  readIdx = 0;
  /* this call triggers a cascade of async calls
  - collect all conversion commands from the sensors to be send to this channel
  - trigger conversion and wait for it: start_next_conversion uses set_interval
  - when all conversions were done
    - update each sensor on channel: update_channel_sensors uses set_interval
    - when all sensors on channel updated, updateChannel(+1)*/
  updateChannel(0);
}

void DS248xComponent::updateChannel(uint8_t channel) {
  if (channel >= channel_count_) return;
  ESP_LOGD(TAG, "Updating channel %i...", channel);
  if (!select_channel(channel)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Select channel failed");
    return;
  }

  if (this->enable_strong_pullup_) {
    this->write_config(this->read_config() | DS248X_CONFIG_STRONG_PULLUP);
  }

  convCmds_.clear();
  for (auto* sensor: this->sensors_) {
      if (*sensor->get_channel() != channel) continue;
      sensor->add_conversion_commands(convCmds_);
  }

  convCmdsIter_ = convCmds_.begin();
  if (convCmdsIter_ != convCmds_.end()) {
    start_next_conversion();
  } else {
    updateChannel(channel + 1);
  }
}

void DS248xComponent::start_next_conversion() {
  if (convCmdsIter_ == convCmds_.end()) { // all conversions done
    ESP_LOGD(TAG, "conversions done");
    this->update_channel_sensors();
    return;
  }
  ESP_LOGD(TAG, "starting next conversion: %02x", *convCmdsIter_);
  bool result = this->reset_devices();
  if (!result) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Reset failed");
    return;
  }
  this->write_to_wire(WIRE_COMMAND_SKIP);
  this->write_to_wire(*convCmdsIter_);
  convCmdsIter_++;

  this->set_interval(TAG, 50, [&] {
    // TODO this waiting is not working, better use ms waiting or check sensor registers?
    if ((is_busy() & DS248X_STATUS_BUSY) != 0) {
      ESP_LOGD(TAG, "SBR tells 1W busy");
      return;
    } 
    this->write_command(DS248X_COMMAND_SINGLEBIT, 0x80); // generates read bit
    delay(1); // wait for single bit command to complete
    uint8_t status = 0;
    this->read(&status, 1);
    ESP_LOGD(TAG, "conversion status: %02x", status);
    if ((status & 0x20) != 0) { // bit 5 SBR = Single Bit Result
      // if not busy anymore
      this->cancel_interval(TAG);
      this->start_next_conversion();
    }
  });
}

void DS248xComponent::update_channel_sensors()
{
  this->set_interval(TAG, 50, [this] {
    if (readIdx >= sensors_.size()) {
      this->cancel_interval(TAG);
      if (this->enable_bus_sleep_) {
        this->write_config(this->read_config() | DS248X_CONFIG_POWER_DOWN);
      }
      return;
    }
    DS248xSensor* sensor = sensors_[readIdx];
    if (*sensor->get_channel() != selectedChannel) { // selected sensor is from different channel
      // cancel this interval and continue with this sensor on the next channel
      this->cancel_interval(TAG);
      updateChannel(*sensor->get_channel());
      return;
    }
    readIdx++;

    if (!sensor->isIgnored()) {
      ESP_LOGD(TAG, "Update Sensor idx: %i", readIdx);
      bool res = sensor->update();
      if (!res) {
        ESP_LOGW(TAG, "Reading sensor failed!");
      }
    }
  });
}

float DS248xComponent::get_setup_priority() const { return setup_priority::DATA; }

uint8_t DS248xComponent::read_config() {
  std::array<uint8_t, 2> cmd;
  cmd[0] = DS248X_COMMAND_SETREADPTR;
  cmd[1] = DS248X_POINTER_CONFIG;
  auto err = this->write(cmd.data(), sizeof(cmd));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error writing SETREADPTR command to Master: %d", err);
  }

  uint8_t cfg_byte;
  err = this->read(&cfg_byte, sizeof(cfg_byte));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error reading from Master: %d", err);
  }

  return cfg_byte;
}

void DS248xComponent::write_config(uint8_t cfg) {
  std::array<uint8_t, 2> cmd;
  cmd[0] = DS248X_COMMAND_WRITECONFIG;
  cmd[1] = cfg | ((~cfg) << 4);
  auto err = this->write(cmd.data(), sizeof(cmd));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error writing WRITECONFIG command to Master: %d", err);
  }
}

uint8_t DS248xComponent::is_busy() {
  std::array<uint8_t, 2> cmd;
  cmd[0] = DS248X_COMMAND_SETREADPTR;
  cmd[1] = DS248X_POINTER_STATUS;
  auto err = this->write(cmd.data(), sizeof(cmd));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error writing SETREADPRT command to Master: %d", err);
  }

  uint8_t status;
  err = this->read(&status, sizeof(status));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error reading SBR from Master: %d", err);

  }
  
  return status;
}

uint8_t DS248xComponent::wait_while_busy() {
  // this commands set read pointer and initially checks
  uint8_t status = is_busy();
  if ((status & DS248X_STATUS_BUSY) == 0) return status;

  // continuous reads
  for(int i=1000; i>0; i--) {
      auto err = this->read(&status, sizeof(status));
      if (err == esphome::i2c::ERROR_OK && !(status & DS248X_STATUS_BUSY))
        break;
	}
	return status;
}

bool DS248xComponent::select_channel(uint8_t channel)
{
  if (channel == selectedChannel) {
    return true;
  }
  auto status = wait_while_busy();
  if (status & DS248X_STATUS_BUSY) {
    ESP_LOGW(TAG, "Master never finished command");
    return false;
  }

  std::array<uint8_t, 2> cmd;
  cmd[0] = DS248X_COMMAND_CHANNELSELECT;
  cmd[1] = DS248X_CODE_CHANNEL0 - (channel << 4) + channel;
  ESP_LOGV(TAG, "select_channel command: %x", cmd[1]);
  auto err = this->write(cmd.data(), sizeof(cmd));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error writing CHANNELSELECT command to Master: %d", err);
  }

  uint8_t selected;
  err = this->read(&selected, sizeof(selected));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error reading from Master: %d", err);
  }
  if (selected != DS248X_CODE_CHANNEL7 + (7 * (7 - channel))) {
    ESP_LOGW(TAG, "select_channel failed: wrote %02xh read back %02xh", cmd[1], selected);
    return false;
  }

  selectedChannel = channel;
  return true;
}


void DS248xComponent::reset_hub() {
  if (this->sleep_pin_) {
    this->sleep_pin_->digital_write(true);
  }

  uint8_t cmd = DS248X_COMMAND_RESET;
  auto err = this->write(&cmd, sizeof(cmd));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error writing RESET command to Master: %d", err);
  }

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

  return true;
}

void DS248xComponent::write_command(uint8_t command, uint8_t data) {
  auto status = wait_while_busy();

  if (status & DS248X_STATUS_BUSY) {
    ESP_LOGE(TAG, "Master never finished command");
    return;
  }

  std::array<uint8_t, 2> cmd;
  cmd[0] = command;
  cmd[1] = data;
  auto err = this->write(cmd.data(), sizeof(cmd));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error writing command to Master: %d", cmd[0]);
  }
}

void DS248xComponent::select(uint8_t channel, uint64_t address) {
  select_channel(channel);
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
    ESP_LOGE(TAG, "Master never finished command");
    return 0;
  }

  uint8_t command = DS248X_COMMAND_READBYTE;
  auto err = this->write(&command, sizeof(command));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error writing READBYTE command to Master: %d", err);
  }

  status = wait_while_busy();

  if (status & DS248X_STATUS_BUSY) {
    ESP_LOGE(TAG, "Master never finished command");
    return 0;
  }

  std::array<uint8_t, 2> cmd;
  cmd[0] = DS248X_COMMAND_SETREADPTR;
  cmd[1] = DS248X_POINTER_DATA;
  err = this->write(cmd.data(), sizeof(cmd));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error writing SETREADPTR command to Master: %d", err);
  }

  uint8_t data_byte;
  err = this->read(&data_byte, sizeof(data_byte));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error reading from Master: %d", err);
  }

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
    ESP_LOGVV(TAG, "Search: i: %i dir: %i, status: %i bit: %llu", i, direction, status, searchBit);

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

void DS248xSensor::ignore() { this->ignored_ = true; }
bool DS248xSensor::isIgnored() { return this->ignored_; }
void DS248xSensor::set_address(uint64_t address) { this->address_ = address; }
optional<uint64_t> DS248xSensor::get_address() { return address_; };
optional<uint8_t> DS248xSensor::get_index() const { return this->index_; }
void DS248xSensor::set_index(uint8_t index) { this->index_ = index; }
uint8_t *DS248xSensor::get_address8() { return reinterpret_cast<uint8_t *>(&*this->address_); }
const std::string &DS248xSensor::get_address_name() {
  if (this->address_name_.empty()) {
    this->address_name_ = std::string("0x") + format_hex(*this->address_);
  }

  return this->address_name_;
}
std::string DS248xSensor::unique_id() { return "dallas-" + str_lower_case(format_hex(*this->address_)); }
void DS248xSensor::set_channel(uint8_t channel) { channel_ = channel; }
optional<uint8_t> DS248xSensor::get_channel() { return channel_; }

// proxies to friend class DS248xComponent
void DS248xSensor::select() { this->parent_->select(*channel_, *address_); }
void DS248xSensor::select_channel() { this->parent_->select_channel(*channel_); }
void DS248xSensor::write_to_wire(uint8_t data) { this->parent_->write_to_wire(data); }
bool DS248xSensor::reset_devices() { return this->parent_->reset_devices(); }

bool IRAM_ATTR DS248xSensor::read_scratch_pad(uint8_t page) {
  this->parent_->select_channel(*this->channel_);
  
  bool result = this->parent_->reset_devices();
  if (!result) {
    this->parent_->status_set_warning();
    ESP_LOGE(TAG, "Reset failed");
    return false;
  }

  this->parent_->select(*this->channel_, *this->address_);
  this->parent_->write_to_wire(DALLAS_COMMAND_READ_SCRATCH_PAD);
  if (page < 255) {
    this->parent_->write_to_wire(page);
  }

  for (uint8_t &i : this->scratch_pad_) {
    i = this->parent_->read_from_wire();
  }

  return true;
}


bool DS248xSensor::check_scratch_pad() {
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
    ESP_LOGD(TAG, "Scratch pad: %02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X (%02X)", this->scratch_pad_[0],
              this->scratch_pad_[1], this->scratch_pad_[2], this->scratch_pad_[3], this->scratch_pad_[4],
              this->scratch_pad_[5], this->scratch_pad_[6], this->scratch_pad_[7], this->scratch_pad_[8],
              crc8(this->scratch_pad_, 8));
    ESP_LOGW(TAG, "'%s' - Scratch pad checksum invalid!", this->get_name().c_str());
  } else if (!config_validity) {
    ESP_LOGW(TAG, "'%s' - Scratch pad config register invalid!", this->get_name().c_str());
  }
  return chksum_validity && config_validity;
}

}  // namespace ds248x
}  // namespace esphome
