#include "pd_pioneer_ir.h"
#include "esphome/core/log.h"

namespace esphome::pd_pioneer_ir {

ESPHOME_LOG_TAG(TAG, "pd_pioneer_ir");

void PDPioneerIR::setup() {
  climate_ir::ClimateIR::setup();
  this->publish_state();
}

void PDPioneerIR::transmit_state() {
  ControlData data;
  data.set_mode(this->mode);
  if (this->mode != climate::CLIMATE_MODE_OFF && this->mode != climate::CLIMATE_MODE_FAN_ONLY)
    data.set_temp(this->target_temperature);
  data.set_fan_mode(this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO));
  data.set_swing_mode(this->swing_mode);
  // ECO shares the power byte, so it must not undo an off command
  data.set_eco(this->mode != climate::CLIMATE_MODE_OFF && this->preset == climate::CLIMATE_PRESET_ECO);
  data.finalize();
  // The remote sends the two bursts as separate transmissions
  this->transmit_<remote_base::PDPioneerProtocol>(data.odd());
  this->transmit_<remote_base::PDPioneerProtocol>(data.even());
}

bool PDPioneerIR::on_receive(remote_base::RemoteReceiveData data) {
  auto frame = remote_base::PDPioneerProtocol().decode(data);
  if (!frame.has_value())
    return false;
  if (frame->is_odd_burst()) {
    this->rx_horizontal_swing_ = ControlData::get_horizontal_swing(*frame);
    return false;
  }
  // decode() only returns odd or even bursts; a mode byte this remote never sends is another device's frame
  const optional<climate::ClimateMode> decoded = ControlData::get_mode(*frame);
  if (!decoded.has_value())
    return false;
  // An auto-mode frame from the remote must not publish HEAT_COOL when the traits leave it out
  if (this->get_traits().supports_mode(*decoded)) {
    this->mode = *decoded;
  } else {
    ESP_LOGD(TAG, "Received mode %s is not enabled, keeping the current mode",
             LOG_STR_ARG(climate::climate_mode_to_string(*decoded)));
  }
  // Off and fan-only bursts carry no target temperature
  if (*decoded != climate::CLIMATE_MODE_OFF && *decoded != climate::CLIMATE_MODE_FAN_ONLY)
    this->target_temperature = ControlData::get_temp(*frame);
  this->fan_mode = ControlData::get_fan_mode(*frame);
  this->swing_mode = ControlData::get_swing_mode(*frame, this->rx_horizontal_swing_);
  // A missed odd burst must not carry this command's horizontal swing into the next one
  this->rx_horizontal_swing_ = false;
  this->preset = ControlData::get_eco(*frame) ? climate::CLIMATE_PRESET_ECO : climate::CLIMATE_PRESET_NONE;
  this->publish_state();
  return true;
}

}  // namespace esphome::pd_pioneer_ir
