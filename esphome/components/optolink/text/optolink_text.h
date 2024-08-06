#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/text/text.h"
#include "../optolink.h"
#include "../datapoint_component.h"

namespace esphome {
namespace optolink {

enum TextType { TEXT_TYPE_DAY_SCHEDULE };

class OptolinkText : public DatapointComponent, public esphome::text::Text {
 public:
  OptolinkText(Optolink *optolink) : DatapointComponent(optolink) {}

  void set_type(TextType type) { type_ = type; }
  void set_day_of_week(int dow) { dow_ = dow; }

 protected:
  void setup() override;
  void update() override { datapoint_read_request_(); }
  void control(const std::string &value) override;

  const StringRef &get_component_name() override { return get_name(); }
  void datapoint_value_changed(uint8_t *value, size_t length) override;

 private:
  TextType type_;
  int dow_ = 0;
};

}  // namespace optolink
}  // namespace esphome

#endif
