#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "esphome/core/string_ref.h"

namespace esphome::homeassistant {

/** Mirrors a Home Assistant `select` or `input_select` entity.
 *
 * The options are read from the entity's `options` attribute at runtime. They are stored in a buffer of
 * `options_buffer_size` bytes and a list of at most `max_options` entries, both reserved in setup(), so a
 * change of options never allocates.
 */
class HomeassistantSelect final : public select::Select, public Component {
 public:
  HomeassistantSelect(uint8_t max_options, uint16_t options_buffer_size)
      : options_buffer_size_(options_buffer_size), max_options_(max_options) {}

  void set_entity_id(const char *entity_id) { this->entity_id_ = entity_id; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  void options_changed_(StringRef options);
  void state_changed_(StringRef state);

  void control(size_t index) override;

  const char *entity_id_{nullptr};

 private:
  // The option pointers in traits point into this buffer; its size must match options_buffer_size_
  std::unique_ptr<char[]> options_buffer_;
  uint16_t options_buffer_size_;
  uint8_t max_options_;
};

}  // namespace esphome::homeassistant
