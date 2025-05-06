#include "microphone.h"

namespace esphome {
namespace microphone {

void Microphone::add_data_callback(std::function<void(const std::vector<uint8_t> &)> &&data_callback) {
  std::function<void(const std::vector<uint8_t> &)> mute_handled_callback =
      [this, data_callback](const std::vector<uint8_t> &data) { data_callback(this->silence_audio_(data)); };
  this->data_callbacks_.add(std::move(mute_handled_callback));
}

std::vector<uint8_t> Microphone::silence_audio_(std::vector<uint8_t> data) {
  if (this->mute_state_) {
    std::memset((void *) data.data(), 0, data.size());
  }

  return data;
}

}  // namespace microphone
}  // namespace esphome
