#include "lg_uart_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lg_uart {

void LGUartSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "[%s] command_string: [%s], encoding_base:[%i] ...", this->name_.c_str(), this->cmd_str_,
                this->encoding_base_);
}

std::string LGUartSensor::describe() { return this->get_name(); }

// When parent gets a reply meant for us, we'll get notified here
// TODO: maybe refactor this so the signature is
//    const std::vector<uint8_t> &data

void LGUartSensor::on_reply_packet(uint8_t pkt[]) {
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
  // TODO: make this user adjustable
  // For active input, LG does base10
  int status_hex = stoi(status_str, nullptr, this->encoding_base_);

  ESP_LOGD(TAG, "[%s] on_reply_packet(): state: [%f], status_hex: [%i]", this->get_name().c_str(), this->state,
           status_hex);

  if (this->status_has_error())
    this->status_clear_error();

  this->publish_state((float) status_hex);
}

void LGUartSensor::update() {
  ESP_LOGD(TAG, "[%s] update(). command: [%s].", this->get_name().c_str(), this->cmd_str_);
  this->parent_->send_cmd(this->cmd_str_, 0xff);
}

}  // namespace lg_uart
}  // namespace esphome
