#include "infrared.h"

#include <cinttypes>

#include "esphome/core/log.h"

#ifdef USE_API
#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/application.h"
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
  this->packed_data_ = nullptr;
  this->base64url_ptr_ = nullptr;
  return *this;
}

InfraredCall &InfraredCall::set_raw_timings_base64url(const std::string &base64url) {
  this->base64url_ptr_ = &base64url;
  this->raw_timings_ = nullptr;
  this->packed_data_ = nullptr;
  return *this;
}

InfraredCall &InfraredCall::set_raw_timings_packed(const uint8_t *data, uint16_t length, uint16_t count) {
  this->packed_data_ = data;
  this->packed_length_ = length;
  this->packed_count_ = count;
  this->raw_timings_ = nullptr;
  this->base64url_ptr_ = nullptr;
  return *this;
}

InfraredCall &InfraredCall::set_repeat_count(uint32_t count) {
  this->repeat_count_ = count;
  return *this;
}

bool InfraredCall::perform() {
  if (this->parent_ == nullptr)
    return false;
#if defined(USE_API) && defined(USE_IR_RF)
  // Before control(): blocking transmitters report completion from inside it, and the
  // non-blocking RMT path flushes the previous frame's completion there
  if (this->api_connection_ != nullptr)
    this->parent_->expect_api_reply_(this->api_connection_);
#endif
  const bool started = this->parent_->control(*this);
#if defined(USE_API) && defined(USE_IR_RF)
  if (!started && this->api_connection_ != nullptr)
    this->parent_->finish_api_reply_(false);
#endif
  return started;
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
#if defined(USE_API) && defined(USE_IR_RF)
  if (this->transmitter_ != nullptr) {
    // only frames this entity submitted; YAML automations and other entities share the transmitter
    this->transmitter_->add_on_complete_callback([this](uint32_t seq) {
      if (seq == this->inflight_seq_)
        this->notify_transmit_complete_();
    });
  }
#endif
}

void Infrared::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Infrared '%s'\n"
                "  Supports Transmitter: %s\n"
                "  Supports Receiver: %s",
                this->get_name().c_str(), YESNO(this->traits_.get_supports_transmitter()),
                YESNO(this->traits_.get_supports_receiver()));
}

bool Infrared::control(const InfraredCall &call) {
  if (this->transmitter_ == nullptr) {
    ESP_LOGW(TAG, "No transmitter configured");
    return false;
  }

  if (!call.has_raw_timings()) {
    ESP_LOGE(TAG, "No raw timings provided");
    return false;
  }

  // Create transmit data object
  auto transmit_call = this->transmitter_->transmit();
  auto *transmit_data = transmit_call.get_data();

  // Set carrier frequency
  auto freq = call.get_carrier_frequency();
  if (freq.has_value()) {
    transmit_data->set_carrier_frequency(*freq);
  }

  // Set timings based on format
  if (call.is_packed()) {
    // Zero-copy from packed protobuf data
    transmit_data->set_data_from_packed_sint32(call.get_packed_data(), call.get_packed_length(),
                                               call.get_packed_count());
    ESP_LOGD(TAG, "Transmitting packed raw timings: count=%" PRIu16 ", repeat=%" PRIu32, call.get_packed_count(),
             call.get_repeat_count());
  } else if (call.is_base64url()) {
    // Decode base64url (URL-safe) into transmit buffer
    if (!transmit_data->set_data_from_base64url(call.get_base64url_data())) {
      ESP_LOGE(TAG, "Invalid base64url data");
      return false;
    }
    // Sanity check: validate timing values are within reasonable bounds
    constexpr int32_t max_timing_us = 500000;  // 500ms absolute max
    for (int32_t timing : transmit_data->get_data()) {
      int32_t abs_timing = timing < 0 ? -timing : timing;
      if (abs_timing > max_timing_us) {
        ESP_LOGE(TAG, "Invalid timing value: %" PRId32 " µs (max %" PRId32 ")", timing, max_timing_us);
        return false;
      }
    }
    ESP_LOGD(TAG, "Transmitting base64url raw timings: count=%zu, repeat=%" PRIu32, transmit_data->get_data().size(),
             call.get_repeat_count());
  } else {
    // From vector (lambdas/automations)
    transmit_data->set_data(call.get_raw_timings());
    ESP_LOGD(TAG, "Transmitting raw timings: count=%zu, repeat=%" PRIu32, call.get_raw_timings().size(),
             call.get_repeat_count());
  }

  // Set repeat count
  if (call.get_repeat_count() > 0) {
    transmit_call.set_send_times(call.get_repeat_count());
  }

  // Perform transmission
  this->inflight_seq_ = transmit_call.get_seq();
  transmit_call.perform();
  return true;
}

void Infrared::notify_transmit_complete_() {
#if defined(USE_API) && defined(USE_IR_RF)
  if (this->api_reply_ == ApiReply::API_REPLY_WAITING)
    this->finish_api_reply_(true);
#endif
}

#if defined(USE_API) && defined(USE_IR_RF)
// Safety net for a transmitter that never reports completion (for example a failed component)
static constexpr uint32_t API_REPLY_TIMEOUT_MS = 30000;

void Infrared::expect_api_reply_(api::APIConnection *conn) {
  if (this->api_reply_ == ApiReply::API_REPLY_WAITING) {
    // only an unpaced client gets here; its earlier request is answered as not started
    this->finish_api_reply_(false);
  }
  this->api_reply_connection_ = conn;
  this->api_reply_registered_ms_ = App.get_loop_component_start_time();
  this->api_reply_ = ApiReply::API_REPLY_WAITING;
  this->enable_loop();
}

void Infrared::finish_api_reply_(bool success) {
  this->api_reply_ = success ? ApiReply::API_REPLY_OWED_OK : ApiReply::API_REPLY_OWED_FAILED;
  this->send_api_reply_();
}

bool Infrared::send_api_reply_() {
  api::InfraredRFTransmitCompleteResponse resp{};
#ifdef USE_DEVICES
  resp.device_id = this->get_device_id();
#endif
  resp.key = this->get_object_id_hash();
  resp.success = this->api_reply_ == ApiReply::API_REPLY_OWED_OK;
  // Refused by a full TCP buffer: the reply stays owed and loop() retries it, since a lost
  // reply would stall the client's pacing for good (same shape as bluetooth_proxy)
  if (!this->api_reply_connection_->send_infrared_rf_transmit_complete_response(resp))
    return false;
  this->api_reply_ = ApiReply::API_REPLY_NONE;
  this->disable_loop();
  return true;
}

// Only runs while an API reply is pending: retries an owed one, expires a transmit that never reported
void Infrared::loop() {
  if (this->api_reply_ == ApiReply::API_REPLY_NONE) {
    this->disable_loop();
    return;
  }
  const bool waiting = this->api_reply_ == ApiReply::API_REPLY_WAITING;
  if (!waiting && this->send_api_reply_())
    return;
  if (App.get_loop_component_start_time() - this->api_reply_registered_ms_ < API_REPLY_TIMEOUT_MS)
    return;
  ESP_LOGW(TAG, "'%s': transmit %s", this->get_name().c_str(),
           waiting ? LOG_STR_LITERAL("never reported completion") : LOG_STR_LITERAL("reply undeliverable"));
  if (waiting) {
    this->finish_api_reply_(false);
  }
  this->api_reply_ = ApiReply::API_REPLY_NONE;
  this->disable_loop();
}

void Infrared::on_api_connection_closed(api::APIConnection *conn) {
  if (this->api_reply_connection_ == conn) {
    this->api_reply_ = ApiReply::API_REPLY_NONE;
    this->disable_loop();
  }
}
#endif

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
