// Official Datasheet:
//   HRXL: https://www.maxbotix.com/documents/HRXL-MaxSonar-WR_Datasheet.pdf
//   XL: https://www.maxbotix.com/documents/XL-MaxSonar-WR_Datasheet.pdf
//
// This implementation is designed to work with the TTL Versions of the
// MaxBotix HRXL and XL MaxSonar WR sensor series. The sensor's TTL Pin (5)
// should be wired to one of the ESP's input pins and configured as uart rx_pin.

#include "hrxl_maxsonar_wr.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hrxl_maxsonar_wr {

static const char *const TAG = "hrxl.maxsonar.wr.sensor";
static const uint8_t ASCII_CR = 0x0D;
static const uint8_t ASCII_NBSP = 0xFF;
static const int MAX_DATA_LENGTH_BYTES = 6;

/**
 * HRXL sensors output the format "R1234\r" at 6Hz
 * The 1234 means 1234mm
 * XL sensors output the format "R123\r" at 5 to 10Hz
 * The 123 means 123cm
 */
void HrxlMaxsonarWrComponent::loop() {
  uint8_t data;
  while (this->available() > 0) {
    if (this->read_byte(&data)) {
      buffer_ += (char) data;
      this->check_buffer_();
    }
  }
}

void HrxlMaxsonarWrComponent::check_buffer_() {
  // The sensor seems to inject a rogue ASCII 255 byte from time to time. Get rid of that.
  if (this->buffer_.back() == static_cast<char>(ASCII_NBSP)) {
    this->buffer_.pop_back();
    return;
  }

  // Stop reading at ASCII_CR. Also prevent the buffer from growing
  // indefinitely if no ASCII_CR is received after MAX_DATA_LENGTH_BYTES.
  if (this->buffer_.back() == static_cast<char>(ASCII_CR) || this->buffer_.length() >= MAX_DATA_LENGTH_BYTES) {
    ESP_LOGV(TAG, "Read from serial: %s", this->buffer_.c_str());

    size_t rpos = this->buffer_.find(static_cast<char>(ASCII_CR));

    if (this->buffer_.length() <= MAX_DATA_LENGTH_BYTES && this->buffer_[0] == 'R' && rpos != std::string::npos) {
      std::string distance = this->buffer_.substr(1, rpos - 1);
      int millimeters = parse_number<int>(distance).value_or(0);

      // XL reports in cm instead of mm and reports 3 digits instead of 4
      if (distance.length() == 3) {
        millimeters = millimeters * 10;
      }

      float meters = float(millimeters) / 1000.0;
      ESP_LOGV(TAG, "Distance from sensor: %d mm, %f m", millimeters, meters);
      this->publish_state(meters);
    } else {
      ESP_LOGW(TAG, "Invalid data read from sensor: %s", this->buffer_.c_str());
    }
    this->buffer_.clear();
  }
}

void HrxlMaxsonarWrComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HRXL MaxSonar WR Sensor:");
  LOG_SENSOR("  ", "Distance", this);
  // As specified in the sensor's data sheet
  this->check_uart_settings(9600, 1, esphome::uart::UART_CONFIG_PARITY_NONE, 8);
}

}  // namespace hrxl_maxsonar_wr
}  // namespace esphome
