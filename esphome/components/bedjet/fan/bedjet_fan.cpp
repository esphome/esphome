#include "bedjet_fan.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace bedjet {

using namespace esphome::fan;

void BedJetFan::dump_config() { LOG_FAN("", "BedJet Fan", this); }
std::string BedJetFan::describe() { return "BedJet Fan"; }

void BedJetFan::control(const fan::FanCall &call) {
  ESP_LOGD(TAG, "Received BedJetFan::control");
  if (!this->parent_->is_connected()) {
    ESP_LOGW(TAG, "Not connected, cannot handle control call yet.");
    return;
  }
  bool did_change = false;

  if (call.get_state().has_value() && this->state != *call.get_state()) {
    // Turning off is easy:
    if (this->state && this->parent_->button_off()) {
      this->state = false;
      this->publish_state();
      return;
    }

    // Turning on, we have to choose a specific mode; for now, use "COOL" mode
    // In the future we could configure the mode to use for fan.turn_on.
    if (this->parent_->button_cool()) {
      this->state = true;
      did_change = true;
    }
  }

  // ignore speed changes if not on or turning on
  if (this->state && call.get_speed().has_value()) {
    auto speed = *call.get_speed();
    if (speed >= 1) {
      this->speed = speed;
      // Fan.speed is 1-20, but Bedjet expects 0-19, so subtract 1
      this->parent_->set_fan_index(this->speed - 1);
      did_change = true;
    }
  }

  if (did_change) {
    this->publish_state();
  }
}

void BedJetFan::on_status(const BedjetStatusPacket *data) {
  ESP_LOGVV(TAG, "[%s] Handling on_status with data=%p", this->get_name().c_str(), (void *) data);
  bool did_change = false;
  bool new_state = data->mode != MODE_STANDBY && data->mode != MODE_WAIT;

  if (new_state != this->state) {
    this->state = new_state;
    did_change = true;
  }

  // BedjetStatusPacket.fan_step is in range 0-19, but Fan.speed wants 1-20.
  if (data->fan_step + 1 != this->speed) {
    this->speed = data->fan_step + 1;
    did_change = true;
  }

  if (did_change) {
    this->publish_state();
  }
}

/** Attempts to update the fan device from the last received BedjetStatusPacket.
 *
 * This will be called from #on_status() when the parent dispatches new status packets,
 * and from #update() when the polling interval is triggered.
 *
 * @return `true` if the status has been applied; `false` if there is nothing to apply.
 */
bool BedJetFan::update_status_() {
  if (!this->parent_->is_connected())
    return false;
  if (!this->parent_->has_status())
    return false;

  auto *status = this->parent_->get_status_packet();

  if (status == nullptr)
    return false;

  this->on_status(status);
  return true;
}

void BedJetFan::update() {
  ESP_LOGD(TAG, "[%s] update()", this->get_name().c_str());
  // TODO: if the hub component is already polling, do we also need to include polling?
  //  We're already going to get on_status() at the hub's polling interval.
  auto result = this->update_status_();
  ESP_LOGD(TAG, "[%s] update_status result=%s", this->get_name().c_str(), result ? "true" : "false");
}

/** Resets states to defaults. */
void BedJetFan::reset_state_() {
  this->state = false;
  this->publish_state();
}
}  // namespace bedjet
}  // namespace esphome

#endif
