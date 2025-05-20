#include "gpio_one_wire.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace gpio {

static const char *const TAG = "gpio.one_wire";

void GPIOOneWireBus::setup() {
  ESP_LOGCONFIG(TAG, "Setting up 1-wire bus...");
  this->t_pin_->setup();
  this->t_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  // clear bus with 480µs high, otherwise initial reset in search might fail
  this->pin_.digital_write(true);
  this->pin_.pin_mode(gpio::FLAG_OUTPUT);
  delayMicroseconds(480);
  this->search();
}

void GPIOOneWireBus::dump_config() {
  ESP_LOGCONFIG(TAG, "GPIO 1-wire bus:");
  LOG_PIN("  Pin: ", this->t_pin_);
  this->dump_devices_(TAG);
}

int HOT IRAM_ATTR GPIOOneWireBus::reset_int() {
  InterruptLock lock;
  // See reset here:
  // https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html
  // Wait for communication to clear (delay G)
  this->pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  uint8_t retries = 125;
  do {
    if (--retries == 0)
      return -1;
    delayMicroseconds(2);
  } while (!this->pin_.digital_read());

  bool r = false;

  // Send 480µs LOW TX reset pulse (drive bus low, delay H)
  this->pin_.digital_write(false);
  this->pin_.pin_mode(gpio::FLAG_OUTPUT);
  delayMicroseconds(480);

  // Release the bus, delay I
  this->pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  uint32_t start = micros();
  delayMicroseconds(30);

  while (micros() - start < 300) {
    // sample bus, 0=device(s) present, 1=no device present
    r = !this->pin_.digital_read();
    if (r)
      break;
    delayMicroseconds(1);
  }

  // delay J
  delayMicroseconds(start + 480 - micros());
  this->pin_.digital_write(true);
  this->pin_.pin_mode(gpio::FLAG_OUTPUT);
  return r ? 1 : 0;
}

void HOT IRAM_ATTR GPIOOneWireBus::write_bit_(bool bit) {
  // drive bus low
  this->pin_.digital_write(false);

  // from datasheet:
  // write 0 low time: t_low0: min=60µs, max=120µs
  // write 1 low time: t_low1: min=1µs, max=15µs
  // time slot: t_slot: min=60µs, max=120µs
  // recovery time: t_rec: min=1µs
  // ds18b20 appears to read the bus after roughly 14µs
  uint32_t delay0 = bit ? 6 : 60;
  uint32_t delay1 = bit ? 64 : 10;

  // delay A/C
  delayMicroseconds(delay0);
  // release bus
  this->pin_.digital_write(true);
  // delay B/D
  delayMicroseconds(delay1);
}

bool HOT IRAM_ATTR GPIOOneWireBus::read_bit_() {
  // drive bus low
  this->pin_.digital_write(false);

  // datasheet says >= 1µs
  delayMicroseconds(5);

  // release bus, delay E
  this->pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

  delayMicroseconds(8);
  // sample bus to read bit from peer
  bool r = this->pin_.digital_read();

  // read slot is at least 60µs
  delayMicroseconds(50);

  this->pin_.digital_write(true);
  this->pin_.pin_mode(gpio::FLAG_OUTPUT);
  return r;
}

void IRAM_ATTR GPIOOneWireBus::write8(uint8_t val) {
  InterruptLock lock;
  for (uint8_t i = 0; i < 8; i++) {
    this->write_bit_(bool((1u << i) & val));
  }
}

void IRAM_ATTR GPIOOneWireBus::write64(uint64_t val) {
  InterruptLock lock;
  for (uint8_t i = 0; i < 64; i++) {
    this->write_bit_(bool((1ULL << i) & val));
  }
}

uint8_t IRAM_ATTR GPIOOneWireBus::read8() {
  InterruptLock lock;
  uint8_t ret = 0;
  for (uint8_t i = 0; i < 8; i++)
    ret |= (uint8_t(this->read_bit_()) << i);
  return ret;
}

uint64_t IRAM_ATTR GPIOOneWireBus::read64() {
  InterruptLock lock;
  uint64_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint64_t(this->read_bit_()) << i);
  }
  return ret;
}

void GPIOOneWireBus::reset_search() {
  this->last_discrepancy_ = 0;
  this->last_device_flag_ = false;
  this->address_ = 0;
}

uint64_t IRAM_ATTR GPIOOneWireBus::search_int() {
  InterruptLock lock;
  if (this->last_device_flag_)
    return 0u;

  uint8_t last_zero = 0;
  uint64_t bit_mask = 1;
  uint64_t address = this->address_;

  // Initiate search
  for (int bit_number = 1; bit_number <= 64; bit_number++, bit_mask <<= 1) {
    // read bit
    bool id_bit = this->read_bit_();
    // read its complement
    bool cmp_id_bit = this->read_bit_();

    if (id_bit && cmp_id_bit) {
      // No devices participating in search
      return 0;
    }

    bool branch;

    if (id_bit != cmp_id_bit) {
      // only chose one branch, the other one doesn't have any devices.
      branch = id_bit;
    } else {
      // there are devices with both 0s and 1s at this bit
      if (bit_number < this->last_discrepancy_) {
        branch = (address & bit_mask) > 0;
      } else {
        branch = bit_number == this->last_discrepancy_;
      }

      if (!branch) {
        last_zero = bit_number;
      }
    }

    if (branch) {
      address |= bit_mask;
    } else {
      address &= ~bit_mask;
    }

    // choose/announce branch
    this->write_bit_(branch);
  }

  this->last_discrepancy_ = last_zero;
  if (this->last_discrepancy_ == 0) {
    // we're at root and have no choices left, so this was the last one.
    this->last_device_flag_ = true;
  }

  this->address_ = address;
  return address;
}

}  // namespace gpio
}  // namespace esphome
