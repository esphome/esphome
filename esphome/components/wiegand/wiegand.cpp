#include "wiegand.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace wiegand {

static const char *const TAG = "wiegand";
static const char *const KEYS = "0123456789*#";

void IRAM_ATTR HOT WiegandStore::d0_gpio_intr(WiegandStore *arg) {
  if (arg->d0.digital_read())
    return;
  arg->count++;
  arg->value <<= 1;
  arg->last_bit_time = millis();
  arg->done = false;
}

void IRAM_ATTR HOT WiegandStore::d1_gpio_intr(WiegandStore *arg) {
  if (arg->d1.digital_read())
    return;
  arg->count++;
  arg->value = (arg->value << 1) | 1;
  arg->last_bit_time = millis();
  arg->done = false;
}

void Wiegand::setup() {
  this->d0_pin_->setup();
  this->store_.d0 = this->d0_pin_->to_isr();
  this->d1_pin_->setup();
  this->store_.d1 = this->d1_pin_->to_isr();
  this->d0_pin_->attach_interrupt(WiegandStore::d0_gpio_intr, &this->store_, gpio::INTERRUPT_FALLING_EDGE);
  this->d1_pin_->attach_interrupt(WiegandStore::d1_gpio_intr, &this->store_, gpio::INTERRUPT_FALLING_EDGE);
}

bool check_eparity(uint64_t value, int start, int length) {
  int parity = 0;
  uint64_t mask = 1LL << start;
  for (int i = 0; i < length; i++, mask <<= 1) {
    if (value & mask)
      parity++;
  }
  return !(parity & 1);
}

bool check_oparity(uint64_t value, int start, int length) {
  int parity = 0;
  uint64_t mask = 1LL << start;
  for (int i = 0; i < length; i++, mask <<= 1) {
    if (value & mask)
      parity++;
  }
  return parity & 1;
}

void Wiegand::loop() {
  if (this->store_.done)
    return;
  if (millis() - this->store_.last_bit_time < 100)
    return;
  uint8_t count = this->store_.count;
  uint64_t value = this->store_.value;
  this->store_.count = 0;
  this->store_.value = 0;
  this->store_.done = true;
  ESP_LOGV(TAG, "received %d-bit value: %llx", count, value);
  for (auto *trigger : this->raw_triggers_)
    trigger->trigger(count, value);
  if (count == 26) {
    std::string tag = to_string((value >> 1) & 0xffffff);
    ESP_LOGD(TAG, "received 26-bit tag: %s", tag.c_str());
    if (!check_eparity(value, 13, 13) || !check_oparity(value, 0, 13)) {
      ESP_LOGW(TAG, "invalid parity");
      return;
    }
    for (auto *trigger : this->tag_triggers_)
      trigger->trigger(tag);
  } else if (count == 34) {
    std::string tag = to_string((value >> 1) & 0xffffffff);
    ESP_LOGD(TAG, "received 34-bit tag: %s", tag.c_str());
    if (!check_eparity(value, 17, 17) || !check_oparity(value, 0, 17)) {
      ESP_LOGW(TAG, "invalid parity");
      return;
    }
    for (auto *trigger : this->tag_triggers_)
      trigger->trigger(tag);
  } else if (count == 37) {
    std::string tag = to_string((value >> 1) & 0x7ffffffff);
    ESP_LOGD(TAG, "received 37-bit tag: %s", tag.c_str());
    if (!check_eparity(value, 18, 19) || !check_oparity(value, 0, 19)) {
      ESP_LOGW(TAG, "invalid parity");
      return;
    }
    for (auto *trigger : this->tag_triggers_)
      trigger->trigger(tag);
  } else if (count == 4) {
    for (auto *trigger : this->key_triggers_)
      trigger->trigger(value);
    if (value < 12) {
      uint8_t key = KEYS[value];
      this->send_key_(key);
    }
  } else if (count == 8) {
    if ((value ^ 0xf0) >> 4 == (value & 0xf)) {
      value &= 0xf;
      for (auto *trigger : this->key_triggers_)
        trigger->trigger(value);
      if (value < 12) {
        uint8_t key = KEYS[value];
        this->send_key_(key);
      }
    }
  } else {
    ESP_LOGD(TAG, "received unknown %d-bit value: %llx", count, value);
  }
}

void Wiegand::dump_config() {
  ESP_LOGCONFIG(TAG, "Wiegand reader:");
  LOG_PIN("  D0 pin: ", this->d0_pin_);
  LOG_PIN("  D1 pin: ", this->d1_pin_);
}

}  // namespace wiegand
}  // namespace esphome
