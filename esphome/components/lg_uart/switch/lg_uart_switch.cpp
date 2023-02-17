#include "lg_uart_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lg_uart {

using namespace esphome::switch_;

void LGUartSwitch::dump_config() {
  ESP_LOGCONFIG(TAG, "[%s] command string: '%s'", this->get_name().c_str(), this->cmd_str_);
}

void LGUartSwitch::update() { ESP_LOGD(TAG, "[%s] update(). returning %i", this->get_name().c_str(), this->state); }

void LGUartSwitch::write_state(bool state) {
  ESP_LOGD(TAG, "[%s] write_state(): %i", this->get_name().c_str(), state);
  this->parent_->send_cmd(this->cmd_str_, (int) state);
  /*
    FOR NOW we just assume that the command went through.
    Will need to uart_read looking for OK to really confirm.

    Will ALSO need to implement polling where the command FF is set and reply examined for NG or proper value
  */
  // TODO: mute seems to be inverted; will need some sort of look up. if cmd string is `ke` then return inverted state?

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
