#include "zigbee_cover_esp32.h"
#include "esphome/core/defines.h"
#if defined(USE_ESP32) && defined(USE_ZIGBEE_COVER)

#include <cmath>

namespace esphome::zigbee {

void ZigbeeCover::setup() {
  this->supports_position_ = this->cover_->get_traits().get_supports_position();
  this->value_ = this->lift_percentage_();
  this->add_attr<uint8_t>(0, this->value_, &this->value_);
  uint8_t config_status =
      EZB_ZCL_WINDOW_COVERING_CONFIG_STATUS_OPERATIONAL | EZB_ZCL_WINDOW_COVERING_CONFIG_STATUS_ONLINE;
  if (this->supports_position_) {
    config_status |= EZB_ZCL_WINDOW_COVERING_CONFIG_STATUS_LIFT_CLOSED_LOOP;
  }
  this->zb_->add_attr(this->endpoint_id_, this->cluster_id_, this->role_, EZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_STATUS_ID,
                      0, EZB_ZCL_ATTR_TYPE_MAP8, 0, config_status);
  // Covers without position support still publish 0 or 1, which reports closed or open
  this->cover_->add_on_state_callback([this]() { this->set_attr(this->lift_percentage_()); });
}

uint8_t ZigbeeCover::lift_percentage_() const {
  // ZCL lift percentage: 0 is open, 100 is closed
  return static_cast<uint8_t>(lroundf((1.0f - this->cover_->position) * 100.0f));
}

void ZigbeeCover::restore_lift_percentage() {
  // value_ is a single byte the main loop updates on every cover state change
  ezb_zcl_set_attr_value(this->endpoint_id_, this->cluster_id_, this->role_, this->attr_id_, EZB_ZCL_STD_MANUF_CODE,
                         &this->value_, false);
}

void ZigbeeCover::on_command(uint8_t cmd_id, uint8_t percentage) {
  auto call = this->cover_->make_call();
  switch (cmd_id) {
    case EZB_ZCL_CMD_WINDOW_COVERING_UP_OPEN_ID:
      call.set_command_open();
      break;
    case EZB_ZCL_CMD_WINDOW_COVERING_DOWN_CLOSE_ID:
      call.set_command_close();
      break;
    case EZB_ZCL_CMD_WINDOW_COVERING_STOP_ID:
      call.set_command_stop();
      break;
    case EZB_ZCL_CMD_WINDOW_COVERING_GO_TO_LIFT_PERCENTAGE_ID:
      call.set_position(1.0f - static_cast<float>(percentage) / 100.0f);
      break;
    default:
      return;
  }
  call.perform();
}

}  // namespace esphome::zigbee

#endif
