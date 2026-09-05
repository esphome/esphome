#include "esphome/core/log.h"
#include "vornado_ir.h"
#include "vornado_ir_codes.h"
#include "esphome/core/application.h"

namespace esphome::vornado_ir {

static const char *const TAG = "vornado_ir";

void VornadoIR::dump_config() { ESP_LOGCONFIG(TAG, "Vornado IR"); }

void VornadoIR::send_power_toggle() {
  ESP_LOGI(TAG, "Sending power toggle request");
  this->transmit_(VORNADO_IR_POWER_TOGGLE_TIMINGS);
}

void VornadoIR::send_change_direction() {
  ESP_LOGI(TAG, "Sending change direction request");
  this->transmit_(VORNADO_IR_CHANGE_DIRECTION_TIMINGS);
}

void VornadoIR::send_increase() {
  ESP_LOGI(TAG, "Sending increase request");
  this->transmit_(VORNADO_IR_INCREASE_TIMINGS);
}

void VornadoIR::send_decrease() {
  ESP_LOGI(TAG, "Sending decrease request");
  this->transmit_(VORNADO_IR_DECREASE_TIMINGS);
}

void VornadoIR::transmit_(const RawTimings &ir_code) {
  ESP_LOGD(TAG, "Sending ir_code");
  auto transmit = this->transmitter_->transmit();
  ESP_LOGD(TAG, "Sending ir_code got transmitter");
  auto *data = transmit.get_data();
  data->set_data(ir_code);
  data->set_carrier_frequency(38000);
  transmit.set_send_times(3);
  transmit.set_send_wait(7);
  ESP_LOGD(TAG, "Sending ir_code actual perform transmit");
  transmit.perform();
}

}  // namespace esphome::vornado_ir
