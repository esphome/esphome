#include "pd_pioneer_ir.h"
#include "esphome/core/log.h"

namespace esphome::pd_pioneer_ir {

static const char *const TAG = "pd_pioneer_ir.climate";

void PDPioneerIR::setup() {
  climate_ir::ClimateIR::setup();
  this->publish_state();
}

climate::ClimateTraits PDPioneerIR::traits() {
  auto traits = climate_ir::ClimateIR::traits();
  traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_HEAT,
                              climate::CLIMATE_MODE_DRY, climate::CLIMATE_MODE_FAN_ONLY});
  return traits;
}

void PDPioneerIR::control(const climate::ClimateCall &call) {
  if (call.get_mode() == climate::CLIMATE_MODE_OFF) {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
    this->preset = climate::CLIMATE_PRESET_NONE;
  } else if (call.get_preset().has_value()) {
    const auto preset = *call.get_preset();
    if ((preset == climate::CLIMATE_PRESET_NONE && this->preset == climate::CLIMATE_PRESET_ECO) ||
        (preset == climate::CLIMATE_PRESET_ECO && this->preset == climate::CLIMATE_PRESET_NONE)) {
      this->eco_ = true;
    }
  }
  climate_ir::ClimateIR::control(call);
}

void PDPioneerIR::transmit_frame_(const remote_base::PDPioneerData &frame) {
  auto transmit = this->transmitter_->transmit();
  remote_base::PDPioneerProtocol protocol;
  protocol.encode(transmit.get_data(), frame);
  transmit.perform();
}

void PDPioneerIR::transmit_pair_(const ControlData &data) {
  ControlData frame = data;
  frame.finalize();

  char odd_buf[remote_base::PDPioneerData::TO_STR_BUFFER_SIZE];
  char even_buf[remote_base::PDPioneerData::TO_STR_BUFFER_SIZE];
  ESP_LOGI(TAG, "TX odd:  %s", frame.odd().to_str(odd_buf));
  ESP_LOGI(TAG, "TX even: %s", frame.even().to_str(even_buf));

  // Send odd and even bursts separately, matching working Pronto button behavior.
  this->transmit_frame_(frame.odd());
  this->transmit_frame_(frame.even());
}

void PDPioneerIR::transmit_state() {
  ControlData data;
  data.set_mode(this->mode);

  if (this->eco_) {
    const bool enable = this->preset != climate::CLIMATE_PRESET_ECO;
    if (this->mode != climate::CLIMATE_MODE_OFF && this->mode != climate::CLIMATE_MODE_FAN_ONLY)
      data.set_temp(this->target_temperature);
    data.set_fan_mode(this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO));
    data.set_swing_mode(this->swing_mode);
    data.set_eco(enable);
    this->preset = enable ? climate::CLIMATE_PRESET_ECO : climate::CLIMATE_PRESET_NONE;
    this->eco_ = false;
    this->transmit_pair_(data);
    this->publish_state();
    return;
  }

  if (this->mode != climate::CLIMATE_MODE_FAN_ONLY)
    data.set_temp(this->target_temperature);
  // Fan first, then swing — vertical swing ORs into even byte 8 after fan encoding.
  data.set_fan_mode(this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO));
  data.set_swing_mode(this->swing_mode);
  data.set_eco(this->preset == climate::CLIMATE_PRESET_ECO);

  this->transmit_pair_(data);
}

bool PDPioneerIR::apply_frame_(const remote_base::PDPioneerData &frame) {
  if (frame.is_odd_burst()) {
    this->rx_state_.apply_odd(frame);
    this->rx_odd_pending_ = true;
    return false;
  }

  if (frame.is_even_burst()) {
    this->rx_state_.apply_even(frame);
    if (!this->rx_odd_pending_) {
      ESP_LOGD(TAG, "even burst without preceding odd burst");
    }
    this->rx_odd_pending_ = false;

    if (this->rx_state_.get_mode() != climate::CLIMATE_MODE_FAN_ONLY)
      this->target_temperature = this->rx_state_.get_temp();
    this->mode = this->rx_state_.get_mode();
    this->fan_mode = this->rx_state_.get_fan_mode();
    this->swing_mode = this->rx_state_.get_swing_mode();
    this->preset = this->rx_state_.get_eco() ? climate::CLIMATE_PRESET_ECO : climate::CLIMATE_PRESET_NONE;
    this->publish_state();
    return true;
  }

  return false;
}

bool PDPioneerIR::on_receive(remote_base::RemoteReceiveData data) {
  auto decoded = remote_base::PDPioneerProtocol().decode(data);
  if (!decoded.has_value())
    return false;
  return this->apply_frame_(*decoded);
}

}  // namespace esphome::pd_pioneer_ir
