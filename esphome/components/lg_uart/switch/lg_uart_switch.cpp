#include "lg_uart_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lg_uart {

using namespace esphome::switch_;

std::string LGUartSwitch::describe() { return this->get_name(); }

void LGUartSwitch::dump_config() {
  ESP_LOGCONFIG(TAG, "[%s] command string: '%s'", this->get_name().c_str(), this->cmd_str_);
}

void LGUartSwitch::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LGUartSwitch '%s' with cmd_string: [%s]...", this->name_.c_str(), this->cmd_str_);

  bool initial_state = this->get_initial_state_with_restore_mode().value_or(false);

  ESP_LOGD(TAG, "[%s] setup(): [%i].", this->get_name().c_str(), initial_state);

  // TODO: consider omitting this? maybe we want to be "unknown" until the next update() when we poll the screen?
  // write state before setup
  if (initial_state) {
    this->turn_on();
  } else {
    this->turn_off();
  }
}

// When parent gets a reply meant for us, we'll get notified here
void LGUartSwitch::on_reply_packet(std::vector<uint8_t> *pkt) {
  ESP_LOGD(TAG, "[%s] on_reply_packet(). reply: [%s].", this->get_name().c_str(),
           this->parent_->get_reply_as_str(pkt).c_str());

  std::string status_str = this->parent_->get_status_str(pkt->at(7), pkt->at(8));
  ESP_LOGD(TAG, "[%s] on_reply_packet(): status_str: [%s]", this->get_name().c_str(), status_str.c_str());

  if (status_str[0] == 0) {
    ESP_LOGE(TAG, "[%s] on_reply_packet(): Didn't get status_str: [%s]", this->get_name().c_str(), status_str.c_str());
    this->status_set_error();
  }
  // TODO: make safe? Possible to get back some chars that do not convert to int?
  // ESPHome does not use exceptions?
  int status_int = stoi(status_str, nullptr, this->encoding_base_);

  ESP_LOGD(TAG, "[%s] update(): Inverted: [%i], state: [%i], status_int: [%i]", this->get_name().c_str(),
           this->inverted_, this->state, (bool) status_int);

  if (this->status_has_error())
    this->status_clear_error();

  this->publish_state((bool) status_int);
}

void LGUartSwitch::update() {
  ESP_LOGD(TAG, "[%s] update(). command: [%s].", this->get_name().c_str(), this->cmd_str_);
  this->parent_->send_cmd(this->cmd_str_, 0xff, true);
}

void LGUartSwitch::set_cmd(const std::string &cmd_str) {
  if (cmd_str.length() != 2) {
    ESP_LOGE(TAG, "Invalid cmd_str.");
    return;
  }
  this->cmd_str_[0] = cmd_str[0];
  this->cmd_str_[1] = cmd_str[1];
}

void LGUartSwitch::write_state(bool state) {
  ESP_LOGD(TAG, "[%s] write_state(): %i", this->get_name().c_str(), state);

  if (this->parent_->send_cmd(this->cmd_str_, (int) state, true)) {
    ESP_LOGD(TAG, "[%s] write_state(): %i - OK!", this->get_name().c_str(), state);
  } else {
    ESP_LOGW(TAG, "[%s] write_state(): %i - NG!", this->get_name().c_str(), state);
    // NG means that we can't confirm the state has changed, bail before publishing an updated state
    this->status_set_warning();
    return;
  }

  // If the reply was OK, we can be very confident that the state the user wants is now in effect.
  this->publish_state(state);
}

}  // namespace lg_uart
}  // namespace esphome
