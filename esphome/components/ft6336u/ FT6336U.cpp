/**************************************************************************/
/*!
  @file     FT6336U.cpp
  Author: Atsushi Sasaki (https://github.com/aselectroworks)
  License: MIT (see LICENSE)
*/
/**************************************************************************/

#include "FT6336U.h"

#include <Wire.h>

FT6336U::FT6336U(uint8_t rst_n, uint8_t int_n) 
: rst_n(rst_n), int_n(int_n) {
}
#if defined(ESP32) || defined(ESP8266)
FT6336U::FT6336U(int8_t sda, int8_t scl, uint8_t rst_n, uint8_t int_n) 
: sda(sda), scl(scl), rst_n(rst_n), int_n(int_n)  {
}
#endif
FT6336U::~FT6336U() {
}


void FT6336U::begin(void) {
    // Initialize I2C
#if defined(ESP32) || defined(ESP8266)
    if(sda != -1 && scl != -1) {
        Wire.begin(sda, scl); 
    }
    else {
        Wire.begin(); 
    }
#else 
    Wire.begin(); 
#endif
	// Int Pin Configuration
	pinMode(int_n, INPUT); 
    // Reset Pin Configuration
	pinMode(rst_n, OUTPUT); 
	digitalWrite(rst_n, LOW); 
	delay(10); 
	digitalWrite(rst_n, HIGH); 
	delay(500); 
}
uint8_t FT6336U::read_device_mode(void) {
    return (readByte(FT6336U_ADDR_DEVICE_MODE) & 0x70) >> 4;
}
void FT6336U::write_device_mode(DEVICE_MODE_Enum mode) {
    writeByte(FT6336U_ADDR_DEVICE_MODE, (mode & 0x07) << 4);
}
uint8_t FT6336U::read_gesture_id(void) {
    return readByte(FT6336U_ADDR_GESTURE_ID);
}
uint8_t FT6336U::read_td_status(void) {
    return readByte(FT6336U_ADDR_TD_STATUS);
}
uint8_t FT6336U::read_touch_number(void) {
    return readByte(FT6336U_ADDR_TD_STATUS) & 0x0F;
}
// Touch 1 functions
uint16_t FT6336U::read_touch1_x(void) {
    uint8_t read_buf[2];
    read_buf[0] = readByte(FT6336U_ADDR_TOUCH1_X);
    read_buf[1] = readByte(FT6336U_ADDR_TOUCH1_X + 1);
	return ((read_buf[0] & 0x0f) << 8) | read_buf[1];
}
uint16_t FT6336U::read_touch1_y(void) {
    uint8_t read_buf[2];
    read_buf[0] = readByte(FT6336U_ADDR_TOUCH1_Y);
    read_buf[1] = readByte(FT6336U_ADDR_TOUCH1_Y + 1);
	return ((read_buf[0] & 0x0f) << 8) | read_buf[1];
}
uint8_t FT6336U::read_touch1_event(void) {
    return readByte(FT6336U_ADDR_TOUCH1_EVENT) >> 6;
}
uint8_t FT6336U::read_touch1_id(void) {
    return readByte(FT6336U_ADDR_TOUCH1_ID) >> 4;
}
uint8_t FT6336U::read_touch1_weight(void) {
    return readByte(FT6336U_ADDR_TOUCH1_WEIGHT);
}
uint8_t FT6336U::read_touch1_misc(void) {
    return readByte(FT6336U_ADDR_TOUCH1_MISC) >> 4;
}
// Touch 2 functions
uint16_t FT6336U::read_touch2_x(void) {
    uint8_t read_buf[2];
    read_buf[0] = readByte(FT6336U_ADDR_TOUCH2_X);
    read_buf[1] = readByte(FT6336U_ADDR_TOUCH2_X + 1);
	return ((read_buf[0] & 0x0f) << 8) | read_buf[1];
}
uint16_t FT6336U::read_touch2_y(void) {
    uint8_t read_buf[2];
    read_buf[0] = readByte(FT6336U_ADDR_TOUCH2_Y);
    read_buf[1] = readByte(FT6336U_ADDR_TOUCH2_Y + 1);
	return ((read_buf[0] & 0x0f) << 8) | read_buf[1];
}
uint8_t FT6336U::read_touch2_event(void) {
    return readByte(FT6336U_ADDR_TOUCH2_EVENT) >> 6;
}
uint8_t FT6336U::read_touch2_id(void) {
    return readByte(FT6336U_ADDR_TOUCH2_ID) >> 4;
}
uint8_t FT6336U::read_touch2_weight(void) {
    return readByte(FT6336U_ADDR_TOUCH2_WEIGHT);
}
uint8_t FT6336U::read_touch2_misc(void) {
    return readByte(FT6336U_ADDR_TOUCH2_MISC) >> 4;
}

// Mode Parameter Register
uint8_t FT6336U::read_touch_threshold(void) {
    return readByte(FT6336U_ADDR_THRESHOLD);
}
uint8_t FT6336U::read_filter_coefficient(void) {
    return readByte(FT6336U_ADDR_FILTER_COE);
}
uint8_t FT6336U::read_ctrl_mode(void) {
    return readByte(FT6336U_ADDR_CTRL);
}
void FT6336U::write_ctrl_mode(CTRL_MODE_Enum mode) {
    writeByte(FT6336U_ADDR_CTRL, mode);
}
uint8_t FT6336U::read_time_period_enter_monitor(void) {
    return readByte(FT6336U_ADDR_TIME_ENTER_MONITOR);
}
uint8_t FT6336U::read_active_rate(void) {
    return readByte(FT6336U_ADDR_ACTIVE_MODE_RATE);
}
uint8_t FT6336U::read_monitor_rate(void) {
    return readByte(FT6336U_ADDR_MONITOR_MODE_RATE);
}

// Gesture Parameters
uint8_t FT6336U::read_radian_value(void) {
	return readByte(FT6336U_ADDR_RADIAN_VALUE);
}
void FT6336U::write_radian_value(uint8_t val) {
	writeByte(FT6336U_ADDR_RADIAN_VALUE, val); 
}
uint8_t FT6336U::read_offset_left_right(void) {
	return readByte(FT6336U_ADDR_OFFSET_LEFT_RIGHT);
}
void FT6336U::write_offset_left_right(uint8_t val) {
	writeByte(FT6336U_ADDR_OFFSET_LEFT_RIGHT, val); 
}
uint8_t FT6336U::read_offset_up_down(void) {
	return readByte(FT6336U_ADDR_OFFSET_UP_DOWN);
}
void FT6336U::write_offset_up_down(uint8_t val) {
	writeByte(FT6336U_ADDR_OFFSET_UP_DOWN, val); 
}
uint8_t FT6336U::read_distance_left_right(void) {
	return readByte(FT6336U_ADDR_DISTANCE_LEFT_RIGHT);
}
void FT6336U::write_distance_left_right(uint8_t val) {
	writeByte(FT6336U_ADDR_DISTANCE_LEFT_RIGHT, val); 
}
uint8_t FT6336U::read_distance_up_down(void) {
	return readByte(FT6336U_ADDR_DISTANCE_UP_DOWN);
}
void FT6336U::write_distance_up_down(uint8_t val) {
	writeByte(FT6336U_ADDR_DISTANCE_UP_DOWN, val); 
}
uint8_t FT6336U::read_distance_zoom(void) {
	return readByte(FT6336U_ADDR_DISTANCE_ZOOM);
}
void FT6336U::write_distance_zoom(uint8_t val) {
	writeByte(FT6336U_ADDR_DISTANCE_ZOOM, val); 
}


// System Information
uint16_t FT6336U::read_library_version(void) {
    uint8_t read_buf[2];
    read_buf[0] = readByte(FT6336U_ADDR_LIBRARY_VERSION_H);
    read_buf[1] = readByte(FT6336U_ADDR_LIBRARY_VERSION_L);
	return ((read_buf[0] & 0x0f) << 8) | read_buf[1];
}
uint8_t FT6336U::read_chip_id(void) {
    return readByte(FT6336U_ADDR_CHIP_ID);
}
uint8_t FT6336U::read_g_mode(void) {
    return readByte(FT6336U_ADDR_G_MODE);
}
void FT6336U::write_g_mode(G_MODE_Enum mode){
	writeByte(FT6336U_ADDR_G_MODE, mode); 
}
uint8_t FT6336U::read_pwrmode(void) {
    return readByte(FT6336U_ADDR_POWER_MODE);
}
uint8_t FT6336U::read_firmware_id(void) {
    return readByte(FT6336U_ADDR_FIRMARE_ID);
}
uint8_t FT6336U::read_focaltech_id(void) {
    return readByte(FT6336U_ADDR_FOCALTECH_ID);
}
uint8_t FT6336U::read_release_code_id(void) {
    return readByte(FT6336U_ADDR_RELEASE_CODE_ID);
}
uint8_t FT6336U::read_state(void) {
    return readByte(FT6336U_ADDR_STATE);
}


//coordinate diagram（FPC downwards）
////y ////////////////////264x176
						//
						//
						//x
						//
						//
FT6336U_TouchPointType FT6336U::scan(void){
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
uint8_t FT6336U::readByte(uint8_t addr) {
    uint8_t rdData = 0; 
    uint8_t rdDataCount; 
    do {
        Wire.beginTransmission(I2C_ADDR_FT6336U); 
        Wire.write(addr); 
        Wire.endTransmission(false); // Restart
        delay(10); 
        rdDataCount = Wire.requestFrom(I2C_ADDR_FT6336U, 1); 
    } while(rdDataCount == 0); 
    while(Wire.available()) {
        rdData = Wire.read(); 
    }
    return rdData; 

}
void FT6336U::writeByte(uint8_t addr, uint8_t data) {
    DEBUG_PRINTLN("")
    DEBUG_PRINT("writeI2C reg 0x")
    DEBUG_PRINT(addr, HEX)
    DEBUG_PRINT(" -> 0x") DEBUG_PRINTLN(data, HEX)
	
	Wire.beginTransmission(I2C_ADDR_FT6336U); 
    Wire.write(addr); 
    Wire.write(data); 
    Wire.endTransmission(); 
}