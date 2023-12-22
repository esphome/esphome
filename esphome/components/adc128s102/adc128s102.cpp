#include "adc128s102.h"
#include "esphome/core/log.h"

namespace esphome {
namespace adc128s102 {

static const char *const TAG = "adc128s102";

float ADC128S102::get_setup_priority() const { return setup_priority::HARDWARE; }

void ADC128S102::setup() {
  ESP_LOGCONFIG(TAG, "Setting up adc128s102");
  this->spi_setup();
}

void ADC128S102::dump_config() {
  ESP_LOGCONFIG(TAG, "ADC128S102:");
  LOG_PIN("  CS Pin:", this->cs_);
}

uint16_t ADC128S102::read_data(uint8_t channel) {
  uint8_t control = channel << 3;

  this->enable();
  uint8_t adc_primary_byte = this->transfer_byte(control);
  uint8_t adc_secondary_byte = this->transfer_byte(0x00);
  this->disable();

  uint16_t digital_value = adc_primary_byte << 8 | adc_secondary_byte;

  return digital_value;
}

}  // namespace adc128s102
}  // namespace esphome
