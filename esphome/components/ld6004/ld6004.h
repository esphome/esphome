#pragma once

#include "esphome/components/ld600x/ld600x.h"

namespace esphome::ld6004 {

enum LD6004NumberType : uint8_t {
  NUMBER_DWELL_LIFETIME = ld600x::LD600X_MODEL_KIND_BASE,
  NUMBER_OUTPUT_INTERVAL,
};

enum LD6004SelectType : uint8_t {
  SELECT_WORK_MODE = ld600x::LD600X_MODEL_KIND_BASE,
  SELECT_P20_MODE,
};

enum LD6004ButtonType : uint8_t {
  BUTTON_CLEAR_DWELL = ld600x::LD600X_MODEL_KIND_BASE,
};

class LD6004Component final : public ld600x::LD600XComponent {
 public:
  LD6004Component() : LD600XComponent("LD6004", "ld6004") {}

#ifdef USE_NUMBER
  void set_dwell_lifetime_number(number::Number *number) { this->dwell_lifetime_number_ = number; }
  void set_output_interval_number(number::Number *number) { this->output_interval_number_ = number; }
#endif
#ifdef USE_SELECT
  void set_work_mode_select(select::Select *select) { this->work_mode_select_ = select; }
  void set_p20_mode_select(select::Select *select) { this->p20_mode_select_ = select; }
#endif

#ifdef USE_NUMBER
  void set_number_value(uint8_t kind, float value) override;
#endif
#ifdef USE_SELECT
  void set_select_value(uint8_t kind, size_t index) override;
#endif
#ifdef USE_BUTTON
  void press_button(uint8_t kind) override;
#endif

 protected:
  bool handle_model_report(uint16_t type, const uint8_t *data, uint16_t len) override;
  // The 0x0A12 report carries all five modes, so the text sensor never needs the base's
  // presence-derived two-state fallback nor the target stream that feeds it.
  bool work_mode_uses_fallback() const override { return false; }
  void setup_model() override;
  void dump_model_config() override;

#ifdef USE_NUMBER
  number::Number *dwell_lifetime_number_{nullptr};
  number::Number *output_interval_number_{nullptr};
#endif
#ifdef USE_SELECT
  select::Select *work_mode_select_{nullptr};
  select::Select *p20_mode_select_{nullptr};
#endif
};

}  // namespace esphome::ld6004
