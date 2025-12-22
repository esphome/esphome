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

  this->reset_hub_();  // selects channel 0 on hub
  uint64_t address = 0;
  uint8_t channel = 0;
  bool search = false;
  while ((search = this->search_(&address)) || channel < channel_count_ - 1) {
    if (!search) {  // if condition is true here with no address found, no more devices on channel
      ESP_LOGD(TAG, "no more devices on channel %d", channel);
      channel++;
      reset_hub_();
      if (!select_channel_(channel)) {
        ESP_LOGW(TAG, "failed switching to channel %d while scan...", channel);
      }
      continue;
    } else {
      ESP_LOGD(TAG, "device 0x%s on channel %d", format_hex(address).c_str(), channel);
    }
    FoundDevice device;
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

  if (!this->enable_strong_pullup_) {
    // Check for parasitic power on all channels
    for (uint8_t i = 0; i < this->channel_count_; i++) {
      if (this->select_channel_(i)) {
        if (this->check_parasitic_power_()) {
          ESP_LOGI(TAG, "Parasitic power detected on channel %d. Enabling strong pullup.", i);
          this->enable_strong_pullup_ = true;
          break;
        }
      }
    }
  }

  for (auto *sensor : this->sensors_) {
    if (sensor->get_channel().has_value() && *sensor->get_channel() >= this->channel_count_) {
      ESP_LOGE(TAG, "Sensor %s configured for channel %d but device only has %d channels.", sensor->get_name().c_str(),
               *sensor->get_channel(), this->channel_count_);
      this->status_set_error();
      sensor->ignore();
      continue;
    }

    if (sensor->get_address().has_value() && sensor->get_channel().has_value()) {
      // sensor was fully specified by config
    } else if (sensor->get_index().has_value()) {
      if (*sensor->get_index() >= this->found_sensors_.size()) {
        ESP_LOGW(TAG, "specified sensor index (%d) bigger than found devices (%d): %s", *sensor->get_index(),
                 (int) this->found_sensors_.size(), sensor->get_name().c_str());
        this->status_set_error();
        sensor->ignore();
        continue;
      }
      sensor->set_address(this->found_sensors_[*sensor->get_index()].address);
      sensor->set_channel(this->found_sensors_[*sensor->get_index()].channel);
    } else {
      if (!sensor->get_address().has_value()) {
        ESP_LOGE(TAG, "Sensor %s has no address and no index.", sensor->get_name().c_str());
        sensor->ignore();
        continue;
      }
      bool sensor_found = false;
      for (auto fsensor : this->found_sensors_) {
        if (fsensor.address == *sensor->get_address()) {
          sensor->set_channel(fsensor.channel);
          sensor_found = true;
          break;
        }
      }
      if (!sensor_found) {
        sensor->ignore();
        ESP_LOGW(TAG, "sensor %s not found at address: 0x%s", sensor->get_name().c_str(),
                 format_hex(*sensor->get_address()).c_str());
        continue;
      }
    }

    if (!sensor->setup_sensor()) {
      this->status_set_error();
    }
  }

  std::sort(this->sensors_.begin(), this->sensors_.end(), [](DS248xSensor *a, DS248xSensor *b) {
    uint8_t ch_a = a->get_channel().value_or(0);
    uint8_t ch_b = b->get_channel().value_or(0);
    return ch_a < ch_b;
  });
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
  ESP_LOGV(TAG, "Start sensor update for %d sensors", (int) sensors_.size());
  this->status_clear_warning();

  if (this->enable_bus_sleep_) {
    this->write_config_(this->read_config_() & ~DS248X_CONFIG_POWER_DOWN);
  }

  read_idx_ = 0;
  /* this call triggers a cascade of async calls
  - collect all conversion commands from the sensors to be sent to this channel
  - trigger conversion and wait for it: start_next_conversion uses set_interval
  - when all conversions were done
    - update each sensor on channel: update_channel_sensors uses set_interval
    - when all sensors on channel updated, updateChannel(+1)*/
  update_channel_(0);
}

void DS248xComponent::update_channel_(uint8_t channel) {
  if (channel >= channel_count_)
    return;

  conv_cmds_.clear();
  for (auto *sensor : this->sensors_) {
    auto channel_opt = sensor->get_channel();
    if (!channel_opt.has_value() || *channel_opt != channel)
      continue;
    sensor->add_conversion_commands(conv_cmds_);
  }

  conv_cmds_iter_ = conv_cmds_.begin();
  if (conv_cmds_iter_ != conv_cmds_.end()) {
    ESP_LOGD(TAG, "Updating channel %i...", channel);
    if (!select_channel_(channel)) {
      this->status_set_warning();
      ESP_LOGE(TAG, "Select channel failed");
      return;
    }
    start_next_conversion_();
  } else {
    update_channel_(channel + 1);
  }
}

void DS248xComponent::start_next_conversion_() {
  if (conv_cmds_iter_ == conv_cmds_.end()) {  // all conversions done
    ESP_LOGD(TAG, "conversions done");
    this->update_channel_sensors_();
    return;
  }
  ESP_LOGD(TAG, "starting next conversion: %02x", *conv_cmds_iter_);
  bool result = this->reset_devices_();
  if (!result) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Reset failed");
    conv_cmds_iter_++;
    this->start_next_conversion_();
    return;
  }
  this->write_to_wire_(WIRE_COMMAND_SKIP);

  if (this->enable_strong_pullup_) {
    this->write_config_(this->read_config_() | DS248X_CONFIG_STRONG_PULLUP);
  }

  this->write_to_wire_(*conv_cmds_iter_);
  conv_cmds_iter_++;

  if (this->enable_strong_pullup_) {
    // Strong pullup is active, we cannot poll as it would disable the pullup
    // and cut power to parasitic devices. Wait for max conversion time (750ms).
    uint16_t delay_ms = 750;
    uint8_t max_res = 9;
    bool force_max_delay = false;
    for (auto *sensor : this->sensors_) {
      if (sensor->get_channel().has_value() && *sensor->get_channel() == selected_channel_ && !sensor->is_ignored()) {
        max_res = std::max(max_res, sensor->get_resolution());
        if (sensor->get_address8()[0] == DALLAS_MODEL_DS18S20) {
          force_max_delay = true;
        }
      }
    }
    if (!force_max_delay) {
      if (max_res == 9) {
        delay_ms = 94;
      } else if (max_res == 10) {
        delay_ms = 188;
      } else if (max_res == 11) {
        delay_ms = 375;
      }
    }

    this->set_interval(TAG, delay_ms, [this] {
      this->cancel_interval(TAG);
      if (this->enable_strong_pullup_) {
        this->write_config_(this->read_config_() & ~DS248X_CONFIG_STRONG_PULLUP);
      }
      this->start_next_conversion_();
    });
  } else {
    this->set_interval(TAG, 50, [&] {
      if ((is_busy_() & DS248X_STATUS_BUSY) != 0) {
        ESP_LOGD(TAG, "SBR tells 1W busy");
        return;
      }
      this->write_command_(DS248X_COMMAND_SINGLEBIT, 0x80);  // generates read bit
      delay(1);                                              // wait for single bit command to complete
      uint8_t status = 0;
      this->read(&status, 1);
      ESP_LOGD(TAG, "conversion status: %02x", status);
      if ((status & 0x20) != 0) {  // bit 5 SBR = Single Bit Result
        // if not busy anymore
        this->cancel_interval(TAG);
        this->start_next_conversion_();
      }
    });
  }
}

void DS248xComponent::update_channel_sensors_() {
  this->set_interval(TAG, 50, [this] {
    if (read_idx_ >= sensors_.size()) {
      this->cancel_interval(TAG);
      if (this->enable_bus_sleep_) {
        this->write_config_(this->read_config_() | DS248X_CONFIG_POWER_DOWN);
      }
      return;
    }
    DS248xSensor *sensor = sensors_[read_idx_];
    auto channel = sensor->get_channel();
    if (!channel.has_value()) {
      ESP_LOGW(TAG, "Sensor at index %u has no channel assigned, skipping.", static_cast<unsigned>(read_idx_));
      read_idx_++;
      return;
    }

    if (*channel != selected_channel_) {  // selected sensor is from different channel
      // cancel this interval and continue with this sensor on the next channel
      this->cancel_interval(TAG);
      update_channel_(*channel);
      return;
    }
    read_idx_++;

    if (!sensor->is_ignored()) {
      ESP_LOGD(TAG, "Update Sensor idx: %i", read_idx_);
      bool res = sensor->update();
      if (!res) {
        ESP_LOGW(TAG, "Reading sensor failed!");
      }
    }
  });
}

float DS248xComponent::get_setup_priority() const { return setup_priority::DATA; }

uint8_t DS248xComponent::read_config_() {
  std::array<uint8_t, 2> cmd;
  cmd[0] = DS248X_COMMAND_SETREADPTR;
  cmd[1] = DS248X_POINTER_CONFIG;
  auto err = this->write(cmd.data(), sizeof(cmd));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error writing SETREADPTR command to Master: %d", err);
  }

  uint8_t cfg_byte = 0;
  err = this->read(&cfg_byte, sizeof(cfg_byte));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error reading from Master: %d", err);
  }

  return cfg_byte;
}

void DS248xComponent::write_config_(uint8_t cfg) {
  std::array<uint8_t, 2> cmd;
  cmd[0] = DS248X_COMMAND_WRITECONFIG;
  cmd[1] = cfg | ((~cfg) << 4);
  auto err = this->write(cmd.data(), sizeof(cmd));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error writing WRITECONFIG command to Master: %d", err);
  }
}

uint8_t DS248xComponent::is_busy_() {
  std::array<uint8_t, 2> cmd;
  cmd[0] = DS248X_COMMAND_SETREADPTR;
  cmd[1] = DS248X_POINTER_STATUS;
  auto err = this->write(cmd.data(), sizeof(cmd));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error writing SETREADPTR command to Master: %d", err);
  }

  uint8_t status = 0;
  err = this->read(&status, sizeof(status));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error reading SBR from Master: %d", err);
  }

  return status;
}

uint8_t DS248xComponent::wait_while_busy_() {
  // this commands set read pointer and initially checks
  uint8_t status = is_busy_();
  if ((status & DS248X_STATUS_BUSY) == 0)
    return status;

  // continuous reads
  for (int i = 100; i > 0; i--) {
    auto err = this->read(&status, sizeof(status));
    if (err == esphome::i2c::ERROR_OK && !(status & DS248X_STATUS_BUSY))
      break;
    delay(1);
  }
  return status;
}

bool DS248xComponent::select_channel_(uint8_t channel) {
  if (this->channel_count_ <= 1) {
    return true;
  }
  if (channel == selected_channel_) {
    return true;
  }
  auto status = wait_while_busy_();
  if (status & DS248X_STATUS_BUSY) {
    ESP_LOGW(TAG, "Master never finished command");
    return false;
  }

  std::array<uint8_t, 2> channel_code;
  channel_code[0] = DS248X_COMMAND_CHANNELSELECT;
  uint8_t selection_code = DS248X_CODE_CHANNEL0 - (channel << 4) + channel;
  channel_code[1] = selection_code;
  ESP_LOGV(TAG, "select_channel command: %x", channel_code[1]);
  auto err = this->write(channel_code.data(), sizeof(channel_code));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error writing CHANNELSELECT command to Master: %d", err);
  }

  uint8_t selected = 0;
  err = this->read(&selected, sizeof(selected));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error reading from Master: %d", err);
  }
  if (selected != channel_code[1]) {
    ESP_LOGW(TAG, "select_channel failed: wrote %02xh read back %02xh", channel_code[1], selected);
    return false;
  }

  selected_channel_ = channel;
  return true;
}

void DS248xComponent::reset_hub_() {
  if (this->sleep_pin_) {
    this->sleep_pin_->digital_write(true);
    delay(1);
  }

  selected_channel_ = 0;

  uint8_t cmd = DS248X_COMMAND_RESET;
  auto err = this->write(&cmd, sizeof(cmd));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error writing RESET command to Master: %d", err);
  }

  uint8_t config = 0;
  if (this->enable_active_pullup_) {
    config |= DS248X_CONFIG_ACTIVE_PULLUP;
  }
  if (this->enable_overdrive_speed_) {
    config |= DS248X_CONFIG_1WIRE_SPEED;
  }
  if (config != 0) {
    this->write_config_(config);
  }

  // DS2484 specific port configuration
  // These commands use opcode 0xC3, which conflicts with the channel select
  // command on multi-channel devices (e.g., DS2482-800). Only execute them
  // when using a single-channel device.
  if (this->channel_count_ == 1) {
    if (this->val_trstl_.has_value()) {
      this->write_command_(DS248X_COMMAND_DS2484_CONFIG, 0x00 | (*this->val_trstl_ & 0x0F));
    }
    if (this->val_tmsp_.has_value()) {
      this->write_command_(DS248X_COMMAND_DS2484_CONFIG, 0x10 | (*this->val_tmsp_ & 0x0F));
    }
    if (this->val_tw0l_.has_value()) {
      this->write_command_(DS248X_COMMAND_DS2484_CONFIG, 0x20 | (*this->val_tw0l_ & 0x0F));
    }
    if (this->val_trec0_.has_value()) {
      this->write_command_(DS248X_COMMAND_DS2484_CONFIG, 0x30 | (*this->val_trec0_ & 0x0F));
    }
    if (this->val_rwpu_.has_value()) {
      this->write_command_(DS248X_COMMAND_DS2484_CONFIG, 0x40 | (*this->val_rwpu_ & 0x0F));
    }
  }

  last_device_found_ = false;
  search_address_ = 0;
  search_last_discrepancy_ = 0;
}

bool DS248xComponent::reset_devices_() {
  auto status = wait_while_busy_();
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

  status = wait_while_busy_();

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

void DS248xComponent::write_command_(uint8_t command, uint8_t data) {
  auto status = wait_while_busy_();

  if (status & DS248X_STATUS_BUSY) {
    ESP_LOGE(TAG, "Master never finished command");
    return;
  }

  std::array<uint8_t, 2> cmd;
  cmd[0] = command;
  cmd[1] = data;
  auto err = this->write(cmd.data(), sizeof(cmd));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error writing command 0x%02X to Master: %d", cmd[0], err);
  }
}

void DS248xComponent::select_(uint8_t channel, uint64_t address) {
  select_channel_(channel);
  this->write_command_(DS248X_COMMAND_WRITEBYTE, WIRE_COMMAND_SELECT);

  for (int i = 0; i < 8; i++) {
    this->write_command_(DS248X_COMMAND_WRITEBYTE, (address >> (i * 8)) & 0xff);
  }
}

void DS248xComponent::write_to_wire_(uint8_t data) { this->write_command_(DS248X_COMMAND_WRITEBYTE, data); }

uint8_t DS248xComponent::read_from_wire_() {
  auto status = wait_while_busy_();

  if (status & DS248X_STATUS_BUSY) {
    ESP_LOGE(TAG, "Master never finished command");
    return 0;
  }

  uint8_t command = DS248X_COMMAND_READBYTE;
  auto err = this->write(&command, sizeof(command));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error writing READBYTE command to Master: %d", err);
  }

  status = wait_while_busy_();

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

  uint8_t data_byte = 0;
  err = this->read(&data_byte, sizeof(data_byte));
  if (err != esphome::i2c::ERROR_OK) {
    ESP_LOGE(TAG, "error reading from Master: %d", err);
  }

  return data_byte;
}

bool DS248xComponent::search_(uint64_t *address) {
  if (last_device_found_)
    return false;

  bool result = this->reset_devices_();
  if (!result) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Reset failed");
    return false;
  }

  write_to_wire_(WIRE_COMMAND_SEARCH);

  uint8_t direction;
  int last_zero = -1;
  for (uint8_t i = 0; i < 64; i++) {
    uint64_t search_bit = 1LL << i;

    if (i < search_last_discrepancy_) {
      direction = (search_address_ & search_bit) != 0;
    } else {
      direction = i == search_last_discrepancy_;
    }

    write_command_(DS248X_COMMAND_TRIPLET, direction ? 0x80 : 0x00);

    uint8_t status = wait_while_busy_();
    ESP_LOGVV(TAG, "Search: i: %i dir: %i, status: %i bit: %llu", i, direction, status, search_bit);

    uint8_t id = status & DS248X_STATUS_SBR;
    uint8_t comp_id = status & DS248X_STATUS_TSB;
    direction = status & DS248X_STATUS_DIR;

    if (id && comp_id) {
      return false;
    } else if (!id && !comp_id && !direction) {
      last_zero = i;
    }

    if (direction) {
      search_address_ |= search_bit;
    } else {
      search_address_ &= ~search_bit;
    }
  }

  search_last_discrepancy_ = last_zero;

  if (last_zero == -1) {
    last_device_found_ = true;
  }

  *address = search_address_;

  return true;
}

bool DS248xComponent::check_parasitic_power_() {
  if (!this->reset_devices_())
    return false;

  this->write_to_wire_(WIRE_COMMAND_SKIP);
  this->write_to_wire_(0xB4);  // READ POWER SUPPLY command

  // Read a bit
  this->write_command_(DS248X_COMMAND_SINGLEBIT, 0x80);
  auto status = this->wait_while_busy_();
  if (status & DS248X_STATUS_BUSY) {
    return false;
  }

  // If bit is 0 (SBR=0), then we have parasitic power
  return (status & DS248X_STATUS_SBR) == 0;
}

void DS248xSensor::ignore() { this->ignored_ = true; }
bool DS248xSensor::is_ignored() { return this->ignored_; }
void DS248xSensor::set_address(uint64_t address) { this->address_ = address; }
optional<uint64_t> DS248xSensor::get_address() { return address_; };
optional<uint8_t> DS248xSensor::get_index() const { return this->index_; }
void DS248xSensor::set_index(uint8_t index) { this->index_ = index; }
uint8_t *DS248xSensor::get_address8() {
  if (!this->address_.has_value())
    return nullptr;
  return reinterpret_cast<uint8_t *>(&*this->address_);
}
const std::string &DS248xSensor::get_address_name() {
  if (this->address_name_.empty() && this->address_.has_value()) {
    this->address_name_ = std::string("0x") + format_hex(*this->address_);
  }

  return this->address_name_;
}
std::string DS248xSensor::unique_id() {
  if (!this->address_.has_value()) {
    ESP_LOGW(TAG, "DS248xSensor unique_id() called before address was set");
    return {};
  }
  return "dallas-" + str_lower_case(format_hex(*this->address_));
}
void DS248xSensor::set_channel(uint8_t channel) { channel_ = channel; }
optional<uint8_t> DS248xSensor::get_channel() { return channel_; }

// proxies to friend class DS248xComponent
void DS248xSensor::select_() {
  if (!this->channel_.has_value() || !this->address_.has_value()) {
    ESP_LOGW(TAG, "Cannot select sensor: channel or address not set");
    return;
  }
  this->parent_->select_(*this->channel_, *this->address_);
}
void DS248xSensor::select_channel_() {
  if (!this->channel_.has_value()) {
    ESP_LOGW(TAG, "Cannot select channel: channel not set");
    return;
  }
  this->parent_->select_channel_(*this->channel_);
}
void DS248xSensor::write_to_wire_(uint8_t data) { this->parent_->write_to_wire_(data); }
bool DS248xSensor::reset_devices_() { return this->parent_->reset_devices_(); }

bool DS248xSensor::read_scratch_pad(uint8_t page) {
  if (!this->channel_.has_value() || !this->address_.has_value()) {
    ESP_LOGW(TAG, "Cannot read scratch pad: channel or address not set");
    return false;
  }
  this->parent_->select_channel_(*this->channel_);

  bool result = this->parent_->reset_devices_();
  if (!result) {
    this->parent_->status_set_warning();
    ESP_LOGE(TAG, "Reset failed");
    return false;
  }

  this->parent_->select_(*this->channel_, *this->address_);
  this->parent_->write_to_wire_(DALLAS_COMMAND_READ_SCRATCH_PAD);
  if (page < 255) {
    this->parent_->write_to_wire_(page);
  }

  for (uint8_t &i : this->scratch_pad_) {
    i = this->parent_->read_from_wire_();
  }

  return true;
}

bool DS248xSensor::write_scratch_pad() {
  if (!this->channel_.has_value() || !this->address_.has_value())
    return false;
  this->parent_->select_channel_(*this->channel_);

  bool result = this->parent_->reset_devices_();
  if (!result) {
    this->parent_->status_set_warning();
    ESP_LOGE(TAG, "Reset failed");
    return false;
  }

  this->parent_->select_(*this->channel_, *this->address_);
  this->parent_->write_to_wire_(DALLAS_COMMAND_WRITE_SCRATCH_PAD);
  this->parent_->write_to_wire_(this->scratch_pad_[2]);  // TH
  this->parent_->write_to_wire_(this->scratch_pad_[3]);  // TL
  this->parent_->write_to_wire_(this->scratch_pad_[4]);  // Config

  return true;
}

bool DS248xSensor::check_scratch_pad() {
  uint8_t *addr = this->get_address8();
  if (!addr)
    return false;

  bool chksum_validity = (crc8(this->scratch_pad_, 8) == this->scratch_pad_[8]);
  bool config_validity = false;

  switch (addr[0]) {
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

bool DS248xSensor::setup_sensor() {
  if (!this->read_scratch_pad()) {
    return false;
  }
  if (!this->check_scratch_pad()) {
    return false;
  }

  uint8_t *addr = this->get_address8();
  if (!addr)
    return false;

  if (addr[0] == DALLAS_MODEL_DS18B20 || addr[0] == DALLAS_MODEL_DS1822) {
    uint8_t config_register = this->scratch_pad_[4];
    uint8_t current_resolution = ((config_register >> 5) & 0x03) + 9;
    if (current_resolution != this->resolution_) {
      // update config register
      config_register = (config_register & 0x9F) | ((this->resolution_ - 9) << 5);
      this->scratch_pad_[4] = config_register;
      this->write_scratch_pad();
    }
  }
  return true;
}

void DS248xSensor::add_conversion_commands(std::set<uint8_t> &commands) {
  commands.insert(DALLAS_COMMAND_START_CONVERSION);
}

bool DS248xSensor::update() {
  if (!this->read_scratch_pad()) {
    this->publish_state(NAN);
    return false;
  }
  if (!this->check_scratch_pad()) {
    this->publish_state(NAN);
    return false;
  }

  uint8_t *addr = this->get_address8();
  if (!addr)
    return false;

  int16_t temp_16 = (int16_t(this->scratch_pad_[1]) << 8) | this->scratch_pad_[0];
  if (addr[0] == DALLAS_MODEL_DS18S20) {
    temp_16 = ((temp_16 & 0xFFFE) << 3) + 16 - this->scratch_pad_[6];
  }

  float temp = temp_16 / 16.0f;
  this->publish_state(temp);
  return true;
}

}  // namespace ds248x
}  // namespace esphome
