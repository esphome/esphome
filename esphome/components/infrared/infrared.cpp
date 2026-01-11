#include "infrared.h"
#include "esphome/core/log.h"

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

  if (call.get_raw_timings().empty()) {
    ESP_LOGE(TAG, "Raw timings array is empty");
    return;
  }

  ESP_LOGD(TAG, "Transmitting raw timings: timing_count=%u, repeat_count=%u", call.get_raw_timings().size(),
           call.get_repeat_count());

  // Create transmit data object
  auto transmit_call = this->transmitter_->transmit();
  auto *transmit_data = transmit_call.get_data();

  // Set carrier frequency
  if (call.get_carrier_frequency().has_value()) {
    transmit_data->set_carrier_frequency(call.get_carrier_frequency().value());
  }

  // Copy raw timings to transmit data
  // Timings format: positive values = mark (LED on), negative values = space (LED off)
  for (const auto &timing : call.get_raw_timings()) {
    if (timing > 0) {
      transmit_data->mark(static_cast<uint32_t>(timing));
    } else {
      transmit_data->space(static_cast<uint32_t>(-timing));
    }
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

void Infrared::transmit_raw_timings(uint32_t carrier_frequency, const uint8_t *timings_data, uint16_t timings_length,
                                    uint16_t timings_count, uint32_t repeat_count) {
  if (this->transmitter_ == nullptr) {
    ESP_LOGW(TAG, "No transmitter configured");
    return;
  }

  if (timings_count == 0) {
    ESP_LOGE(TAG, "Raw timings array is empty");
    return;
  }

  ESP_LOGD(TAG, "Transmitting raw timings: carrier=%u Hz, timing_count=%u, repeat_count=%u", carrier_frequency,
           timings_count, repeat_count);

  // Create transmit data object
  auto call = this->transmitter_->transmit();
  auto *transmit_data = call.get_data();

  // Set carrier frequency
  transmit_data->set_carrier_frequency(carrier_frequency);

  // Decode packed sint32 timings directly from protobuf data using remote_base helper
  transmit_data->set_data_from_packed_sint32(timings_data, timings_length, timings_count);

  // Set repeat count (default to 1 if not specified or 0)
  if (repeat_count > 0) {
    call.set_send_times(repeat_count);
  }

  // Transmit the data
  call.perform();
}

}  // namespace esphome::infrared
