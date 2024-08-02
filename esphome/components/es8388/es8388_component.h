#pragma once

#include <map>

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace es8388 {

enum ES8388Preset : uint8_t { NONE = 0x00, RASPIAUDIO_MUSE_LUXE = 0x01, RASPIAUDIO_RADIO = 0x02 };
typedef std::vector<std::array<uint8_t, 2>> Instructions;
struct Macro {
  std::string name;
  Instructions instructions;
};
typedef std::map<std::string, Macro> Macros;

class ES8388Component : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::LATE - 1; }

  void set_preset(ES8388Preset preset) { this->preset_ = preset; }
  void set_init_instructions(Instructions instructions) { this->init_instructions_ = instructions; }
  void register_macro(std::string name, Instructions instructions);
  void execute_macro(std::string name);

 protected:
  void setup_raspiaudio_radio();
  void setup_raspiaudio_muse_luxe();
  ES8388Preset preset_;
  Instructions init_instructions_;
  Macros macros_;
};

template<typename... Ts> class ES8388MacroAction : public Action<Ts...>, public Parented<ES8388Component> {
 public:
  TEMPLATABLE_VALUE(std::string, macro_id)

  void play(Ts... x) {
    if (this->macro_id_.has_value()) {
      std::string macro_id = this->macro_id_.value(x...);
      this->parent_->execute_macro(macro_id);
    }
  }
};

}  // namespace es8388
}  // namespace esphome
