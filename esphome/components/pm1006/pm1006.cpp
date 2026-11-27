#include "pm1006.h"
#include "esphome/core/log.h"

namespace esphome::pm1006 {

ESPHOME_LOG_TAG(TAG, "pm1006");

static const uint8_t PM1006_REQUEST[] = {0x11, 0x02, 0x0B, 0x01, 0xE1};

void PM1006Component::setup() {
  // because this implementation is currently rx-only, there is nothing to setup
}

void PM1006Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PM1006:");
  LOG_SENSOR("  ", "PM2.5", this->pm_2_5_sensor_);
  LOG_UPDATE_INTERVAL(this);
}

void PM1006Component::update() {
  ESP_LOGV(TAG, "sending measurement request");
  this->write_array(PM1006_REQUEST, sizeof(PM1006_REQUEST));
}

void PM1006Component::loop() {
  this->read_available([this](uint8_t byte) {
    const uint8_t *frame = this->reader_.feed(byte);
    if (frame == nullptr)
      return;
    // All bytes of a frame, checksum included, sum to zero
    uint8_t checksum = sum_bytes(frame, FRAME_SIZE);
    if (checksum != 0) {
      ESP_LOGW(TAG, "PM1006 checksum is wrong: %02x, expected zero", checksum);
      this->reader_.resync();
      return;
    }
    this->parse_data_(frame);
  });
}

// Bytes 3..18 are DF1..DF16 in the datasheet at
// http://www.jdscompany.co.kr/download.asp?gubun=07&filename=PM1006_LED_PARTICLE_SENSOR_MODULE_SPECIFICATIONS.pdf
// PM1006 only uses DF3..DF4 (PM2.5); PM1006K uses more.
void PM1006Component::parse_data_(const uint8_t *frame) {
  const uint16_t pm_2_5_concentration = encode_uint16(frame[5], frame[6]);

  ESP_LOGD(TAG, "Got PM2.5 Concentration: %d µg/m³", pm_2_5_concentration);

  PUBLISH_STATE(this->pm_2_5_sensor_, pm_2_5_concentration);
}

}  // namespace esphome::pm1006
