/* Official Datasheet:
 *   https://maxbotix.com/pages/xl-tanksensor-wr-datasheet
 *
 * This implementation is designed to work with the TTL Versions of the
 * MaxBotix XL Tanksensor WR sensor series. The sensor's TTL Pin (5) should be
 * wired to one of the ESP's input pins and configured as uart rx_pin.
 *
 * The sensor outputs measurements at a fixed rate, typically ~1-4 Hz.
 *
 * Data format:
 * "R"
 * 3-5 digits (MB7850: cm, MB7851/MB7853/MB7854: mm)
 * <SPACE>
 * "T"
 * 3 digits (teach confidence)
 * <CR>
 *
 * Example: "R0743 T098\r" at a fixed rate. Where
 * 0743 means a distance of 743 mm and T098 is 98% "teach confidence".
 */

#include "xl_tanksensor_wr.h"
#include "esphome/core/log.h"

namespace esphome {
namespace xl_tanksensor_wr {

static const char *const TAG = "xl.tanksensor.wr.sensor";
static const uint8_t ASCII_CR = 0x0D;
static const uint8_t ASCII_NBSP = 0xFF;
static const int MAX_DATA_LENGTH_BYTES = 11;

void XlTanksensorWrComponent::loop() {
  uint8_t data;
  while (this->available() > 0) {
    if (this->read_byte(&data)) {
      buffer_ += (char) data;
      this->check_buffer_();
    }
  }
}

void XlTanksensorWrComponent::check_buffer_() {
  // The sensor seems to inject a rogue ASCII 255 byte from time to time. Get rid of that.
  // (This hack/fix was copied from the hrxl_maxsonar_wr component, and may not be true here)
  if (this->buffer_.back() == static_cast<char>(ASCII_NBSP)) {
    this->buffer_.pop_back();
    return;
  }

  // Stop reading at ASCII_CR. Also prevent the buffer from growing
  // indefinitely if no ASCII_CR is received after MAX_DATA_LENGTH_BYTES.
  if (this->buffer_.back() == static_cast<char>(ASCII_CR) || this->buffer_.length() >= MAX_DATA_LENGTH_BYTES) {
    ESP_LOGV(TAG, "Read from serial: %s", this->buffer_.c_str());

    if (this->buffer_.length() >= 6 && // Absolute minimum should be "R0 T0\r"
        this->buffer_[0] == 'R' &&
        this->buffer_.back() == static_cast<char>(ASCII_CR))
    {
      int millimeters = 0;
      float meters = 0;
      for (size_t spaceIndex = 0; spaceIndex < this->buffer_.length(); ++spaceIndex) {
        if (this->buffer_[spaceIndex] == ' ') {
          millimeters = parse_number<int>(this->buffer_.substr(1, spaceIndex-1)).value_or(0);
          meters = float(millimeters) / 1000.0;
          break;
        }
      }
      ESP_LOGV(TAG, "Distance from sensor: %d mm, %f m", millimeters, meters);
      this->publish_state(meters);
    } else {
      ESP_LOGW(TAG, "Invalid data read from sensor: %s", this->buffer_.c_str());
    }
    this->buffer_.clear();
  }
}

void XlTanksensorWrComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "XL Tanksensor WR Sensor:");
  LOG_SENSOR("  ", "Distance", this);
  // As specified in the sensor's data sheet
  this->check_uart_settings(9600, 1, esphome::uart::UART_CONFIG_PARITY_NONE, 8);
}

}  // namespace xl_tanksensor_wr
}  // namespace esphome
