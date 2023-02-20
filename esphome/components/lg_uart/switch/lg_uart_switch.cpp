#include "lg_uart_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lg_uart {

using namespace esphome::switch_;

void LGUartSwitch::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LGUartSwitch '%s' with cmd_string: [%s]...", this->name_.c_str(), this->cmd_str_);

  bool initial_state = this->get_initial_state_with_restore_mode().value_or(false);

  ESP_LOGD(TAG, "[%s] setup(): [%i].", this->get_name().c_str(), initial_state);

  // write state before setup
  if (initial_state) {
    this->turn_on();
  } else {
    this->turn_off();
  }
}

void LGUartSwitch::dump_config() {
  ESP_LOGCONFIG(TAG, "[%s] command string: '%s'", this->get_name().c_str(), this->cmd_str_);
}

void LGUartSwitch::update() {
  ESP_LOGD(TAG, "[%s] update(). command: [%s] returning %i", this->get_name().c_str(), this->cmd_str_, this->state);

  // For most commands, supplying `ff` as the value is the inquire? packet
  if (!this->parent_->send_cmd(this->cmd_str_, 0xff, this->reply)) {
    ESP_LOGD(TAG, "[%s] update(). got NG.", this->get_name().c_str());
    return;
  }
  ESP_LOGD(TAG, "[%s] update(). reply: [%s]", this->get_name().c_str(), this->reply);

  // Immediately after the OK will be a number which is the state we inquired about.
  // Chars -> string -> int. And in the case of a switch int -> bool.

  std::string status_str;
  status_str.push_back(this->reply[7]);
  status_str.push_back(this->reply[8]);
  ESP_LOGD(TAG, "[%s] update(): status_str: [%s]", this->get_name().c_str(), status_str.c_str());
  int status_int = stoi(status_str);

  ESP_LOGD(TAG, "[%s] update(): Inverted: [%i], state: [%i], status_int: [%i]", this->get_name().c_str(),
           this->inverted_, this->state, (bool) status_int);

  // If the user has indicated that
  this->publish_state((bool) status_int);
}

void LGUartSwitch::write_state(bool state) {
  ESP_LOGD(TAG, "[%s] write_state(): %i", this->get_name().c_str(), state);

  if (this->parent_->send_cmd(this->cmd_str_, (int) state, this->reply)) {
    ESP_LOGD(TAG, "[%s] write_state(): %i - OK!", this->get_name().c_str(), state);
  } else {
    ESP_LOGD(TAG, "[%s] write_state(): %i - NG!", this->get_name().c_str(), state);
    // NG means that we can't confirm the state has changed, bail before publishing an updated state
    return;
  }

  // If the reply was OK, we can be very confident that the state the user wants is now in effect.
  this->publish_state(state);
}

}  // namespace lg_uart
}  // namespace esphome
