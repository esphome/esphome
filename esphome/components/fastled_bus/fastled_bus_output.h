#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/output/float_output.h"

#define FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_ESP32_RAW_PIN_ORDER
#define FASTLED_RMT_BUILTIN_DRIVER true

// Avoid annoying compiler messages
#define FASTLED_INTERNAL

#include "fastled_bus.h"

namespace esphome {
namespace fastled_bus {

typedef struct sMapping {
  FastLEDBus *bus_;
  uint16_t num_chips_;
  uint16_t ofs_;
  uint8_t channel_offset_;
  uint16_t repeat_distance_;
} Mapping;

template<std::size_t S> class MappingsBuilder {
 private:
  Mapping mappings_[S];
  uint8_t index_{0};

 public:
  typedef Mapping Mappings[S];

  MappingsBuilder &add_mapping(FastLEDBus *const bus, const uint16_t num_chips, const uint16_t ofs,
                               const uint8_t channel_offset, const uint16_t repeat_distance) {
    auto *current = &(this->mappings_[this->index_++]);
    current->bus_ = bus;
    current->num_chips_ = num_chips;
    current->ofs_ = ofs;
    current->channel_offset_ = channel_offset;
    current->repeat_distance_ = repeat_distance;
    return *this;
  }
  MappingsBuilder<S>::Mappings &done() { return this->mappings_; }
};

template<std::size_t S> MappingsBuilder<S> &MappingsBuilderCreate() { return *(new MappingsBuilder<S>()); }

void setup_mapping(Mapping &map);
void write_mapping(Mapping &map, float state);

template<std::size_t S> class Output : public esphome::output::FloatOutput, public Component {
 protected:
  const Mapping (&mappings_)[S];

 public:
  // repeated_distance must be big to stop the set loop
  Output(const Mapping (&mappings)[S]) : mappings_{mappings} {}

  void setup() override {
    for (auto mapping : this->mappings_) {
      setup_mapping(mapping);
    }
  }

  void dump_config() override {}

  void write_state(float state) override {
    for (auto mapping : this->mappings_) {
      write_mapping(mapping, state);
    }
  }
};

}  // namespace fastled_bus
}  // namespace esphome

#endif  // USE_ARDUINO
