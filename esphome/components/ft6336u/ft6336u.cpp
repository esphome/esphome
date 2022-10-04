/**************************************************************************/
/*!
  Author: Gustavo Ambrozio
  Based on work by: Atsushi Sasaki (https://github.com/aselectroworks/Arduino-FT6336U)
*/
/**************************************************************************/

#include "ft6336u.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// Registers
static const uint8_t FT6336U_ADDR_TD_STATUS = 0x02;

static const uint8_t FT6336U_ADDR_TOUCH1_ID = 0x05;
static const uint8_t FT6336U_ADDR_TOUCH1_X = 0x03;
static const uint8_t FT6336U_ADDR_TOUCH1_Y = 0x05;

static const uint8_t FT6336U_ADDR_TOUCH2_ID = 0x0B;
static const uint8_t FT6336U_ADDR_TOUCH2_X = 0x09;
static const uint8_t FT6336U_ADDR_TOUCH2_Y = 0x0B;

namespace esphome {
namespace ft6336u {

static const char *const TAG = "FT6336UTouchscreen";

void FT6336UTouchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up FT6336UTouchscreen Touchscreen...");
  if (this->interrupt_pin_ != nullptr) {
		this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
		this->interrupt_pin_->setup();
	}

  if (this->reset_pin_ != nullptr) {
		this->reset_pin_->setup();
	}

  this->hard_reset_();

  // Get touch resolution
  this->x_resolution_ = 320;
  this->y_resolution_ = 480;

  this->set_power_state(true);
}

void FT6336UTouchscreen::loop() {
  FT6336U_TouchPointType tp = scan();

  if (tp.touch_count == 0) {
    for (auto *listener : this->touch_listeners_)
      listener->release();
    return;
  }

  std::vector<TouchPoint> touches;
  uint8_t touch_count = std::min<uint8_t>(tp.touch_count, 2);

	uint16_t w = this->display_->get_width_internal();
	uint16_t h = this->display_->get_height_internal()
  ESP_LOGV(TAG, "Touch count: %d", touch_count);

  for (int i = 0; i < touch_count; i++) {
    uint32_t raw_x = tp.tp[i].x * w / this->x_resolution_;
    uint32_t raw_y = tp.tp[i].y * h / this->y_resolution_;

    TouchPoint tp;
    // tp.x = raw_x;
    // tp.y = raw_y;
    switch (this->rotation_) {
      case ROTATE_0_DEGREES:
        tp.x = raw_x;
        tp.y = raw_y;
        break;
      case ROTATE_90_DEGREES:
        tp.x = raw_y;
        tp.y = w - std::min<uint32_t>(raw_x, w);
        break;
      case ROTATE_180_DEGREES:
        tp.x = w - std::min<uint32_t>(raw_x, w);
        tp.y = h - std::min<uint32_t>(raw_y, h);
        break;
      case ROTATE_270_DEGREES:
        tp.x = h - std::min<uint32_t>(raw_y, h);
        tp.y = raw_x;
        break;
    }

    this->defer([this, tp]() { this->send_touch_(tp); });
  }
}

void FT6336UTouchscreen::set_power_state(bool enable) {
}

bool FT6336UTouchscreen::get_power_state() {
  return true;
}

void FT6336UTouchscreen::hard_reset_() {
  if (this->reset_pin_ != nullptr) {
	  this->reset_pin_->digital_write(false);
	  delay(10);
	  this->reset_pin_->digital_write(true);
	}
}

void FT6336UTouchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "FT6336U Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

uint8_t FT6336UTouchscreen::read_td_status(void) {
  return readByte(FT6336U_ADDR_TD_STATUS);
}
// Touch 1 functions
uint16_t FT6336UTouchscreen::read_touch1_x(void) {
  uint8_t read_buf[2];
  read_buf[0] = readByte(FT6336U_ADDR_TOUCH1_X);
  read_buf[1] = readByte(FT6336U_ADDR_TOUCH1_X + 1);
	return ((read_buf[0] & 0x0f) << 8) | read_buf[1];
}
uint16_t FT6336UTouchscreen::read_touch1_y(void) {
  uint8_t read_buf[2];
  read_buf[0] = readByte(FT6336U_ADDR_TOUCH1_Y);
  read_buf[1] = readByte(FT6336U_ADDR_TOUCH1_Y + 1);
	return ((read_buf[0] & 0x0f) << 8) | read_buf[1];
}
uint8_t FT6336UTouchscreen::read_touch1_id(void) {
  return readByte(FT6336U_ADDR_TOUCH1_ID) >> 4;
}
// Touch 2 functions
uint16_t FT6336UTouchscreen::read_touch2_x(void) {
  uint8_t read_buf[2];
  read_buf[0] = readByte(FT6336U_ADDR_TOUCH2_X);
  read_buf[1] = readByte(FT6336U_ADDR_TOUCH2_X + 1);
	return ((read_buf[0] & 0x0f) << 8) | read_buf[1];
}
uint16_t FT6336UTouchscreen::read_touch2_y(void) {
  uint8_t read_buf[2];
  read_buf[0] = readByte(FT6336U_ADDR_TOUCH2_Y);
  read_buf[1] = readByte(FT6336U_ADDR_TOUCH2_Y + 1);
	return ((read_buf[0] & 0x0f) << 8) | read_buf[1];
}
uint8_t FT6336UTouchscreen::read_touch2_id(void) {
  return readByte(FT6336U_ADDR_TOUCH2_ID) >> 4;
}

//coordinate diagram（FPC downwards）
////y ////////////////////264x176
						//
						//
						//x
						//
						//
FT6336U_TouchPointType FT6336UTouchscreen::scan(void){
  touchPoint.touch_count = read_td_status();

  if(touchPoint.touch_count == 0) {
    touchPoint.tp[0].status = release;
    touchPoint.tp[1].status = release;
  }
  else if(touchPoint.touch_count == 1) {
    uint8_t id1 = read_touch1_id(); // id1 = 0 or 1
    touchPoint.tp[id1].status = (touchPoint.tp[id1].status == release) ? touch : stream;
    touchPoint.tp[id1].x = read_touch1_x();
    touchPoint.tp[id1].y = read_touch1_y();
    touchPoint.tp[~id1 & 0x01].status = release;
  }
  else {
    uint8_t id1 = read_touch1_id(); // id1 = 0 or 1
    touchPoint.tp[id1].status = (touchPoint.tp[id1].status == release) ? touch : stream;
    touchPoint.tp[id1].x = read_touch1_x();
    touchPoint.tp[id1].y = read_touch1_y();
    uint8_t id2 = read_touch2_id(); // id2 = 0 or 1(~id1 & 0x01)
    touchPoint.tp[id2].status = (touchPoint.tp[id2].status == release) ? touch : stream;
    touchPoint.tp[id2].x = read_touch2_x();
    touchPoint.tp[id2].y = read_touch2_y();
  }

  return touchPoint;
}

// Private Function
uint8_t FT6336UTouchscreen::readByte(uint8_t addr) {
  uint8_t rdData = 0;
  this->read_byte(addr, &rdData);
  return rdData;
}

void FT6336UTouchscreen::writeByte(uint8_t addr, uint8_t data) {
	this->write_byte(addr, data);
  ESP_LOGD(TAG, "writeI2C reg 0x%x -> 0x%x", addr, data);
}

}  // namespace FT6336UTouchscreen
}  // namespace esphome
