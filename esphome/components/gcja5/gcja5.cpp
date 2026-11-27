/* From snooping with a logic analyzer, the I2C on this sensor is broken. I was only able
 * to receive 1's as a response from the sensor. I was able to get the UART working.
 *
 * The datasheet says the values should be divided by 1000, but this must only be for the I2C
 * implementation. Comparing UART values with another sensor, there is no need to divide by 1000.
 */
#include "gcja5.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <cstring>

namespace esphome::gcja5 {

ESPHOME_LOG_TAG(TAG, "gcja5");

void GCJA5Component::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_transmission_ >= 500) {
    // last transmission too long ago. Reset RX index.
    this->reader_.reset();
  }

  const size_t avail = this->available();
  if (avail == 0) {
    return;
  }

  // There must now be data waiting
  this->last_transmission_ = now;
  this->read_available(
      [this](uint8_t val) {
        const uint8_t *frame = this->reader_.feed(val);
        if (frame == nullptr)
          return;
        if (!this->check_frame_(frame)) {
          this->status_set_warning();
          ESP_LOGV(TAG, "Have 32 bytes but not good data. Skipping.");
          this->reader_.resync();
          return;
        }
        this->status_clear_warning();
        PUBLISH_STATE(this->pm_1_0_sensor_, encode_value_le<uint32_t>(frame + 1));
        PUBLISH_STATE(this->pm_2_5_sensor_, encode_value_le<uint32_t>(frame + 5));
        PUBLISH_STATE(this->pm_10_0_sensor_, encode_value_le<uint32_t>(frame + 9));
        PUBLISH_STATE(this->pmc_0_3_sensor_, encode_value_le<uint16_t>(frame + 13));
        PUBLISH_STATE(this->pmc_0_5_sensor_, encode_value_le<uint16_t>(frame + 15));
        PUBLISH_STATE(this->pmc_1_0_sensor_, encode_value_le<uint16_t>(frame + 17));
        PUBLISH_STATE(this->pmc_2_5_sensor_, encode_value_le<uint16_t>(frame + 21));
        PUBLISH_STATE(this->pmc_5_0_sensor_, encode_value_le<uint16_t>(frame + 23));
        PUBLISH_STATE(this->pmc_10_0_sensor_, encode_value_le<uint16_t>(frame + 25));
      },
      avail);
}

bool GCJA5Component::check_frame_(const uint8_t *frame) {
  ESP_LOGVV(TAG, "GCJA5 Data: ");
  for (uint8_t i = 0; i < FRAME_SIZE; i++) {
    ESP_LOGVV(TAG, "  %u: 0b" BYTE_TO_BINARY_PATTERN " (0x%02X)", i + 1, BYTE_TO_BINARY(frame[i]), frame[i]);
  }

  // The header byte 0x02 is checked by the reader
  uint8_t crc = xor_bytes(frame + 1, 29);
  ESP_LOGVV(TAG, "Checksum packet was (0x%02X), calculated checksum was (0x%02X)", frame[30], crc);
  if (frame[31] != 0x03 || crc != frame[30]) {
    ESP_LOGVV(TAG, "Discarding bad packet - failed checks.");
    return false;
  }
  ESP_LOGVV(TAG, "Good packet found.");

  uint8_t status = frame[29];
  if (!this->first_status_log_) {
    this->first_status_log_ = true;

    ESP_LOGI(TAG,
             "GCJA5 Status\n"
             "  Overall Status : %i\n"
             "  PD Status      : %i\n"
             "  LD Status      : %i\n"
             "  Fan Status     : %i",
             (status >> 6) & 0x03, (status >> 4) & 0x03, (status >> 2) & 0x03, (status >> 0) & 0x03);
  }
  return true;
}

void GCJA5Component::dump_config() { ; }

}  // namespace esphome::gcja5
