#include "dallas_temp.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dallas_temp {

static const char *const TAG = "dallas.temp.sensor";

static const uint8_t DALLAS_MODEL_DS18S20 = 0x10;
static const uint8_t DALLAS_COMMAND_START_CONVERSION = 0x44;
static const uint8_t DALLAS_COMMAND_READ_SCRATCH_PAD = 0xBE;
static const uint8_t DALLAS_COMMAND_WRITE_SCRATCH_PAD = 0x4E;
static const uint8_t DALLAS_COMMAND_COPY_SCRATCH_PAD = 0x48;

uint16_t DallasTemperatureSensor::millis_to_wait_for_conversion_() const {
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

void DallasTemperatureSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Dallas Temperature Sensor:");
  if (this->address_ == 0) {
    ESP_LOGW(TAG, "  Unable to select an address");
    return;
  }
  LOG_ONE_WIRE_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Resolution: %u bits", this->resolution_);
  LOG_UPDATE_INTERVAL(this);
}

void DallasTemperatureSensor::update() {
  if (this->address_ == 0)
    return;

  this->status_clear_warning();

  this->send_command_(DALLAS_COMMAND_START_CONVERSION);

  this->set_timeout(this->get_address_name(), this->millis_to_wait_for_conversion_(), [this] {
    if (!this->read_scratch_pad_() || !this->check_scratch_pad_()) {
      this->publish_state(NAN);
      return;
    }

    float tempc = this->get_temp_c_();
    ESP_LOGD(TAG, "'%s': Got Temperature=%.1f°C", this->get_name().c_str(), tempc);
    this->publish_state(tempc);
  });
}

bool DallasTemperatureSensor::read_scratch_pad_() {
  bool success = this->send_command_(DALLAS_COMMAND_READ_SCRATCH_PAD);
  if (success) {
    for (uint8_t &i : this->scratch_pad_) {
      i = this->bus_->read8();
    }
  } else {
    ESP_LOGW(TAG, "'%s' - reading scratch pad failed bus reset", this->get_name().c_str());
    this->status_set_warning("bus reset failed");
  }
  return success;
}

void DallasTemperatureSensor::setup() {
  ESP_LOGCONFIG(TAG, "setting up Dallas temperature sensor...");
  if (!this->check_address_())
    return;
  if (!this->read_scratch_pad_())
    return;
  if (!this->check_scratch_pad_())
    return;

  if ((this->address_ & 0xff) == DALLAS_MODEL_DS18S20) {
    // DS18S20 doesn't support resolution.
    ESP_LOGW(TAG, "DS18S20 doesn't support setting resolution.");
    return;
  }

  uint8_t res;
  switch (this->resolution_) {
    case 12:
      res = 0x7F;
      break;
    case 11:
      res = 0x5F;
      break;
    case 10:
      res = 0x3F;
      break;
    case 9:
    default:
      res = 0x1F;
      break;
  }

  if (this->scratch_pad_[4] == res)
    return;
  this->scratch_pad_[4] = res;

  if (this->send_command_(DALLAS_COMMAND_WRITE_SCRATCH_PAD)) {
    this->bus_->write8(this->scratch_pad_[2]);  // high alarm temp
    this->bus_->write8(this->scratch_pad_[3]);  // low alarm temp
    this->bus_->write8(this->scratch_pad_[4]);  // resolution
  }

  // write value to EEPROM
  this->send_command_(DALLAS_COMMAND_COPY_SCRATCH_PAD);
}

bool DallasTemperatureSensor::check_scratch_pad_() {
  bool chksum_validity = (crc8(this->scratch_pad_, 8) == this->scratch_pad_[8]);

#ifdef ESPHOME_LOG_LEVEL_VERY_VERBOSE
  ESP_LOGVV(TAG, "Scratch pad: %02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X (%02X)", this->scratch_pad_[0],
            this->scratch_pad_[1], this->scratch_pad_[2], this->scratch_pad_[3], this->scratch_pad_[4],
            this->scratch_pad_[5], this->scratch_pad_[6], this->scratch_pad_[7], this->scratch_pad_[8],
            crc8(this->scratch_pad_, 8));
#endif
  if (!chksum_validity) {
    ESP_LOGW(TAG, "'%s' - Scratch pad checksum invalid!", this->get_name().c_str());
    this->status_set_warning("scratch pad checksum invalid");
    ESP_LOGD(TAG, "Scratch pad: %02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X (%02X)", this->scratch_pad_[0],
             this->scratch_pad_[1], this->scratch_pad_[2], this->scratch_pad_[3], this->scratch_pad_[4],
             this->scratch_pad_[5], this->scratch_pad_[6], this->scratch_pad_[7], this->scratch_pad_[8],
             crc8(this->scratch_pad_, 8));
  }
  return chksum_validity;
}

float DallasTemperatureSensor::get_temp_c_() {
  int16_t temp = (this->scratch_pad_[1] << 8) | this->scratch_pad_[0];
  if ((this->address_ & 0xff) == DALLAS_MODEL_DS18S20) {
    return (temp >> 1) + (this->scratch_pad_[7] - this->scratch_pad_[6]) / float(this->scratch_pad_[7]) - 0.25;
  }
  switch (this->resolution_) {
    case 9:
      temp &= 0xfff8;
      break;
    case 10:
      temp &= 0xfffc;
      break;
    case 11:
      temp &= 0xfffe;
      break;
    case 12:
    default:
      break;
  }

  return temp / 16.0f;
}

}  // namespace dallas_temp
}  // namespace esphome
