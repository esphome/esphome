#pragma once
#include <cstdint>
#include <cstring>
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome::sbtest {

class SBTest : public Component {
 public:
  SBTest(uint8_t *a, size_t na, uint32_t *b, size_t nb, const char **c, size_t nc)
      : a_(a), na_(na), b_(b), nb_(nb), c_(c), nc_(nc) {}
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override {
    static uint8_t seed = 0x11;
    bool zero = true;
    for (size_t i = 0; i < this->na_; i++)
      zero &= this->a_[i] == 0;
    for (size_t i = 0; i < this->nb_; i++)
      zero &= this->b_[i] == 0;
    for (size_t i = 0; i < this->nc_; i++)
      zero &= this->c_[i] == nullptr;
    const bool aligned =
        ((uintptr_t) this->b_ % alignof(uint32_t)) == 0 && ((uintptr_t) this->c_ % alignof(const char *)) == 0;
    // fill each buffer completely with its own pattern, then check the others were not touched
    memset(this->a_, seed, this->na_);
    for (size_t i = 0; i < this->nb_; i++)
      this->b_[i] = 0xA5000000u | seed;
    for (size_t i = 0; i < this->nc_; i++)
      this->c_[i] = "x";
    this->seed_ = seed++;
    this->setup_ok_ = zero && aligned;
    ESP_LOGI("sbtest", "SBTEST setup zero=%d aligned=%d a=%p/%u b=%p/%u c=%p/%u %s", zero, aligned, this->a_,
             (unsigned) this->na_, this->b_, (unsigned) this->nb_, this->c_, (unsigned) this->nc_,
             this->setup_ok_ ? "PASS" : "FAIL");
  }
  void dump_config() override {
    bool ok = true;
    for (size_t i = 0; i < this->na_; i++)
      ok &= this->a_[i] == this->seed_;
    for (size_t i = 0; i < this->nb_; i++)
      ok &= this->b_[i] == (0xA5000000u | this->seed_);
    for (size_t i = 0; i < this->nc_; i++)
      ok &= this->c_[i] != nullptr && this->c_[i][0] == 'x';
    ESP_LOGI("sbtest", "SBTEST intact=%d setup_ok=%d seed=0x%02X %s", ok, this->setup_ok_, this->seed_,
             ok && this->setup_ok_ ? "PASS" : "FAIL");
  }

 protected:
  uint8_t *a_;
  size_t na_;
  uint32_t *b_;
  size_t nb_;
  const char **c_;
  size_t nc_;
  uint8_t seed_{0};
  bool setup_ok_{false};
};

}  // namespace esphome::sbtest
