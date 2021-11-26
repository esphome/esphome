#include "dallas_component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dallas {

static const char *const TAG = "dallas.sensor";

static const uint8_t DALLAS_MODEL_DS18S20 = 0x10;
static const uint8_t DALLAS_MODEL_DS1822 = 0x22;
static const uint8_t DALLAS_MODEL_DS18B20 = 0x28;
static const uint8_t DALLAS_MODEL_DS1825 = 0x3B;
static const uint8_t DALLAS_MODEL_DS28EA00 = 0x42;
static const uint8_t DALLAS_COMMAND_START_CONVERSION = 0x44;
static const uint8_t DALLAS_COMMAND_READ_SCRATCH_PAD = 0xBE;
static const uint8_t DALLAS_COMMAND_WRITE_SCRATCH_PAD = 0x4E;

uint16_t DallasTemperatureSensor::millis_to_wait_for_conversion() const {
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

void DallasComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DallasComponent...");

  pin_->setup();
  one_wire_ = new ESPOneWire(pin_);  // NOLINT(cppcoreguidelines-owning-memory)

  std::vector<uint64_t> raw_sensors;
  raw_sensors = this->one_wire_->search_vec();

  for (auto &address : raw_sensors) {
    std::string s = uint64_to_string(address);
    auto *address8 = reinterpret_cast<uint8_t *>(&address);
    if (crc8(address8, 7) != address8[7]) {
      ESP_LOGW(TAG, "Dallas device 0x%s has invalid CRC.", s.c_str());
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

  for (auto sensor : this->sensors_) {
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
void DallasComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DallasComponent:");
  LOG_PIN("  Pin: ", this->pin_);
  LOG_UPDATE_INTERVAL(this);

  if (this->found_sensors_.empty()) {
    ESP_LOGW(TAG, "  Found no sensors!");
  } else {
    ESP_LOGD(TAG, "  Found sensors:");
    for (auto &address : this->found_sensors_) {
      std::string s = uint64_to_string(address);
      ESP_LOGD(TAG, "    0x%s", s.c_str());
    }
  }

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

void DallasComponent::register_sensor(DallasTemperatureSensor *sensor) { this->sensors_.push_back(sensor); }
void DallasComponent::update() {
  this->status_clear_warning();

  bool result;
  if (!this->one_wire_->reset()) {
    result = false;
  } else {
    result = true;
    this->one_wire_->skip();
    this->one_wire_->write8(DALLAS_COMMAND_START_CONVERSION);
  }

  if (!result) {
    ESP_LOGE(TAG, "Requesting conversion failed");
    this->status_set_warning();
    return;
  }

  for (auto *sensor : this->sensors_) {
    this->set_timeout(sensor->get_address_name(), sensor->millis_to_wait_for_conversion(), [this, sensor] {
      bool res = sensor->read_scratch_pad();

      if (!res) {
        ESP_LOGW(TAG, "'%s' - Resetting bus for read failed!", sensor->get_name().c_str());
        sensor->publish_state(NAN);
        this->status_set_warning();
        return;
      }
      if (!sensor->check_scratch_pad()) {
        ESP_LOGW(TAG, "'%s' - Scratch pad checksum invalid!", sensor->get_name().c_str());
        sensor->publish_state(NAN);
        this->status_set_warning();
        return;
      }

      float tempc = sensor->get_temp_c();
      ESP_LOGD(TAG, "'%s': Got Temperature=%.1f°C", sensor->get_name().c_str(), tempc);
      sensor->publish_state(tempc);
    });
  }
}

void DallasTemperatureSensor::set_address(uint64_t address) { this->address_ = address; }
uint8_t DallasTemperatureSensor::get_resolution() const { return this->resolution_; }
void DallasTemperatureSensor::set_resolution(uint8_t resolution) { this->resolution_ = resolution; }
optional<uint8_t> DallasTemperatureSensor::get_index() const { return this->index_; }
void DallasTemperatureSensor::set_index(uint8_t index) { this->index_ = index; }
uint8_t *DallasTemperatureSensor::get_address8() { return reinterpret_cast<uint8_t *>(&this->address_); }
const std::string &DallasTemperatureSensor::get_address_name() {
  if (this->address_name_.empty()) {
    this->address_name_ = std::string("0x") + uint64_to_string(this->address_);
  }

  return this->address_name_;
}
bool IRAM_ATTR DallasTemperatureSensor::read_scratch_pad() {
  auto *wire = this->parent_->one_wire_;
  if (!wire->reset()) {
    return false;
  }

  wire->select(this->address_);
  wire->write8(DALLAS_COMMAND_READ_SCRATCH_PAD);

  for (unsigned char &i : this->scratch_pad_) {
    i = wire->read8();
  }
  return true;
}
bool DallasTemperatureSensor::setup_sensor() {
  bool r = this->read_scratch_pad();

  if (!r) {
    ESP_LOGE(TAG, "Reading scratchpad failed: reset");
    return false;
  }
  if (!this->check_scratch_pad())
    return false;

  if (this->scratch_pad_[4] == this->resolution_)
    return false;

  if (this->get_address8()[0] == DALLAS_MODEL_DS18S20) {
    // DS18S20 doesn't support resolution.
    ESP_LOGW(TAG, "DS18S20 doesn't support setting resolution.");
    return false;
  }

  switch (this->resolution_) {
    case 12:
      this->scratch_pad_[4] = 0x7F;
      break;
    case 11:
      this->scratch_pad_[4] = 0x5F;
      break;
    case 10:
      this->scratch_pad_[4] = 0x3F;
      break;
    case 9:
    default:
      this->scratch_pad_[4] = 0x1F;
      break;
  }

  auto *wire = this->parent_->one_wire_;
  if (wire->reset()) {
    wire->select(this->address_);
    wire->write8(DALLAS_COMMAND_WRITE_SCRATCH_PAD);
    wire->write8(this->scratch_pad_[2]);  // high alarm temp
    wire->write8(this->scratch_pad_[3]);  // low alarm temp
    wire->write8(this->scratch_pad_[4]);  // resolution
    wire->reset();

    // write value to EEPROM
    wire->select(this->address_);
    wire->write8(0x48);
  }

  delay(20);  // allow it to finish operation
  wire->reset();
  return true;
}
bool DallasTemperatureSensor::check_scratch_pad() {
#ifdef ESPHOME_LOG_LEVEL_VERY_VERBOSE
  ESP_LOGVV(TAG, "Scratch pad: %02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X (%02X)", this->scratch_pad_[0],
            this->scratch_pad_[1], this->scratch_pad_[2], this->scratch_pad_[3], this->scratch_pad_[4],
            this->scratch_pad_[5], this->scratch_pad_[6], this->scratch_pad_[7], this->scratch_pad_[8],
            crc8(this->scratch_pad_, 8));
#endif
  return crc8(this->scratch_pad_, 8) == this->scratch_pad_[8];
}
float DallasTemperatureSensor::get_temp_c() {
  int16_t temp = (int16_t(this->scratch_pad_[1]) << 11) | (int16_t(this->scratch_pad_[0]) << 3);
  if (this->get_address8()[0] == DALLAS_MODEL_DS18S20) {
    int diff = (this->scratch_pad_[7] - this->scratch_pad_[6]) << 7;
    temp = ((temp & 0xFFF0) << 3) - 16 + (diff / this->scratch_pad_[7]);
  }

  return temp / 128.0f;
}
std::string DallasTemperatureSensor::unique_id() { return "dallas-" + uint64_to_string(this->address_); }

}  // namespace dallas
}  // namespace esphome
