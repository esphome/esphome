#pragma once
#include <cstdint>
#include <cstring>
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#ifdef USE_ESP32
#include <esp_memory_utils.h>
#endif

namespace esphome::sbpsram {

class SBPsram : public Component {
 public:
  SBPsram(uint8_t *maybe, size_t n, uint32_t *plain) : given_(maybe), n_(n), plain_(plain) {}
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override {
    // codegen already picked the storage: the static array, or psram_buffer<T>() (nullptr if it failed)
    this->buf_ = this->given_;
    size_t nonzero = 0;
    if (this->buf_ != nullptr)
      for (size_t i = 0; i < this->n_; i++)
        nonzero += this->buf_[i] != 0;
    for (size_t i = 0; this->plain_ != nullptr && i < PLAIN_N; i++)
      nonzero += this->plain_[i] != 0;
    this->zeroed_ = nonzero == 0;
    int ext = -1;
#ifdef USE_ESP32
    ext = this->buf_ ? esp_ptr_external_ram(this->buf_) : -1;
#endif
#ifdef USE_PSRAM
    const int psram = 1;
    this->placed_ = ext == 1;  // with PSRAM the buffer must not fall back to internal RAM
#else
    const int psram = 0;
#endif
    ESP_LOGI("sbpsram", "SBPSRAM use_psram=%d given=%p buf=%p n=%u external=%d nonzero_at_start=%u plain=%p", psram,
             this->given_, this->buf_, (unsigned) this->n_, ext, (unsigned) nonzero, this->plain_);
    // the plain buffer has no fallback, so it must exist and be aligned with or without PSRAM
    this->plain_ok_ = this->plain_ != nullptr && (uintptr_t) this->plain_ % alignof(uint32_t) == 0;
    for (size_t i = 0; this->plain_ok_ && i < PLAIN_N; i++)
      this->plain_[i] = 0xA5000000u | i;
    if (this->buf_ == nullptr)
      return;
    for (size_t i = 0; i < this->n_; i++)
      this->buf_[i] = (uint8_t) (i * 7 + 3);
  }
  void dump_config() override {
    bool ok = this->buf_ != nullptr;
    for (size_t i = 0; ok && i < this->n_; i++)
      ok = this->buf_[i] == (uint8_t) (i * 7 + 3);
    bool plain = this->plain_ok_;
    for (size_t i = 0; plain && i < PLAIN_N; i++)
      plain = this->plain_[i] == (0xA5000000u | i);
    bool pass = ok && plain && this->zeroed_ && this->placed_;
    ESP_LOGI("sbpsram", "SBPSRAM readback=%d plain=%d zeroed=%d placed=%d %s", ok, plain, this->zeroed_, this->placed_,
             pass ? "PASS" : "FAIL");
  }

 protected:
  static constexpr size_t PLAIN_N = 8;  // matches the uint32 static_buffer in __init__.py
  uint8_t *given_;
  size_t n_;
  uint32_t *plain_;
  uint8_t *buf_{nullptr};
  bool zeroed_{false};
  bool placed_{true};
  bool plain_ok_{false};
};

}  // namespace esphome::sbpsram
