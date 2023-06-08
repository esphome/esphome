#ifdef USE_ESP32
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include "esp32/rom/ets_sys.h"
#include <esp32-hal.h>

#include "easyscale.h"

#ifdef USE_POWER_SUPPLY
#include "esphome/components/power_supply/power_supply.h"
#endif

namespace esphome {
namespace easyscale {

static const char *const TAG = "EasyScale";

void EasyScale::setup_state(light::LightState *state) {
  // Disable Gamma correction, as the Easyscale device has that somewhat built in
  state->set_gamma_correct(0.00);
}

void EasyScale::send_zero_() {
  this->pin_->digital_write(LOW);
  ets_delay_us(80);
  this->pin_->digital_write(HIGH);
  ets_delay_us(20);
}

void EasyScale::send_one_() {
  this->pin_->digital_write(LOW);
  ets_delay_us(20);
  this->pin_->digital_write(HIGH);
  ets_delay_us(80);
}

void EasyScale::setup() {
  this->pin_->setup();
  ESP_LOGD(TAG, "Switching device to Easyscale mode");

  InterruptLock lock;
  // reset the chip, by holding the control line low.
  this->pin_->digital_write(LOW);
  delay(10);

  // Enable EasyScale mode
  this->pin_->digital_write(HIGH);
  ets_delay_us(200);
  this->pin_->digital_write(LOW);
  ets_delay_us(500);
  this->pin_->digital_write(HIGH);

  delay(1);
}

void EasyScale::write_state(light::LightState *state) {
  float bright;
  state->current_values_as_brightness(&bright);

#ifdef USE_POWER_SUPPLY
  if (bright > 0.0f) {
    this->power_.request();
  } else {
    this->power_.unrequest();
  }
#endif

  bright = clamp(bright, 0.0f, 1.0f);
  // NOLINTNEXTLINE(bugprone-incorrect-roundings, bugprone-integer-division)
  uint8_t out = (31 / 1 * bright) + 0.5;

  ESP_LOGD(TAG, "%.2f Easyscale step: %d", bright, out);

  // Send the device address byte
  for (int i = 7; i >= 0; i--) {
    if ((this->device_address_ >> i) & 1) {
      this->send_one_();
    } else {
      this->send_zero_();
    }
  }

  // End of Byte delay/spacing
  this->pin_->digital_write(LOW);
  ets_delay_us(10);

  // Start of Next Byte
  this->pin_->digital_write(HIGH);
  ets_delay_us(10);

  // No RFA
  this->send_zero_();
  // Address bit 1, always 0 with TPS61165
  this->send_zero_();
  // Address bit 0, always 0 with TPS61165
  this->send_zero_();

  // Send the 4 bits of brightness
  for (int i = 4; i >= 0; i--) {
    if ((out >> i) & 1) {
      this->send_one_();
    } else {
      this->send_zero_();
    }
  }
  this->pin_->digital_write(LOW);
  ets_delay_us(100);
  this->pin_->digital_write(HIGH);
  ets_delay_us(10);
}

}  // namespace easyscale
}  // namespace esphome
#endif
