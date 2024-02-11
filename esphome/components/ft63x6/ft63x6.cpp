/**************************************************************************/
/*!
  Author: Gustavo Ambrozio
  Based on work by: Atsushi Sasaki (https://github.com/aselectroworks/Arduino-FT6336U)
*/
/**************************************************************************/

#include "ft63x6.h"
#include "esphome/core/log.h"

// Registers
// Reference: https://focuslcds.com/content/FT6236.pdf
namespace esphome {
namespace ft63x6 {

static const uint8_t FT63X6_ADDR_TOUCH1_STATE = 0x03;
static const uint8_t FT63X6_ADDR_TOUCH1_X = 0x03;
static const uint8_t FT63X6_ADDR_TOUCH1_ID = 0x05;
static const uint8_t FT63X6_ADDR_TOUCH1_Y = 0x05;

static const uint8_t FT63X6_ADDR_TOUCH2_STATE = 0x09;
static const uint8_t FT63X6_ADDR_TOUCH2_X = 0x09;
static const uint8_t FT63X6_ADDR_TOUCH2_ID = 0x0B;
static const uint8_t FT63X6_ADDR_TOUCH2_Y = 0x0B;

static const char *const TAG = "FT63X6Touchscreen";

void FT63X6Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up FT63X6Touchscreen Touchscreen...");
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->hard_reset_();
  }

  // Get touch resolution
  if (this->x_raw_max_ == this->x_raw_min_) {
    this->x_raw_max_ = 320;
  }
  if (this->y_raw_max_ == this->y_raw_min_) {
    this->y_raw_max_ = 480;
  }

  uint8_t chip_id;

  this->read_bytes(0xA3, (uint8_t *) &chip_id, 1);

  if (chip_id != 0) {
    ESP_LOGI(TAG, "FT6336U touch driver started chipid: %d", chip_id);
  } else {
    ESP_LOGE(TAG, "FT6336U touch driver failed to start");
  }
}

void FT63X6Touchscreen::hard_reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
  }
}

void FT63X6Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "FT63X6 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void FT63X6Touchscreen::update_touches() {
  uint8_t data[15];
  uint16_t touch_id, x, y;

  if (!this->read_bytes(0x00, (uint8_t *) &data, 15)) {
    ESP_LOGE(TAG, "Failed to read touch data");
    this->skip_update_ = true;
    return;
  }

  if (((data[0x08] & 0x0f) != 0x0f) || ((data[0x0E] & 0x0f) != 0x0f)) {
    ESP_LOGVV(TAG, "Data does not look okey. lets wait. %d/%d", (data[0x08] & 0x0f), (data[0x0E] & 0x0f));
    this->skip_update_ = true;
    return;
  }

  if ((data[0x02] & 0x0f) == 0x00) {
    ESP_LOGD(TAG, "No touches detected");
    return;
  }
  ESP_LOGI(TAG, "Touches found: %d", data[0x02] & 0x0f);

  if (((data[FT63X6_ADDR_TOUCH1_STATE] >> 6) & 0x01) == 0) {  // checking event flag bit 6 if it is null
    touch_id = data[FT63X6_ADDR_TOUCH1_ID] >> 4;  // id1 = 0 or 1
    x = encode_uint16(data[FT63X6_ADDR_TOUCH1_X] & 0x0F, data[FT63X6_ADDR_TOUCH1_X + 1]);
    y = encode_uint16(data[FT63X6_ADDR_TOUCH1_Y] & 0x0F, data[FT63X6_ADDR_TOUCH1_Y + 1]);
    if ((x == 0) && (y==0)) {
      ESP_LOGW(TAG, "Reporting a (0,0) touch");
    }
    this->add_raw_touch_position_(touch_id, x, y, data[0x07]);
  }
  if (((data[FT63X6_ADDR_TOUCH2_STATE] >> 6) & 0x01) == 0) { // checking event flag bit 6 if it is null
    touch_id = data[FT63X6_ADDR_TOUCH2_ID] >> 4;  // id1 = 0 or 1
    x = encode_uint16(data[FT63X6_ADDR_TOUCH2_X] & 0x0F, data[FT63X6_ADDR_TOUCH2_X + 1]);
    y = encode_uint16(data[FT63X6_ADDR_TOUCH2_Y] & 0x0F, data[FT63X6_ADDR_TOUCH2_Y + 1]);
    this->add_raw_touch_position_(touch_id, x, y, data[0x0D]);
  }
}

}  // namespace ft63x6
}  // namespace esphome
