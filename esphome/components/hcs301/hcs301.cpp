#include "esphome/core/log.h"
#include "hcs301.h"

namespace esphome {
namespace hcs301 {

static const char *TAG = "hcs301";

// This is how many words the EEPROM has (12x 16 bit addresses)
#define EEPROM_SIZE 12

void HCS301::setup() {
  if (this->power_pin_ != nullptr) {
    this->power_pin_->setup();
    this->power_pin_->digital_write(false);
  }
  if (this->clock_pin_ != nullptr) {
    this->clock_pin_->setup();
    this->clock_pin_->digital_write(false);
  }
  if (this->pwm_pin_ != nullptr) {
    this->pwm_pin_->setup();
    this->pwm_pin_->digital_write(false);
  }
}

void HCS301::loop() {}

void HCS301::dump_config(){
    ESP_LOGCONFIG(TAG, "HCS301");
    LOG_PIN("  Power Pin: ", this->power_pin_);
    LOG_PIN("  Clock Pin: ", this->clock_pin_);
    LOG_PIN("  PWM Pin: ", this->pwm_pin_);
}

bool HCS301::program(uint32_t serial, uint16_t sequence, uint64_t key) {
    uint16_t eeprom_buffer[EEPROM_SIZE]; // Buffer for EEPROM Data
    uint16_t vbuffer[EEPROM_SIZE]; // Data for verification received from device
    
    uint32_t seed = 0x0; // we don't handle "Secure Learn" yet

    HCSConfig config;
    config.voltage_low = (hcs301_voltage_ == HCS301_Voltage::Vcc_9_12);

    // prepare the EEPROM data
    config_eeprom_buffer_(eeprom_buffer, key, sequence, serial, seed, config);
    // write to the HCS301
    write_eeprom_buffer(eeprom_buffer, vbuffer);
    // check the EEPROM readback
    uint8_t success = 0xFF;
    for (int i = 0; i < EEPROM_SIZE; i++){
        ESP_LOGD(TAG, "Verify word %d, expect %04X, got %04X", i, eeprom_buffer[i], vbuffer[i]);
        if (eeprom_buffer[i] != vbuffer[i]) {
            success = i;
            break;
        }
    }
    if (success != 0xFF) {
        ESP_LOGD(TAG, "Failed to program HCS301, EEPROM verification failed at word %d", success);
        return false;
    }
    return true;
}

// FIXME: This should really be exposed as a Switch
void HCS301::transmitS2() {
    ESP_LOGD(TAG, "Triggering Transmit");
    pwm_pin_->pin_mode(INPUT);
    power_pin_->digital_write(true);
    clock_pin_->digital_write(true);
    delay(15);
    clock_pin_->digital_write(false);
//    power_pin_->digital_write(false);
    ESP_LOGD(TAG, "Done");
}

void HCS301::config_eeprom_buffer_(uint16_t *eeprom_buffer, uint64_t key, uint16_t sync, uint32_t serial, uint32_t seed, struct HCSConfig hcsconfig) {
  // Configure the secret key:
  // Split the key up into 4 separate words inside the eeprom buffer
  // This is accomplished by bitmasking the 64 bit key provided as an argument
  eeprom_buffer[0] = (key & 0x000000000000ffff);          // KEY_0
  eeprom_buffer[1] = (key & 0x00000000ffff0000) >> 16;    // KEY_1
  eeprom_buffer[2] = (key & 0x0000ffff00000000) >> 32;    // KEY_2
  eeprom_buffer[3] = (key & 0xffff000000000000) >> 48;    // KEY_3
  eeprom_buffer[4] = sync;                                // SYNC
  eeprom_buffer[5] = 0x0000;                              // RESERVED

  // In the HCS301 serial numbers only use the first 28 bits. If anything is set with those first 4 bits, its an invalid serial number
  // That being said, the bits are just ignored...
  if (serial & 0xF0000000) {
    ESP_LOGD(TAG, "Invalid serial number, bits 29+ set: %08X", serial);
  }

  // Take the serial number and split into 2 words by bitmask and shift.
  eeprom_buffer[6] = (serial & 0x0000ffff);               // SERIAL_0
  eeprom_buffer[7] = (serial & 0x0fff0000) >> 16;         // SERIAL_1 / Auto Shutoff (MSb)

  // If AUTO_SHUTOFF is true, set the most significant bit (31) of the serial number.
  // We can simply add 0x8000 to the since it will be set to zero because of the bitmask above
  if (hcsconfig.auto_shutoff == 1) {
    eeprom_buffer[7] |= 0x8000;
  }

  // Split the seed into 2 words
  eeprom_buffer[8] = (seed & 0x0000ffff);                 // SEED_0
  eeprom_buffer[9] = (seed & 0xffff0000) >> 16;           // SEED_1
  eeprom_buffer[10] = 0x0000;                             // RESERVED
  
  // Setup the config.
  uint16_t config = 0;
  config |= hcsconfig.overflow_0 << 10;
  config |= hcsconfig.overflow_1 << 11;
  config |= hcsconfig.voltage_low << 12;
  config |= hcsconfig.bsl0 << 13;
  config |= hcsconfig.bsl1 << 14;

  // Take the first 10 bit of the serial number to set the discrimination bits per the datasheet:
  config |= (serial & 0x03ff);

  eeprom_buffer[11] = config;    // CONFIG
}

void HCS301::write_eeprom_buffer(uint16_t *eeprom_buffer, uint16_t *vbuffer) {
  // Set PWM to output mode
  this->pwm_pin_->pin_mode(OUTPUT);
  // Default the pwm to low
  this->pwm_pin_->digital_write(false);
  // Enable programming mode for the HCS301
  // S2[CLK_PIN] High for 3.5MS
  this->clock_pin_->digital_write(true);
  this->power_pin_->digital_write(true);
  // TPS for 3.5ms-4.5ms
  delayMicroseconds(3500);
  this->pwm_pin_->digital_write(true);
  // TPH1 for minimum of 3.5ms
  delayMicroseconds(3500);
  this->pwm_pin_->digital_write(false);
  // TPH2 for 50us
  delayMicroseconds(50);
  this->clock_pin_->digital_write(false);
  // TPBW for 4.0ms
  delay(4);
  for (int line_number = 0; line_number < EEPROM_SIZE; line_number++) {
    this->pwm_pin_->pin_mode(OUTPUT);
    for (int i = 0; i <= 15; i++) {
      bool val;
      // Shift the buffer line_number i bits to the right, then bitmask with 0x01 to get a 0 or 1 (LOW/HIGH) and output directly to the PWM pin.
      val = ((eeprom_buffer[line_number] >> i) & 0x01) == 0x01;
      this->pwm_pin_->digital_write(val);
      this->clock_pin_->digital_write(true);
      delayMicroseconds(35);
      this->clock_pin_->digital_write(false);
      delayMicroseconds(35);
    }
    this->clock_pin_->digital_write(false);
    this->pwm_pin_->digital_write(false);
    // Set PWM to input mode so we can get the ack 
    // FIXME: No check for ack?
    this->pwm_pin_->pin_mode(INPUT);
    // TWC for 50ms
    delay(50);
  }
  // Verify Cycle.
  this->clock_pin_->digital_write(false);
  this->pwm_pin_->digital_write(false);
  for (int line_number = 0; line_number < EEPROM_SIZE; line_number++) {
    uint16_t val = 0x0000;
    for (int i = 0; i < 16 ; i++) {
      this->clock_pin_->digital_write(true);
      delayMicroseconds(35);
      // Read from LSB first...
      val |= this->pwm_pin_->digital_read() << i;
      this->clock_pin_->digital_write(false);
      delayMicroseconds(35);
    }
    // Write the entire word to the vbuffer
    vbuffer[line_number] = val;
  }
  // Turn off the HCS301
  delay(200);
  this->power_pin_->digital_write(false);
}

}  // namespace empty_component
}  // namespace esphome