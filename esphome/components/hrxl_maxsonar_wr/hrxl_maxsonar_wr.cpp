// Official Datasheet:
//   HRXL: https://www.maxbotix.com/documents/HRXL-MaxSonar-WR_Datasheet.pdf
//   XL: https://www.maxbotix.com/documents/XL-MaxSonar-WR_Datasheet.pdf
//
// This implementation is designed to work with the TTL Versions of the
// MaxBotix HRXL and XL MaxSonar WR sensor series. The sensor's TTL Pin (5)
// should be wired to one of the ESP's input pins and configured as uart rx_pin.

#include "hrxl_maxsonar_wr.h"
#include "esphome/core/log.h"

namespace esphome::hrxl_maxsonar_wr {

ESPHOME_LOG_TAG(TAG, "hrxl.maxsonar.wr.sensor");

/**
 * HRXL sensors output the format "R1234\r" at 6Hz
 * The 1234 means 1234mm
 * XL sensors output the format "R123\r" at 5 to 10Hz
 * The 123 means 123cm
 */
void HrxlMaxsonarWrComponent::loop() {
  uint8_t data;
  while (this->available() > 0) {
    if (!this->read_byte(&data))
      continue;
    const char *line = this->line_reader_.feed(data);
    if (line != nullptr) {
      ESP_LOGV(TAG, "Read from serial: %s", line);
      this->handle_line_(line, this->line_reader_.length());
    } else if (this->line_reader_.overflowed()) {
      // No CR in time: report it and start over with the next byte
      ESP_LOGW(TAG, "Invalid data read from sensor: %s", this->line_reader_.partial());
      this->line_reader_.reset();
    }
  }
}

void HrxlMaxsonarWrComponent::handle_line_(const char *line, size_t len) {
  if (line[0] != 'R') {
    ESP_LOGW(TAG, "Invalid data read from sensor: %s", line);
    return;
  }
  int millimeters = parse_number<int>(line + 1).value_or(0);

  // XL reports in cm instead of mm and reports 3 digits instead of 4
  if (len - 1 == 3) {
    millimeters = millimeters * 10;
  }

  float meters = float(millimeters) / 1000.0f;
  ESP_LOGV(TAG, "Distance from sensor: %d mm, %f m", millimeters, meters);
  this->publish_state(meters);
}

void HrxlMaxsonarWrComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HRXL MaxSonar WR Sensor:");
  LOG_SENSOR("  ", "Distance", this);
}

}  // namespace esphome::hrxl_maxsonar_wr
