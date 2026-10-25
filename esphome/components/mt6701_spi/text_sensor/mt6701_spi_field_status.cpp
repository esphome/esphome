#include "mt6701_spi_field_status.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::mt6701_spi {

ESPHOME_LOG_TAG(TAG, "mt6701_spi.text_sensor");

// Indexed by Mg[1:0]; 0b11 is reserved in the datasheet
PROGMEM_STRING_TABLE(FieldStatusStrings, "OK", "TOO_STRONG", "TOO_WEAK", "UNKNOWN");

void MT6701SPIFieldStatus::update() {
  if (!this->parent_->read_encoder())
    return;
  char text[FieldStatusStrings::MAX_LENGTH + 1];
  ESPHOME_strncpy_P(text,
                    reinterpret_cast<ESPHOME_PGM_P>(FieldStatusStrings::get_progmem_str(
                        this->parent_->get_status() & 0x03, FieldStatusStrings::LAST_INDEX)),
                    sizeof(text));
  this->publish_state(text);
}

void MT6701SPIFieldStatus::dump_config() {
  LOG_TEXT_SENSOR("", "MT6701 field status", this);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::mt6701_spi
