#include "lg_uart_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lg_uart {

using namespace esphome::number;

void LGUartNumber::setup() {
  ESP_LOGCONFIG(TAG, "Setting up LGUartNumber '%s' with cmd_string: [%s]...", this->name_.c_str(), this->cmd_str_);
}

std::string LGUartNumber::describe() { return this->get_name(); }

// void LGUartNumber::parse_and_publish(const std::vector<uint8_t> &data) {
//   ESP_LOGD(TAG, "[%s] parse_and_publish(). command: [%s].", this->get_name().c_str(), this->cmd_str_);

//   // float result = payload_to_float(data, *this) / multiply_by_;

//   // // Is there a lambda registered
//   // // call it with the pre converted value and the raw data array
//   // if (this->transform_func_.has_value()) {
//   //   // the lambda can parse the response itself
//   //   auto val = (*this->transform_func_)(this, result, data);
//   //   if (val.has_value()) {
//   //     ESP_LOGV(TAG, "Value overwritten by lambda");
//   //     result = val.value();
//   //   }
//   // }
//   // ESP_LOGD(TAG, "Number new state : %.02f", result);
//   // // this->sensor_->raw_state = result;
//   // this->publish_state(result);
// }

void LGUartNumber::control(float value) {
  ESP_LOGD(TAG, "[%s] control(). Value: [%f]...", this->name_.c_str(), value);

  if (this->parent_->send_cmd(this->cmd_str_, (int) value)) {
    ESP_LOGD(TAG, "[%s] control(): %i - OK!", this->get_name().c_str());
  } else {
    ESP_LOGW(TAG, "[%s] control(): %i - NG!", this->get_name().c_str());
    this->status_set_warning();
    return;
  }

  // If the reply was OK, we can be very confident that the state the user wants is now in effect.
  this->publish_state((int) value);
}

void LGUartNumber::dump_config() {
  ESP_LOGCONFIG(TAG, "[%s] command string: '%s'", this->get_name().c_str(), this->cmd_str_);
}

// When parent gets a reply meant for us, we'll get notified here
// TODO: maybe refactor this so the signature is
//    const std::vector<uint8_t> &data
void LGUartNumber::on_reply_packet(uint8_t pkt[]) {
  ESP_LOGD(TAG, "[%s] on_reply_packet(). reply: [%s].", this->get_name().c_str(), pkt);

  // We are called only when the reply was OK.
  // We will need to pull out the two chars, string -> int.
  std::string status_str;
  status_str.push_back(pkt[7]);
  status_str.push_back(pkt[8]);
  ESP_LOGD(TAG, "[%s] on_reply_packet(): status_str: [%s]", this->get_name().c_str(), status_str.c_str());

  if (status_str[0] == 0) {
    ESP_LOGE(TAG, "[%s] on_reply_packet(): Didn't get status_str: [%s]", this->get_name().c_str(), status_str.c_str());
    this->status_set_error();
  }
  // TODO: make safe? Possible to get back some chars that do not convert to int?

  // For volume, at least, LG seems to do hex encoding so 100 volume is 0x64
  int status_hex = stoi(status_str, nullptr, 16);

  ESP_LOGD(TAG, "[%s] on_reply_packet(): state: [%f], status_hex: [%i]", this->get_name().c_str(), this->state,
           status_hex);

  if (this->status_has_error())
    this->status_clear_error();

  this->publish_state((float) status_hex);
}

void LGUartNumber::update() {
  ESP_LOGD(TAG, "[%s] update(). command: [%s].", this->get_name().c_str(), this->cmd_str_);
  this->parent_->send_cmd(this->cmd_str_, 0xff);
}

// void LGUartNumber::write_state(bool state) {
//   ESP_LOGD(TAG, "[%s] write_state(): %i", this->get_name().c_str(), state);

//   if (this->parent_->send_cmd(this->cmd_str_, (int) state)) {
//     ESP_LOGD(TAG, "[%s] write_state(): %i - OK!", this->get_name().c_str(), state);
//   } else {
//     ESP_LOGW(TAG, "[%s] write_state(): %i - NG!", this->get_name().c_str(), state);
//     // NG means that we can't confirm the state has changed, bail before publishing an updated state
//     this->status_set_warning();
//     return;
//   }

//   // If the reply was OK, we can be very confident that the state the user wants is now in effect.
//   this->publish_state(state);
// }

}  // namespace lg_uart
}  // namespace esphome
