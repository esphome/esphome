#pragma once
// MCP23017 IOCON and INT line checks on an ESP8266: INTB on GPIO13, INTA on GPIO14, PA0/PA1 jumpered to PB0/PB1.
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <Arduino.h>

namespace esphome::mcptest {

static const char *const TAG = "mcptest";
static volatile uint32_t isr13_count = 0;
static volatile uint32_t isr14_count = 0;
static void IRAM_ATTR isr13() { isr13_count++; }
static void IRAM_ATTR isr14() { isr14_count++; }
inline bool pin_level(uint8_t pin) { return digitalRead(pin); }  // NOLINT

class McpTest : public Component, public i2c::I2CDevice {
 public:
  void set_pins(GPIOPin *a0, GPIOPin *a1) {
    this->pa_[0] = a0;
    this->pa_[1] = a1;
  }
  void set_bs(binary_sensor::BinarySensor *b0, binary_sensor::BinarySensor *b1) {
    this->pb_[0] = b0;
    this->pb_[1] = b1;
  }
  void set_opts(bool isr_inta, const char *label) {
    this->isr_inta_ = isr_inta;
    this->label_ = label;
  }
  float get_setup_priority() const override { return setup_priority::LATE; }

  void setup() override {
    pinMode(13, INPUT);  // NOLINT
    attachInterrupt(digitalPinToInterrupt(13), isr13, CHANGE);
    if (this->isr_inta_) {
      pinMode(14, INPUT);  // NOLINT
      attachInterrupt(digitalPinToInterrupt(14), isr14, CHANGE);
    }
    this->pa_[0]->setup();
    this->pa_[1]->setup();
    this->pa_[0]->digital_write(false);
    this->pa_[1]->digital_write(false);
  }

  uint8_t rd(uint8_t reg) {
    uint8_t v = 0xEE;
    if (!this->read_byte(reg, &v)) {
      ESP_LOGE(TAG, "read 0x%02X failed", reg);
    }
    return v;
  }

  // A failed write reads back unchanged and would look like no aliasing; run.sh fails on the [E] line.
  void wr(uint8_t reg, uint8_t v) {
    if (!this->write_byte(reg, v)) {
      ESP_LOGE(TAG, "write 0x%02X failed", reg);
    }
  }

  void loop() override {
    uint32_t now = millis();
    if (this->phase_ == 0) {
      if (now < 6000)
        return;
      this->iocon_a_ = this->rd(0x0A);
      this->iocon_b_ = this->rd(0x0B);
      ESP_LOGI(TAG, "[%s] IOCON 0x0A=0x%02X 0x0B=0x%02X GPINTENB=0x%02X IODIRA=0x%02X IODIRB=0x%02X INT13=%d INT14=%d",
               this->label_, this->iocon_a_, this->iocon_b_, this->rd(0x05), this->rd(0x00), this->rd(0x01),
               pin_level(13), pin_level(14));
      this->isr13_start_ = isr13_count;
      this->isr14_start_ = isr14_count;
      this->phase_ = 1;
      this->next_ = now + 200;
      return;
    }
    if (this->phase_ == 1) {
      if (now < this->next_)
        return;
      if (this->step_ > 0 && this->pending_check_) {
        // 200 ms after the toggle: INT released? loopback seen?
        int p = (this->step_ - 1) / 10;
        bool want = this->level_[p];
        if (!pin_level(13))
          this->intb_low_late_++;
        if (!pin_level(14))
          this->inta_low_late_++;
        if (this->pb_[p]->has_state() && this->pb_[p]->state == want)
          this->loop_ok_++;
        this->pending_check_ = false;
      }
      if (this->step_ >= 20) {
        this->phase_ = 2;
        return;
      }
      int p = this->step_ / 10;
      this->level_[p] = !this->level_[p];
      this->pa_[p]->digital_write(this->level_[p]);
      if (!pin_level(13))
        this->intb_low_now_++;
      if (!pin_level(14))
        this->inta_low_now_++;
      this->step_++;
      this->pending_check_ = true;
      this->next_ = now + 200;
      return;
    }
    if (this->phase_ == 2) {
      ESP_LOGI(
          TAG,
          "[%s] SUMMARY iocon_a=0x%02X iocon_b=0x%02X toggles=20 inta_low_now=%u intb_low_now=%u inta_low_200ms=%u "
          "intb_low_200ms=%u loopback_ok=%u/20 isr_intb=%u isr_inta=%s%u bs_changes=%u heap_free=%u",
          this->label_, this->iocon_a_, this->iocon_b_, this->inta_low_now_, this->intb_low_now_, this->inta_low_late_,
          this->intb_low_late_, this->loop_ok_, isr13_count - this->isr13_start_, this->isr_inta_ ? "" : "n/a ",
          isr14_count - this->isr14_start_, this->changes_, ESP.getFreeHeap());
      this->ioconb_test_();
      this->phase_ = 3;
    }
  }

  void ioconb_test_() {
    uint8_t a0 = this->rd(0x0A), b0 = this->rd(0x0B);
    const uint8_t bit = 0x08;  // HAEN, ignored by the I2C part
    this->wr(0x0B, b0 ^ bit);
    uint8_t a1 = this->rd(0x0A), b1 = this->rd(0x0B);
    this->wr(0x0B, b0);
    uint8_t a2 = this->rd(0x0A), b2 = this->rd(0x0B);
    this->wr(0x0A, a0 ^ bit);
    uint8_t a3 = this->rd(0x0A), b3 = this->rd(0x0B);
    this->wr(0x0A, a0);
    uint8_t a4 = this->rd(0x0A), b4 = this->rd(0x0B);
    ESP_LOGI(TAG,
             "[%s] IOCONB_TEST start A=0x%02X B=0x%02X | write B^0x08 -> A=0x%02X B=0x%02X | restore -> A=0x%02X "
             "B=0x%02X | write A^0x08 -> A=0x%02X B=0x%02X | restore -> A=0x%02X B=0x%02X",
             this->label_, a0, b0, a1, b1, a2, b2, a3, b3, a4, b4);
    // Second probe on a bit that matters (MIRROR) to rule out HAEN being read-only
    this->wr(0x0B, b0 ^ 0x40);
    uint8_t a5 = this->rd(0x0A), b5 = this->rd(0x0B);
    this->wr(0x0B, b0);
    uint8_t a6 = this->rd(0x0A), b6 = this->rd(0x0B);
    ESP_LOGI(TAG, "[%s] IOCONB_TEST2 write B^0x40 -> A=0x%02X B=0x%02X | restore -> A=0x%02X B=0x%02X", this->label_,
             a5, b5, a6, b6);
  }

  void count_change() { this->changes_++; }

 protected:
  GPIOPin *pa_[2];
  binary_sensor::BinarySensor *pb_[2];
  bool isr_inta_{false};
  const char *label_{""};
  int phase_{0};
  uint32_t next_{0};
  int step_{0};
  bool pending_check_{false};
  bool level_[2]{false, false};
  uint8_t iocon_a_{0}, iocon_b_{0};
  uint32_t isr13_start_{0}, isr14_start_{0};
  uint32_t intb_low_now_{0}, inta_low_now_{0}, intb_low_late_{0}, inta_low_late_{0}, loop_ok_{0}, changes_{0};
};

}  // namespace esphome::mcptest
