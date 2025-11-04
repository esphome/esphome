#include "microphone.h"

namespace esphome {
namespace microphone {

void Microphone::add_data_callback(std::function<void(const std::vector<uint8_t> &)> &&data_callback) {
  std::function<void(const std::vector<uint8_t> &)> mute_handled_callback =
      [this, data_callback](const std::vector<uint8_t> &data) {
        if (this->mute_state_) {
          data_callback(std::vector<uint8_t>(data.size(), 0));
        } else {
          data_callback(data);
        };
      };
  this->data_callbacks_.add(std::move(mute_handled_callback));
}

}  // namespace microphone
}  // namespace esphome
