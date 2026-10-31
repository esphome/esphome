#pragma once

#include "esphome/components/ld600x/ld600x.h"

namespace esphome::ld6004 {

enum LD6004ButtonType : uint8_t {
  BUTTON_CLEAR_DWELL = ld600x::LD600X_MODEL_KIND_BASE,
};

class LD6004Component final : public ld600x::LD600XComponent {
 public:
  LD6004Component() : LD600XComponent(LOG_STR("ld6004")) {}

#ifdef USE_NUMBER
  SUB_NUMBER(dwell_lifetime)
  SUB_NUMBER(output_interval)
#endif
#ifdef USE_SELECT
  SUB_SELECT(work_mode)
  SUB_SELECT(p20_mode)
#endif

#ifdef USE_NUMBER
  void set_number_value(number::Number *entity, float value) override;
#endif
#ifdef USE_SELECT
  void set_select_value(select::Select *entity, size_t index) override;
#endif
#ifdef USE_BUTTON
  void press_button(uint8_t kind) override;
#endif

 protected:
  bool handle_model_report(uint16_t type, const uint8_t *data, uint16_t len) override;
  void setup_model() override;
  void dump_model_config() override;
};

}  // namespace esphome::ld6004
