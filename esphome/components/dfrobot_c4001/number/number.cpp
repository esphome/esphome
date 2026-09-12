#include "number.h"

namespace esphome {
namespace dfrobot_c4001 {
void MaxRangeNumber::control(float value) { this->parent_->set_max_range(value); }
void MinRangeNumber::control(float value) { this->parent_->set_min_range(value); };
void TriggerRangeNumber::control(float value) { this->parent_->set_trigger_range(value); };
void HoldSensitivityNumber::control(float value) { this->parent_->set_hold_sensitivity(value); };
void TriggerSensitivityNumber::control(float value) { this->parent_->set_trigger_sensitivity(value); };
void OnLatencyNumber::control(float value) { this->parent_->set_on_latency(value); }
void OffLatencyNumber::control(float value) { this->parent_->set_off_latency(value); }
void InhibitTimeNumber::control(float value) { this->parent_->set_inhibit_time(value); }
void ThresholdFactorNumber::control(float value) { this->parent_->set_threshold_factor(value); }
}  // namespace dfrobot_c4001
}  // namespace esphome
