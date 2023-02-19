#include "lg_uart_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lg_uart {

using namespace esphome::switch_;

void LGUartSwitch::dump_config() {
  ESP_LOGCONFIG(TAG, "[%s] command string: '%s'", this->get_name().c_str(), this->cmd_str_);
}

void LGUartSwitch::update() {
  ESP_LOGD(TAG, "[%s] update(). command: [%s] returning %i", this->get_name().c_str(), this->cmd_str_, this->state);
}

void LGUartSwitch::write_state(bool state) {
  ESP_LOGD(TAG, "[%s] write_state(): %i", this->get_name().c_str(), state);

  const char test[] = "ke";
  if (this->parent_->send_cmd(test, (int) state, this->reply)) {
    // if (this->parent_->send_cmd(this->cmd_str_, (int) state, this->reply)) {
    ESP_LOGD(TAG, "[%s] write_state(): %i - OK!", this->get_name().c_str(), state);
  } else {
    ESP_LOGD(TAG, "[%s] write_state(): %i - NG!", this->get_name().c_str(), state);
    // NG means that we can't confirm the state has changed, bail before publishing an updated state
    return;
  }

  // Convert the reply into status
  ESP_LOGD(TAG, "[%s] write_state(): REPLY: [%s] (%c, %c)", this->get_name().c_str(), this->reply, this->reply[7],
           this->reply[8]);

  std::string status_str;
  status_str.push_back(this->reply[7]);
  status_str.push_back(this->reply[8]);
  int status = stoi(status_str);
  ESP_LOGD(TAG, "[%s] write_state(): status_str: [%s], status: [%u]", this->get_name().c_str(), status_str.c_str(),
           status);

  if (this->inverted_) {
    this->publish_state(!status);
  } else {
    this->publish_state(status);
  }

  // if (state != this->inverted_) {
  //   // Turning ON, check interlocking

  //   bool found = false;
  //   for (auto *lock : this->interlock_) {
  //     if (lock == this)
  //       continue;

  //     if (lock->state) {
  //       lock->turn_off();
  //       found = true;
  //     }
  //   }
  //   if (found && this->interlock_wait_time_ != 0) {
  //     this->set_timeout("interlock", this->interlock_wait_time_, [this, state] {
  //       // Don't write directly, call the function again
  //       // (some other switch may have changed state while we were waiting)
  //       this->write_state(state);
  //     });
  //     return;
  //   }
  // } else if (this->interlock_wait_time_ != 0) {
  //   // If we are switched off during the interlock wait time, cancel any pending
  //   // re-activations
  //   this->cancel_timeout("interlock");
  // }

  // this->pin_->digital_write(state);
  // this->publish_state(state);
}

/* Public */

}  // namespace lg_uart
}  // namespace esphome
