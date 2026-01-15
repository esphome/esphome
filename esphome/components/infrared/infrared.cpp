#include "infrared.h"
#include "esphome/core/log.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif

namespace esphome::infrared {

static const char *const TAG = "infrared";

// ========== InfraredCall ==========

InfraredCall &InfraredCall::set_carrier_frequency(uint32_t frequency) {
  this->carrier_frequency_ = frequency;
  return *this;
}

InfraredCall &InfraredCall::set_raw_timings(const std::vector<int32_t> &timings) {
  this->raw_timings_ = &timings;
  this->packed_data_ = nullptr;  // Clear packed if vector is set
  return *this;
}

InfraredCall &InfraredCall::set_raw_timings_packed(const uint8_t *data, uint16_t length, uint16_t count) {
  this->packed_data_ = data;
  this->packed_length_ = length;
  this->packed_count_ = count;
  this->raw_timings_ = nullptr;  // Clear vector if packed is set
  return *this;
}

InfraredCall &InfraredCall::set_repeat_count(uint32_t count) {
  this->repeat_count_ = count;
  return *this;
}

void InfraredCall::perform() {
  if (this->parent_ != nullptr) {
    this->parent_->control(*this);
  }
}

// ========== Infrared ==========

void Infrared::setup() {
  // Set up traits based on configuration
  this->traits_.set_supports_transmitter(this->has_transmitter());
  this->traits_.set_supports_receiver(this->has_receiver());

  // Register as listener for received IR data
  if (this->receiver_ != nullptr) {
    this->receiver_->register_listener(this);
  }
}

void Infrared::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Infrared '%s'\n"
                "  Supports Transmitter: %s\n"
                "  Supports Receiver: %s",
                this->get_name().c_str(), YESNO(this->traits_.get_supports_transmitter()),
                YESNO(this->traits_.get_supports_receiver()));
}

InfraredCall Infrared::make_call() { return InfraredCall(this); }

void Infrared::control(const InfraredCall &call) {
  if (this->transmitter_ == nullptr) {
    ESP_LOGW(TAG, "No transmitter configured");
    return;
  }

  if (!call.has_raw_timings()) {
    ESP_LOGE(TAG, "No raw timings provided");
    return;
  }

  // Create transmit data object
  auto transmit_call = this->transmitter_->transmit();
  auto *transmit_data = transmit_call.get_data();

  // Set carrier frequency
  if (call.get_carrier_frequency().has_value()) {
    transmit_data->set_carrier_frequency(call.get_carrier_frequency().value());
  }

  // Set timings based on format
  if (call.is_packed()) {
    // Zero-copy from packed protobuf data
    transmit_data->set_data_from_packed_sint32(call.get_packed_data(), call.get_packed_length(),
                                               call.get_packed_count());
    ESP_LOGD(TAG, "Transmitting packed raw timings: count=%u, repeat=%u", call.get_packed_count(),
             call.get_repeat_count());
  } else {
    // From vector (lambdas/automations)
    transmit_data->set_data(call.get_raw_timings());
    ESP_LOGD(TAG, "Transmitting raw timings: count=%zu, repeat=%u", call.get_raw_timings().size(),
             call.get_repeat_count());
  }

  // Set repeat count
  if (call.get_repeat_count() > 0) {
    transmit_call.set_send_times(call.get_repeat_count());
  }

  // Perform transmission
  transmit_call.perform();
}

uint32_t Infrared::get_capability_flags() const {
  uint32_t flags = 0;

  // Add transmit/receive capability based on traits
  if (this->traits_.get_supports_transmitter())
    flags |= InfraredCapability::CAPABILITY_TRANSMITTER;
  if (this->traits_.get_supports_receiver())
    flags |= InfraredCapability::CAPABILITY_RECEIVER;

  return flags;
}

bool Infrared::on_receive(remote_base::RemoteReceiveData data) {
  // Forward received IR data to API server
#if defined(USE_API) && defined(USE_IR_RF)
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

}  // namespace esphome::infrared
