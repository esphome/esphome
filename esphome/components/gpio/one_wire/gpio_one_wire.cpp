#include "gpio_one_wire.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::gpio {

static const char *const TAG = "gpio.one_wire";

void GPIOOneWireBus::setup() {
#ifdef USE_ONE_WIRE_RMT
  if (this->use_rmt_) {
    this->setup_rmt_();
    return;
  }
#endif

  this->t_pin_->setup();
  this->t_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  // Clear bus with 480us high, otherwise initial reset in search might fail.
  this->pin_.digital_write(true);
  this->pin_.pin_mode(gpio::FLAG_OUTPUT);
  delayMicroseconds(480);
  this->search();
}

void GPIOOneWireBus::dump_config() {
#ifdef USE_ONE_WIRE_RMT
  if (this->use_rmt_) {
    ESP_LOGCONFIG(TAG, "GPIO 1-wire bus (RMT):");
  } else
#endif
  {
    ESP_LOGCONFIG(TAG, "GPIO 1-wire bus:");
  }
  LOG_PIN("  Pin: ", this->t_pin_);
  this->dump_devices_(TAG);
}

int HOT IRAM_ATTR GPIOOneWireBus::reset_int() {
#ifdef USE_ONE_WIRE_RMT
  if (this->use_rmt_)
    return this->reset_rmt_();
#endif

  InterruptLock lock;
  // See reset here:
  // https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html
  // Wait for communication to clear (delay G).
  this->pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  uint8_t retries = 125;
  do {
    if (--retries == 0)
      return -1;
    delayMicroseconds(2);
  } while (!this->pin_.digital_read());

  bool r = false;

  // Send 480us LOW TX reset pulse (drive bus low, delay H).
  this->pin_.digital_write(false);
  this->pin_.pin_mode(gpio::FLAG_OUTPUT);
  delayMicroseconds(480);

  // Release the bus, delay I.
  this->pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  uint32_t start = micros();
  delayMicroseconds(30);

  while (micros() - start < 300) {
    // Sample bus, 0=device(s) present, 1=no device present.
    r = !this->pin_.digital_read();
    if (r)
      break;
    delayMicroseconds(1);
  }

  // Delay J: finish the 480us slot, but never spin if it already elapsed.
  uint32_t elapsed = micros() - start;
  if (elapsed < 480)
    delayMicroseconds(480 - elapsed);
  this->pin_.digital_write(true);
  this->pin_.pin_mode(gpio::FLAG_OUTPUT);
  return r ? 1 : 0;
}

void HOT IRAM_ATTR GPIOOneWireBus::write_bit_(bool bit) {
#ifdef USE_ONE_WIRE_RMT
  if (this->use_rmt_) {
    this->write_bit_rmt_(bit);
    return;
  }
#endif

  // Drive bus low.
  this->pin_.digital_write(false);

  // From datasheet:
  // write 0 low time: t_low0: min=60us, max=120us
  // write 1 low time: t_low1: min=1us, max=15us
  // time slot: t_slot: min=60us, max=120us
  // recovery time: t_rec: min=1us
  // DS18B20 appears to read the bus after roughly 14us.
  uint32_t delay0 = bit ? 6 : 60;
  uint32_t delay1 = bit ? 64 : 10;

  delayMicroseconds(delay0);
  this->pin_.digital_write(true);
  delayMicroseconds(delay1);
}

bool HOT IRAM_ATTR GPIOOneWireBus::read_bit_() {
#ifdef USE_ONE_WIRE_RMT
  if (this->use_rmt_) {
    bool bit;
    return this->read_bit_rmt_(&bit) && bit;
  }
#endif

  this->pin_.digital_write(false);
  delayMicroseconds(5);

  this->pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  delayMicroseconds(8);
  bool r = this->pin_.digital_read();
  delayMicroseconds(50);

  this->pin_.digital_write(true);
  this->pin_.pin_mode(gpio::FLAG_OUTPUT);
  return r;
}

void IRAM_ATTR GPIOOneWireBus::write8(uint8_t val) {
#ifdef USE_ONE_WIRE_RMT
  if (this->use_rmt_) {
    this->write8_rmt_(val);
    return;
  }
#endif

  InterruptLock lock;
  for (uint8_t i = 0; i < 8; i++)
    this->write_bit_(bool((1u << i) & val));
}

void IRAM_ATTR GPIOOneWireBus::write64(uint64_t val) {
#ifdef USE_ONE_WIRE_RMT
  if (this->use_rmt_) {
    this->write64_rmt_(val);
    return;
  }
#endif

  InterruptLock lock;
  for (uint8_t i = 0; i < 64; i++)
    this->write_bit_(bool((1ULL << i) & val));
}

uint8_t IRAM_ATTR GPIOOneWireBus::read8() {
#ifdef USE_ONE_WIRE_RMT
  if (this->use_rmt_)
    return this->read8_rmt_();
#endif

  InterruptLock lock;
  uint8_t ret = 0;
  for (uint8_t i = 0; i < 8; i++)
    ret |= (uint8_t(this->read_bit_()) << i);
  return ret;
}

uint64_t IRAM_ATTR GPIOOneWireBus::read64() {
#ifdef USE_ONE_WIRE_RMT
  if (this->use_rmt_)
    return this->read64_rmt_();
#endif

  InterruptLock lock;
  uint64_t ret = 0;
  for (uint8_t i = 0; i < 64; i++)
    ret |= (uint64_t(this->read_bit_()) << i);
  return ret;
}

void GPIOOneWireBus::reset_search() {
  this->last_discrepancy_ = 0;
  this->last_device_flag_ = false;
  this->address_ = 0;
}

uint64_t IRAM_ATTR GPIOOneWireBus::search_int() {
#ifdef USE_ONE_WIRE_RMT
  if (this->use_rmt_)
    return this->search_rmt_();
#endif

  InterruptLock lock;
  if (this->last_device_flag_)
    return 0u;

  uint8_t last_zero = 0;
  uint64_t bit_mask = 1;
  uint64_t address = this->address_;

  for (int bit_number = 1; bit_number <= 64; bit_number++, bit_mask <<= 1) {
    bool id_bit = this->read_bit_();
    bool cmp_id_bit = this->read_bit_();

    if (id_bit && cmp_id_bit)
      return 0;

    bool branch;
    if (id_bit != cmp_id_bit) {
      branch = id_bit;
    } else {
      if (bit_number < this->last_discrepancy_) {
        branch = (address & bit_mask) > 0;
      } else {
        branch = bit_number == this->last_discrepancy_;
      }

      if (!branch)
        last_zero = bit_number;
    }

    if (branch) {
      address |= bit_mask;
    } else {
      address &= ~bit_mask;
    }

    this->write_bit_(branch);
  }

  this->last_discrepancy_ = last_zero;
  if (this->last_discrepancy_ == 0)
    this->last_device_flag_ = true;

  this->address_ = address;
  return address;
}

}  // namespace esphome::gpio