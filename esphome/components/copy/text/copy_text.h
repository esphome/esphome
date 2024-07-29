#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text/text.h"

namespace esphome {
namespace copy {

class CopyText : public text::Text, public Component {
 public:
  void set_source(text::Text *source) { source_ = source; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(const std::string &value) override;

  text::Text *source_;
};

}  // namespace copy
}  // namespace esphome
