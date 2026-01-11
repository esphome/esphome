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
  this->raw_timings_ = timings;
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

#ifdef USE_API
  // Register with API server
  if (api::global_api_server != nullptr) {
    api::global_api_server->register_infrared(this);
  }
#endif
}

void Infrared::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Infrared:\n"
                "  Supports Transmitter: %s\n"
                "  Supports Receiver: %s",
                YESNO(this->traits_.get_supports_transmitter()), YESNO(this->traits_.get_supports_receiver()));
  LOG_ENTITY_BASE("  ", "", this);
}

InfraredCall Infrared::make_call() { return InfraredCall(this); }

void Infrared::control(const InfraredCall &call) {
  if (this->transmitter_ == nullptr) {
    ESP_LOGW(TAG, "No transmitter configured");
    return;
  }

  if (call.raw_timings_.empty()) {
    ESP_LOGE(TAG, "Raw timings array is empty");
    return;
  }

  ESP_LOGD(TAG, "Transmitting raw timings: timing_count=%u, repeat_count=%u", call.raw_timings_.size(),
           call.repeat_count_);

  // Create transmit data object
  auto transmit_call = this->transmitter_->transmit();
  auto *transmit_data = transmit_call.get_data();

  // Set carrier frequency
  if (call.carrier_frequency_.has_value()) {
    transmit_data->set_carrier_frequency(call.carrier_frequency_.value());
  }

  // Copy raw timings to transmit data
  // Timings format: positive values = mark (LED on), negative values = space (LED off)
  for (const auto &timing : call.raw_timings_) {
    if (timing > 0) {
      transmit_data->mark(static_cast<uint32_t>(timing));
    } else {
      transmit_data->space(static_cast<uint32_t>(-timing));
    }
  }

  // Set repeat count
  if (call.repeat_count_ > 0) {
    transmit_call.set_send_times(call.repeat_count_);
  }

  // Perform transmission
  transmit_call.perform();
}

#ifdef USE_API
uint32_t Infrared::get_capability_flags() const {
  uint32_t flags = 0;

  // Add transmit/receive capability based on traits
  if (this->traits_.get_supports_transmitter())
    flags |= InfraredCapability::CAPABILITY_TRANSMITTER;
  if (this->traits_.get_supports_receiver())
    flags |= InfraredCapability::CAPABILITY_RECEIVER;

  return flags;
}

void Infrared::transmit_raw_timings(const api::InfraredTransmitRawTimingsRequest &msg) {
  if (this->transmitter_ == nullptr) {
    ESP_LOGW(TAG, "No transmitter configured");
    return;
  }

  if (msg.timings.empty()) {
    ESP_LOGE(TAG, "Raw timings array is empty");
    return;
  }

  ESP_LOGD(TAG, "Transmitting raw timings: key=%u, carrier=%u Hz, timing_count=%u, repeat_count=%u", msg.key,
           msg.carrier_frequency, msg.timings.size(), msg.repeat_count);

  // Create transmit data object
  auto call = this->transmitter_->transmit();
  auto *transmit_data = call.get_data();

  // Set carrier frequency
  transmit_data->set_carrier_frequency(msg.carrier_frequency);

  // Set the raw timings data
  // Timings format: positive values = mark (LED on), negative values = space (LED off)
  transmit_data->set_data(msg.timings);

  // Set repeat count (default to 1 if not specified or 0)
  if (msg.repeat_count > 0) {
    call.set_send_times(msg.repeat_count);
  }

  // Transmit the data
  call.perform();
}
#endif  // USE_API

}  // namespace esphome::infrared
