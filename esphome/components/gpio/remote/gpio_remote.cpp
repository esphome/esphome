#include "gpio_remote.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gpio {

static const char *const TAG = "remote.gpio";

void GPIORemote::transmit(int repeat, int wait, const std::string &name, const std::vector<remote::arg_t> &args) {
  RemoteProtocolCodec *protocol = this->get_protocol(name);

  if (protocol == nullptr) {
    ESP_LOGE(TAG, "Unsupported protocol '%s'", name.c_str());
    return;
  }

  this->temp_.reset();
  protocol->encode(&this->temp_, args);
  ESP_LOGD(TAG, "Sending command:");
  protocol->dump(args);
  this->send_internal_(repeat, wait);
};

}  // namespace gpio
}  // namespace esphome
