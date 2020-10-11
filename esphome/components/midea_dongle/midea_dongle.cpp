#include "midea_dongle.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace midea_dongle {

void MideaDongle::loop() {
  while (this->available()) {
    const uint8_t rx = this->read();

    switch (this->idx_) {
      case OFFSET_START:
        if (rx != SYNC_BYTE)
          continue;
        break;
      case OFFSET_LENGTH:
        if (!rx || rx >= sizeof(buf_)) {
          reset_();
          continue;
        }
        this->cnt_ = rx;
    }

    this->buf_[this->idx_++] = rx;

    if (--this->cnt_)
      continue;

    reset_();

    BaseFrame frame(this->buf_);

    if (!frame.is_valid())
      continue;

    for (auto &listener : this->listeners_)
      if (listener.app_type == frame.app_type() || frame.app_type() == BROADCAST)
        listener.on_frame(frame);
  }
}

void MideaDongle::register_listener(MideaAppliance app_type, const std::function<void(Frame &)> &func) {
  auto listener = MideaListener {
      .app_type = app_type,
      .on_frame = func,
  };
  this->listeners_.push_back(listener);
}

}  // namespace midea_dongle
}  // namespace esphome
