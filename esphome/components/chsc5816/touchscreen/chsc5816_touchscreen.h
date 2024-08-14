#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace chsc5816 {

typedef struct __CHSC5816_Header {
  uint16_t fw_ver;
  uint16_t checksum;
  uint32_t sig;
  uint32_t vid_pid;
  uint16_t raw_offet;
  uint16_t dif_offet;
} CHSC5816_Header_t;

/*
uint16_t fw_ver;    00 00
uint16_t checksum;  00 00
uint32_t sig;       00 00 00 00
uint32_t vid_pid;   00 00 00 00
uint16_t raw_offet; 00 00
uint16_t dif_offet; 00 00
*/

union __CHSC5816_PointReg {
  struct {
    uint8_t status;
    uint8_t fingerNumber;
    uint8_t x_l8;
    uint8_t y_l8;
    uint8_t z;
    uint8_t x_h4 : 4;
    uint8_t y_h4 : 4;
    uint8_t id : 4;
    uint8_t event : 4;
    uint8_t p2;
  } rp;
  unsigned char data[8];
};

static const char *const TAG = "chsc5816.touchscreen";

static const uint8_t REG_STATUS = 0x00;
static const uint8_t REG_TOUCH_NUM = 0x02;
static const uint8_t REG_XPOS_HIGH = 0x03;
static const uint8_t REG_XPOS_LOW = 0x04;
static const uint8_t REG_YPOS_HIGH = 0x05;
static const uint8_t REG_YPOS_LOW = 0x06;
static const uint8_t REG_DIS_AUTOSLEEP = 0xFE;
static const uint8_t REG_CHIP_ID = 0xA7;
static const uint8_t REG_FW_VERSION = 0xA9;
static const uint8_t REG_SLEEP = 0xE5;
static const uint8_t REG_IRQ_CTL = 0xFA;
static const uint8_t IRQ_EN_MOTION = 0x70;

static const uint8_t CST826_CHIP_ID = 0x11;
static const uint8_t CST820_CHIP_ID = 0xB7;
static const uint8_t CHSC5816S_CHIP_ID = 0xB4;
static const uint8_t CHSC5816D_CHIP_ID = 0xB6;
static const uint8_t CHSC5816T_CHIP_ID = 0xB5;
static const uint8_t CST716_CHIP_ID = 0x20;

class CHSC5816ButtonListener {
 public:
  virtual void update_button(bool state) = 0;
};

class CHSC5816Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void update_touches() override;
  void register_button_listener(CHSC5816ButtonListener *listener) { this->button_listeners_.push_back(listener); }
  void dump_config() override;

  // int readRegister(int reg, uint8_t *buf, uint8_t length, bool stop);
  // int writeRegister(int reg, uint8_t *buf, uint8_t length, bool stop);
  int readRegister(uint32_t reg, uint8_t *buf, uint8_t length, bool stop);
  int writeRegister(uint32_t reg, uint8_t *buf, uint8_t length, bool stop);
  void reset();
  bool checkOnline();

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  uint8_t __reg_addr_len = 4;
  CHSC5816_Header_t __header;
  void continue_setup_();
  void update_button_state_(bool state);

  InternalGPIOPin *interrupt_pin_{};
  GPIOPin *reset_pin_{};
  uint8_t chip_id_{};
  std::vector<CHSC5816ButtonListener *> button_listeners_;
  bool button_touched_{};
};

}  // namespace chsc5816
}  // namespace esphome
