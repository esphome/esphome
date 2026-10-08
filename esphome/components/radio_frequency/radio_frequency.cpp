#include "radio_frequency.h"

#include <cinttypes>

#include "esphome/core/log.h"

namespace esphome::radio_frequency {

static const char *const TAG = "radio_frequency";

void RadioFrequency::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Radio Frequency '%s'\n"
                "  Supports Transmitter: %s\n"
                "  Supports Receiver: %s",
                this->get_name().c_str(), YESNO(this->get_supports_transmitter()),
                YESNO(this->get_supports_receiver()));
  if (this->traits_.get_frequency_min_hz() > 0) {
    if (this->traits_.get_frequency_min_hz() == this->traits_.get_frequency_max_hz()) {
      ESP_LOGCONFIG(TAG, "  Frequency: %" PRIu32 " Hz (fixed)", this->traits_.get_frequency_min_hz());
    } else {
      ESP_LOGCONFIG(TAG, "  Frequency Range: %" PRIu32 " - %" PRIu32 " Hz", this->traits_.get_frequency_min_hz(),
                    this->traits_.get_frequency_max_hz());
    }
  }
}

bool RadioFrequency::on_receive(remote_base::RemoteReceiveData data) {
  this->receive_callback_.call(data);
  return IrRfEntity::on_receive(data);
}

}  // namespace esphome::radio_frequency
