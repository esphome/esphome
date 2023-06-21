/* From snooping with a logic analyzer, the I2C on this sensor is broken. I was only able
 * to receive 1's as a response from the sensor. I was able to get the UART working.
 */
#include "gcja5.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace gcja5 {

static const char *const TAG = "gcja5";

void GCJA5Component::setup() { ESP_LOGCONFIG(TAG, "Setting up gcja5..."); }

void GCJA5Component::loop() {
  const uint32_t now = millis();
  if (now - this->last_transmission_ >= 500) {
    // last transmission too long ago. Reset RX index.
    this->rx_message_.clear();
  }

  if (this->available() == 0) {
    return;
  }

  // There must now be data waiting
  this->last_transmission_ = now;
  uint8_t val;
  while (this->available() != 0) {
    this->read_byte(&val);
    this->rx_message_.push_back(val);

    // check if rx_message_ has 32 bytes of data
    if (this->rx_message_.size() == 32) {
      this->parse_data_();
      this->rx_message_.clear();
    }
  }
}


bool GCJA5Component::calculate_checksum() {
  uint8_t crc = 0;

  for (uint8_t i = 1; i < 30; i++)
    crc = crc ^ this->rx_message_[i];

  ESP_LOGVV(TAG, "Checksum packet was (0x%02X), calculated checksum was (0x%02X)", this->rx_message_[30], crc);

  if (crc == this->rx_message_[30])
    return true;

  return false;
}

uint32_t GCJA5Component::get_32_bit_uint(uint8_t start_index) {
  return (((uint32_t) this->rx_message_[start_index + 3]) << 24) | (((uint32_t) this->rx_message_[start_index + 2]) << 16) |
         (((uint32_t) this->rx_message_[start_index + 1]) << 8) | ((uint32_t) this->rx_message_[start_index]);
}

uint16_t GCJA5Component::get_16_bit_uint(uint8_t start_index) {
  return (((uint32_t) this->rx_message_[start_index + 1]) << 8) | ((uint32_t) this->rx_message_[start_index]);
}

void GCJA5Component::parse_data_() {
  ESP_LOGVV(TAG, "GCJA5 Data: ");
  for (uint8_t i = 0; i < 32; i++) {
    ESP_LOGVV(TAG, "  %u: 0b" BYTE_TO_BINARY_PATTERN " (0x%02X)", i + 1, BYTE_TO_BINARY(this->rx_message_[i]),
              this->rx_message_[i]);
  }

  if (this->rx_message_[0] != 0x02 || this->rx_message_[31] != 0x03 || !this->calculate_checksum()) {
    ESP_LOGVV(TAG, "Discarding bad packet - failed checks.");
    return;
  } else
    ESP_LOGVV(TAG, "Good packet found.");

  haveGoodData = true;

  uint32_t pm1_0 = get_32_bit_uint(1);
  uint32_t pm2_5 = get_32_bit_uint(5);
  uint32_t pm10_0 = get_32_bit_uint(9);

  /*uint16_t pmc0_5 = get_16_bit_uint(13);
  uint16_t pmc1_0 = get_16_bit_uint(15);
  uint16_t pmc2_5 = get_16_bit_uint(17);

  uint16_t pmc5_0 = get_16_bit_uint(21);
  uint16_t pmc7_5 = get_16_bit_uint(23);
  uint16_t pmc10_0 = get_16_bit_uint(25);
  */
  
  uint8_t status = this->rx_message_[29];
  if (!firstStatusLog) {
    firstStatusLog = true;

    ESP_LOGI(TAG, "GCJA5 Status");
    ESP_LOGI(TAG, "Overall Status : %i", (status >> 6) & 0x03);
    ESP_LOGI(TAG, "PD Status      : %i", (status >> 4) & 0x03);
    ESP_LOGI(TAG, "LD Status      : %i", (status >> 2) & 0x03);
    ESP_LOGI(TAG, "Fan Status     : %i", (status >> 0) & 0x03);
  }
}

void GCJA5Component::dump_config() { ; }

void GCJA5Component::update() {
  if (haveGoodData) {
    if (this->pm_1_0_sensor_ != nullptr)
      this->pm_1_0_sensor_->publish_state(get_32_bit_uint(1));
    if (this->pm_2_5_sensor_ != nullptr)
      this->pm_2_5_sensor_->publish_state(get_32_bit_uint(5));
    if (this->pm_10_0_sensor_ != nullptr)
      this->pm_10_0_sensor_->publish_state(get_32_bit_uint(9));

    if (this->pmc_0_3_sensor_ != nullptr)
      this->pmc_0_3_sensor_->publish_state(get_16_bit_uint(13));
    if (this->pmc_0_5_sensor_ != nullptr)
      this->pmc_0_5_sensor_->publish_state(get_16_bit_uint(15));
    if (this->pmc_1_0_sensor_ != nullptr)
      this->pmc_1_0_sensor_->publish_state(get_16_bit_uint(17));
    if (this->pmc_2_5_sensor_ != nullptr)
      this->pmc_2_5_sensor_->publish_state(get_16_bit_uint(21));
    if (this->pmc_5_0_sensor_ != nullptr)
      this->pmc_5_0_sensor_->publish_state(get_16_bit_uint(23));
    if (this->pmc_10_0_sensor_ != nullptr)
      this->pmc_10_0_sensor_->publish_state(get_16_bit_uint(25));
  } else {
    this->status_set_warning();
    ESP_LOGV(TAG, "No data. Skipping update.");
  }
}
}  // namespace gcja5
}  // namespace esphome
