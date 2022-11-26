#include "pm1006.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pm1006 {

static const char *const TAG = "pm1006";

static const uint8_t PM1006_RESPONSE_HEADER[] = {0x16, 0x11, 0x0B};
static const uint8_t PM1006K_RESPONSE_HEADER[] = {0x16, 0x0D, 0x02};

static const uint8_t PM1006_REQUEST[] = {0x11, 0x02, 0x0B, 0x01, 0xE1};
static const uint8_t PM1006K_REQUEST[] = {0x11, 0x01, 0x02, 0xEC};

void PM1006Component::setup() {
  // because this implementation is currently rx-only, there is nothing to setup
}

void PM1006Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PM1006: ", this->type_);
  LOG_SENSOR("  ", "PM1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM10.0", this->pm_10_0_sensor_);
  LOG_UPDATE_INTERVAL(this);
  this->check_uart_settings(9600);
}

void PM1006Component::update() {
  ESP_LOGV(TAG, "sending measurement request");
  switch(this->type_){
    case PM1006_TYPE_1006:
      this->write_array(PM1006_REQUEST, sizeof(PM1006_REQUEST));
      break;
    case PM1006_TYPE_1006K:
      this->write_array(PM1006K_REQUEST, sizeof(PM1006K_REQUEST));
      break;
  }
}

void PM1006Component::loop() {
  while (this->available() != 0) {
    this->read_byte(&this->data_[this->data_index_]);
    auto check = this->check_byte_();
    if (!check.has_value()) {
      // finished
      this->parse_data_();
      this->data_index_ = 0;
    } else if (!*check) {
      // wrong data
      ESP_LOGV(TAG, "Byte %i of received data frame is invalid.", this->data_index_);
      this->data_index_ = 0;
    } else {
      // next byte
      this->data_index_++;
    }
  }
}

float PM1006Component::get_setup_priority() const { return setup_priority::DATA; }

uint8_t PM1006Component::pm1006_checksum_(const uint8_t *command_data, uint8_t length) const {
  uint8_t sum = 0;
  for (uint8_t i = 0; i < length; i++) {
    sum += command_data[i];
  }
  return sum;
}

optional<bool> PM1006Component::check_byte_() const {
  uint8_t index = this->data_index_;
  uint8_t byte = this->data_[index];

  const uint8_t HEADER_SIZE =
  sizeof (this->type_ == PM1006_TYPE_1006 ? PM1006_RESPONSE_HEADER : PM1006K_RESPONSE_HEADER);
  const uint8_t PAYLOAD_SIZE = this->type_ == PM1006_TYPE_1006 ? 16 : 12;

  // index 0..2 are the fixed header
  if (index < HEADER_SIZE) {
    // We can't store the correct header in a unit8_t* as the comparison starts to fail
    // As such, let's use a ternary to get the correct header and then grab the index we want from there
    return byte == ((this->type_ == PM1006_TYPE_1006) ? PM1006_RESPONSE_HEADER : PM1006K_RESPONSE_HEADER)[index];
  }

  // just some additional notes here:
  // index 3..4 is unused
  // index 5..6   (DF3-4)   is our PM2.5 reading (3..6 is called DF1-DF4 in the datasheet)
  // index 7..8   (DF5-6)   is unused
  // index 9..10  (DF7-8)   is our PM1.0 reading
  // index 11..12 (DF9-10)  is unused
  // index 13..14 (DF11-12) is our PM10 reading
  // http://www.jdscompany.co.kr/download.asp?gubun=07&filename=PM1006_LED_PARTICLE_SENSOR_MODULE_SPECIFICATIONS.pdf
  // that datasheet goes on up to DF16, which is unused for PM1006 but used in PM1006K
  // so this code should be trivially extensible to support that one later
  if (index < (HEADER_SIZE + PAYLOAD_SIZE))
    return true;

  // checksum
  if (index == (HEADER_SIZE + PAYLOAD_SIZE)) {
    uint8_t checksum = pm1006_checksum_(this->data_, HEADER_SIZE + PAYLOAD_SIZE + 1);
    if (checksum != 0) {
      ESP_LOGW(TAG, "PM1006 checksum is wrong: %02x, expected zero", checksum);
      return false;
    }
    return {};
  }

  return false;
}

void PM1006Component::parse_data_() {
  const int pm_1_0_concentration = this->get_16_bit_uint_(9);   //DF7
  const int pm_2_5_concentration = this->get_16_bit_uint_(5);   //DF3
  const int pm_10_0_concentration = this->get_16_bit_uint_(13); //DF11

  ESP_LOGD(TAG, "Got PM1.0 Concentration: %d µg/m³", pm_1_0_concentration);
  ESP_LOGD(TAG, "Got PM2.5 Concentration: %d µg/m³", pm_2_5_concentration);
  ESP_LOGD(TAG, "Got PM10 Concentration: %d µg/m³", pm_10_0_concentration);

  if (this->pm_1_0_sensor_ != nullptr) {
    this->pm_1_0_sensor_->publish_state(pm_1_0_concentration);
  }

  if (this->pm_2_5_sensor_ != nullptr) {
    this->pm_2_5_sensor_->publish_state(pm_2_5_concentration);
  }

  if (this->pm_10_0_sensor_ != nullptr) {
    this->pm_10_0_sensor_->publish_state(pm_10_0_concentration);
  }
}

uint16_t PM1006Component::get_16_bit_uint_(uint8_t start_index) const {
  return encode_uint16(this->data_[start_index], this->data_[start_index + 1]);
}

}  // namespace pm1006
}  // namespace esphome
