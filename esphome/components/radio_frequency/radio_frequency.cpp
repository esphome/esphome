#include "radio_frequency.h"

#include <cinttypes>

#include "esphome/core/log.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

namespace esphome::radio_frequency {

static const char *const TAG = "radio_frequency";

// ========== RadioFrequencyCall ==========

RadioFrequencyCall &RadioFrequencyCall::set_frequency(uint32_t frequency_hz) {
  this->frequency_hz_ = frequency_hz;
  return *this;
}

RadioFrequencyCall &RadioFrequencyCall::set_modulation(RadioFrequencyModulation modulation) {
  this->modulation_ = modulation;
  return *this;
}

RadioFrequencyCall &RadioFrequencyCall::set_raw_timings(const std::vector<int32_t> &timings) {
  this->raw_timings_ = &timings;
  this->packed_data_ = nullptr;
  this->base64url_ptr_ = nullptr;
  return *this;
}

RadioFrequencyCall &RadioFrequencyCall::set_raw_timings_base64url(const std::string &base64url) {
  this->base64url_ptr_ = &base64url;
  this->raw_timings_ = nullptr;
  this->packed_data_ = nullptr;
  return *this;
}

RadioFrequencyCall &RadioFrequencyCall::set_raw_timings_packed(const uint8_t *data, uint16_t length, uint16_t count) {
  this->packed_data_ = data;
  this->packed_length_ = length;
  this->packed_count_ = count;
  this->raw_timings_ = nullptr;
  this->base64url_ptr_ = nullptr;
  return *this;
}

RadioFrequencyCall &RadioFrequencyCall::set_repeat_count(uint32_t count) {
  this->repeat_count_ = count;
  return *this;
}

void RadioFrequencyCall::perform() {
  if (this->parent_ != nullptr) {
    // Fire any on_control hooks (user-wired automations) before handing off to
    // the platform-specific control() — gives users a chance to react to call
    // parameters (e.g. retune an external RF front-end based on call.get_frequency()).
    this->parent_->control_callback_.call(*this);
    this->parent_->control(*this);
  }
}

// ========== RadioFrequency ==========

void RadioFrequency::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Radio Frequency '%s'\n"
                "  Supports Transmitter: %s\n"
                "  Supports Receiver: %s",
                this->get_name().c_str(), YESNO(this->traits_.get_supports_transmitter()),
                YESNO(this->traits_.get_supports_receiver()));
  if (this->traits_.get_frequency_min_hz() > 0) {
    if (this->traits_.get_frequency_min_hz() == this->traits_.get_frequency_max_hz()) {
      ESP_LOGCONFIG(TAG, "  Frequency: %" PRIu32 " Hz (fixed)", this->traits_.get_frequency_min_hz());
    } else {
      ESP_LOGCONFIG(TAG, "  Frequency Range: %" PRIu32 " - %" PRIu32 " Hz", this->traits_.get_frequency_min_hz(),
                    this->traits_.get_frequency_max_hz());
    }
  }
}

RadioFrequencyCall RadioFrequency::make_call() { return RadioFrequencyCall(this); }

uint32_t RadioFrequency::get_capability_flags() const {
  uint32_t flags = 0;
  if (this->traits_.get_supports_transmitter())
    flags |= RadioFrequencyCapability::CAPABILITY_TRANSMITTER;
  if (this->traits_.get_supports_receiver())
    flags |= RadioFrequencyCapability::CAPABILITY_RECEIVER;
  return flags;
}

bool RadioFrequency::on_receive(remote_base::RemoteReceiveData data) {
  // Invoke local callbacks
  this->receive_callback_.call(data);

  // Forward received RF data to API server
#if defined(USE_API) && defined(USE_RADIO_FREQUENCY)
  if (api::global_api_server != nullptr) {
#ifdef USE_DEVICES
    uint32_t device_id = this->get_device_id();
#else
    uint32_t device_id = 0;
#endif
    api::global_api_server->send_infrared_rf_receive_event(device_id, this->get_object_id_hash(), &data.get_raw_data());
  }
#endif
  return false;  // Don't consume the event, allow other listeners to process it
}

}  // namespace esphome::radio_frequency
