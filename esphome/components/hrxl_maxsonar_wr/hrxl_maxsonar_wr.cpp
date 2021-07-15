// Official Datasheet:
//   https://www.maxbotix.com/documents/HRXL-MaxSonar-WR_Datasheet.pdf
//
// This implementation is designed to work with the TTL Versions of the
// MaxBotix HRXL MaxSonar WR sensor series. The sensor's TTL Pin (5) should be
// wired to one of the ESP's input pins and configured as uart rx_pin.

#include "hrxl_maxsonar_wr.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hrxlmaxsonarwr {

static const char *const TAG = "hrxl.maxsonar.wr.sensor";
static const uint8_t ASCII_CR = 0x0D;
static const int MAX_DATA_LENGTH_BYTES = 6;

/**
 * The sensor outputs something like "R1234\r" at a fixed rate of 6 Hz. Where
 * 1234 means a distance of 1,234 m.
 */
void HrxlMaxsonarWrComponent::loop() {
  std::string buffer;
  while (this->available() > 0) {
    uint8_t data;
    for (int i = 0; i < MAX_DATA_LENGTH_BYTES; i++) {
      if (this->read_byte(&data)) {
        if (data == ASCII_CR) {
          break;
        } else {
          buffer += (char) data;
        }
      } else {
        break;
      }
    }

    ESP_LOGV(TAG, "Read from serial: %s", buffer.c_str());
    if (buffer.length() == (MAX_DATA_LENGTH_BYTES - 1) && buffer[0] == 'R') {
      int millimeters = atoi(buffer.substr(1).c_str());
      float meters = float(millimeters) / 1000.0;
      ESP_LOGV(TAG, "Distance from sensor: %u mm, %f m", millis, meters);
      this->publish_state(meters);
    } else {
      ESP_LOGW(TAG, "Invalid data read from sensor: %s", buffer.c_str());
    }
  }
}

void HrxlMaxsonarWrComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HRXL MaxSonar WR Sensor:");
  LOG_SENSOR("  ", "Distance", this);
  // As specified in the sensor's data sheet
  this->check_uart_settings(9600, 1, esphome::uart::UART_CONFIG_PARITY_NONE, 8);
}

}  // namespace hrxlmaxsonarwr
}  // namspace esphome
