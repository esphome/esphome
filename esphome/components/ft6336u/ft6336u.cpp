/**************************************************************************/
/*!
  @file     FT6336UTouchscreen.cpp
  Author: Gustavo Ambrozio
  Based on work by: Atsushi Sasaki(https://github.com/aselectroworks)
  License: MIT (see LICENSE)
*/
/**************************************************************************/

#include "ft6336u.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ft6336u {

static const char *const TAG = "FT6336UTouchscreen";

void FT6336UTouchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up FT6336UTouchscreen Touchscreen...");
  this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->interrupt_pin_->setup();

  this->rts_pin_->setup();

  this->hard_reset_();

  // Get touch resolution
  this->x_resolution_ = 320;
  this->y_resolution_ = 480;

  this->set_power_state(true);
  ESP_LOGCONFIG(TAG, "display_height = %d, display_width = %d", this->display_height_, this->display_width_);
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
  this->rts_pin_->digital_write(false);
  delay(10);
  this->rts_pin_->digital_write(true);
  delay(500);
}

void FT6336UTouchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "FT6336U Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  RTS Pin: ", this->rts_pin_);
}

uint8_t FT6336UTouchscreen::read_device_mode(void) {
    return (readByte(FT6336U_ADDR_DEVICE_MODE) & 0x70) >> 4;
}
void FT6336UTouchscreen::write_device_mode(DEVICE_MODE_Enum mode) {
    writeByte(FT6336U_ADDR_DEVICE_MODE, (mode & 0x07) << 4);
}
uint8_t FT6336UTouchscreen::read_gesture_id(void) {
    return readByte(FT6336U_ADDR_GESTURE_ID);
}
uint8_t FT6336UTouchscreen::read_td_status(void) {
    return readByte(FT6336U_ADDR_TD_STATUS);
}
uint8_t FT6336UTouchscreen::read_touch_number(void) {
    return readByte(FT6336U_ADDR_TD_STATUS) & 0x0F;
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
uint8_t FT6336UTouchscreen::read_touch1_event(void) {
    return readByte(FT6336U_ADDR_TOUCH1_EVENT) >> 6;
}
uint8_t FT6336UTouchscreen::read_touch1_id(void) {
    return readByte(FT6336U_ADDR_TOUCH1_ID) >> 4;
}
uint8_t FT6336UTouchscreen::read_touch1_weight(void) {
    return readByte(FT6336U_ADDR_TOUCH1_WEIGHT);
}
uint8_t FT6336UTouchscreen::read_touch1_misc(void) {
    return readByte(FT6336U_ADDR_TOUCH1_MISC) >> 4;
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
uint8_t FT6336UTouchscreen::read_touch2_event(void) {
    return readByte(FT6336U_ADDR_TOUCH2_EVENT) >> 6;
}
uint8_t FT6336UTouchscreen::read_touch2_id(void) {
    return readByte(FT6336U_ADDR_TOUCH2_ID) >> 4;
}
uint8_t FT6336UTouchscreen::read_touch2_weight(void) {
    return readByte(FT6336U_ADDR_TOUCH2_WEIGHT);
}
uint8_t FT6336UTouchscreen::read_touch2_misc(void) {
    return readByte(FT6336U_ADDR_TOUCH2_MISC) >> 4;
}

// Mode Parameter Register
uint8_t FT6336UTouchscreen::read_touch_threshold(void) {
    return readByte(FT6336U_ADDR_THRESHOLD);
}
uint8_t FT6336UTouchscreen::read_filter_coefficient(void) {
    return readByte(FT6336U_ADDR_FILTER_COE);
}
uint8_t FT6336UTouchscreen::read_ctrl_mode(void) {
    return readByte(FT6336U_ADDR_CTRL);
}
void FT6336UTouchscreen::write_ctrl_mode(CTRL_MODE_Enum mode) {
    writeByte(FT6336U_ADDR_CTRL, mode);
}
uint8_t FT6336UTouchscreen::read_time_period_enter_monitor(void) {
    return readByte(FT6336U_ADDR_TIME_ENTER_MONITOR);
}
uint8_t FT6336UTouchscreen::read_active_rate(void) {
    return readByte(FT6336U_ADDR_ACTIVE_MODE_RATE);
}
uint8_t FT6336UTouchscreen::read_monitor_rate(void) {
    return readByte(FT6336U_ADDR_MONITOR_MODE_RATE);
}

// Gesture Parameters
uint8_t FT6336UTouchscreen::read_radian_value(void) {
	return readByte(FT6336U_ADDR_RADIAN_VALUE);
}
void FT6336UTouchscreen::write_radian_value(uint8_t val) {
	writeByte(FT6336U_ADDR_RADIAN_VALUE, val); 
}
uint8_t FT6336UTouchscreen::read_offset_left_right(void) {
	return readByte(FT6336U_ADDR_OFFSET_LEFT_RIGHT);
}
void FT6336UTouchscreen::write_offset_left_right(uint8_t val) {
	writeByte(FT6336U_ADDR_OFFSET_LEFT_RIGHT, val); 
}
uint8_t FT6336UTouchscreen::read_offset_up_down(void) {
	return readByte(FT6336U_ADDR_OFFSET_UP_DOWN);
}
void FT6336UTouchscreen::write_offset_up_down(uint8_t val) {
	writeByte(FT6336U_ADDR_OFFSET_UP_DOWN, val); 
}
uint8_t FT6336UTouchscreen::read_distance_left_right(void) {
	return readByte(FT6336U_ADDR_DISTANCE_LEFT_RIGHT);
}
void FT6336UTouchscreen::write_distance_left_right(uint8_t val) {
	writeByte(FT6336U_ADDR_DISTANCE_LEFT_RIGHT, val); 
}
uint8_t FT6336UTouchscreen::read_distance_up_down(void) {
	return readByte(FT6336U_ADDR_DISTANCE_UP_DOWN);
}
void FT6336UTouchscreen::write_distance_up_down(uint8_t val) {
	writeByte(FT6336U_ADDR_DISTANCE_UP_DOWN, val); 
}
uint8_t FT6336UTouchscreen::read_distance_zoom(void) {
	return readByte(FT6336U_ADDR_DISTANCE_ZOOM);
}
void FT6336UTouchscreen::write_distance_zoom(uint8_t val) {
	writeByte(FT6336U_ADDR_DISTANCE_ZOOM, val); 
}


// System Information
uint16_t FT6336UTouchscreen::read_library_version(void) {
    uint8_t read_buf[2];
    read_buf[0] = readByte(FT6336U_ADDR_LIBRARY_VERSION_H);
    read_buf[1] = readByte(FT6336U_ADDR_LIBRARY_VERSION_L);
	return ((read_buf[0] & 0x0f) << 8) | read_buf[1];
}
uint8_t FT6336UTouchscreen::read_chip_id(void) {
    return readByte(FT6336U_ADDR_CHIP_ID);
}
uint8_t FT6336UTouchscreen::read_g_mode(void) {
    return readByte(FT6336U_ADDR_G_MODE);
}
void FT6336UTouchscreen::write_g_mode(G_MODE_Enum mode){
	writeByte(FT6336U_ADDR_G_MODE, mode); 
}
uint8_t FT6336UTouchscreen::read_pwrmode(void) {
    return readByte(FT6336U_ADDR_POWER_MODE);
}
uint8_t FT6336UTouchscreen::read_firmware_id(void) {
    return readByte(FT6336U_ADDR_FIRMARE_ID);
}
uint8_t FT6336UTouchscreen::read_focaltech_id(void) {
    return readByte(FT6336U_ADDR_FOCALTECH_ID);
}
uint8_t FT6336UTouchscreen::read_release_code_id(void) {
    return readByte(FT6336U_ADDR_RELEASE_CODE_ID);
}
uint8_t FT6336UTouchscreen::read_state(void) {
    return readByte(FT6336U_ADDR_STATE);
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
