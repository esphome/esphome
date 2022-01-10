// Official Datasheet:
//   https://www.maxbotix.com/documents/HRXL-MaxSonar-WR_Datasheet.pdf
//
// This implementation is designed to work with the TTL Versions of the
// MaxBotix HRXL MaxSonar WR sensor series. The sensor's TTL Pin (5) should be
// wired to one of the ESP's input pins and configured as uart rx_pin.

#include "hrxl_maxsonar_wr.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hrxl_maxsonar_wr {

static const char *const TAG = "hrxl.maxsonar.wr.sensor";
static const uint8_t ASCII_CR = 0x0D;
static const uint8_t ASCII_NBSP = 0xFF;
static const int MAX_DATA_LENGTH_BYTES = 6;

/**
 * The sensor outputs something like "R1234\r" at a fixed rate of 6 Hz. Where
 * 1234 means a distance of 1,234 m.
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

    if (this->buffer_.length() == MAX_DATA_LENGTH_BYTES && this->buffer_[0] == 'R' &&
        this->buffer_.back() == static_cast<char>(ASCII_CR)) {
      int millimeters = parse_number<int>(this->buffer_.substr(1, MAX_DATA_LENGTH_BYTES - 2)).value_or(0);
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
