#include "M5Stack_4_Relays.h"
#include "esphome/core/log.h"

namespace esphome {
namespace M5Stack_4_Relays {

static const char *const TAG = "M5Stack_4_Relays";

void M5Stack_4_Relays::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Stack_4_Relays:");
  LOG_I2C_DEVICE(this);
  //if (this->is_failed()) {
  //  ESP_LOGE(TAG, "Communication with M5Stack_4_Relays failed!");
  //}
  /* LOG_SWITCH("  ", "Relays", this);*/
}

/*! @brief Setting the mode of the device, and turn off all relays.
 *  @param mode Async = 0, Sync = 1. */
void M5Stack_4_Relays::Init(bool mode) {
  write1Byte(UNIT_4RELAY_REG, mode);
  write1Byte(UNIT_4RELAY_RELAY_REG, 0);
}
//
///*! @brief Set the mode of all leds at the same time.
// *  @param state OFF = 0, ON = 1. */
//void M5Stack_4_Relays::ledAll(bool state) { write1Byte(UNIT_4RELAY_RELAY_REG, state * (0xf0)); }
//
///*! @brief Control the on/off of the specified led.
// *  @param number Bit number of led (0~3).
//    @param state OFF = 0, ON = 1 . */
//void M5Stack_4_Relays::ledWrite(uint8_t number, bool state) {
//  uint8_t StateFromDevice = read1Byte(UNIT_4RELAY_RELAY_REG);
//  if (state == 0) {
//    StateFromDevice &= ~(UNIT_4RELAY_REG << number);
//  } else {
//    StateFromDevice |= (UNIT_4RELAY_REG << number);
//  }
//  write1Byte(UNIT_4RELAY_RELAY_REG, StateFromDevice);
//}

/*! @brief Read a certain length of data to the specified register address. */
uint8_t M5Stack_4_Relays::read1Byte(uint8_t Register_address) {
  uint8_t data;
  if (!this->read_byte(Register_address, &data)) {
    this->mark_failed();
    return uint8_t(0);
  }
  return data;
}
//
///*! @brief Set the mode of all relays at the same time.
// *  @param state OFF = 0, ON = 1. */
//void M5Stack_4_Relays::relayAll(bool state) { write1Byte(UNIT_4RELAY_RELAY_REG, state * (0x0f)); }

/*! @brief Control the on/off of the specified relay.
 *  @param number Bit number of relay (0~3).
    @param state OFF = 0, ON = 1 . */
void M5Stack_4_Relays::relayWrite(uint8_t number, bool state) {
  uint8_t StateFromDevice = read1Byte(UNIT_4RELAY_RELAY_REG);
  if (state == 0) {
    StateFromDevice &= ~(0x01 << number);
  } else {
    StateFromDevice |= (0x01 << number);
  }
  write1Byte(UNIT_4RELAY_RELAY_REG, StateFromDevice);
}

void M5Stack_4_Relays::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Stack_4_Relays...");
  uint8_t setupmode = 1;
  this->Init(setupmode);
}

/*! @brief Setting the mode of the device.
 *  @param mode Async = 0, Sync = 1. */
void M5Stack_4_Relays::set_switchMode(bool mode) { write1Byte(UNIT_4RELAY_REG, mode); }

/*! @brief Write a certain length of data to the specified register address. */
void M5Stack_4_Relays::write1Byte(uint8_t Register_address, uint8_t data) {
  if (!this->write_byte(Register_address, data)) {
    this->mark_failed();
    return;
  }
}

}  // namespace M5Stack_4_Relays
}  // namespace esphome
