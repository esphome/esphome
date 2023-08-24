#pragma once

#ifdef USE_ESP_IDF

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace esp_adf {
class ESPADF;

class ESPADFPipeline : public Parented<ESPADF> {};

class ESPADF : public Component {
 public:
  void setup() override;

  float get_setup_priority() const override;

  void lock() { this->lock_.lock(); }
  bool try_lock() { return this->lock_.try_lock(); }
  void unlock() { this->lock_.unlock(); }

 protected:
  Mutex lock_;
};

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF
