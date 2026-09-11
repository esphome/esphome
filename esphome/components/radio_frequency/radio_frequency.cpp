#include "radio_frequency.h"

#include <cinttypes>

#include "esphome/core/log.h"

#ifdef USE_API
#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/application.h"
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

bool RadioFrequencyCall::perform() {
  if (this->parent_ == nullptr) {
    return false;
  }
  // Fire any on_control hooks (user-wired automations) before handing off to
  // the platform-specific control() — gives users a chance to react to call
  // parameters (e.g. retune an external RF front-end based on call.get_frequency()).
  this->parent_->control_callback_.call(*this);
#if defined(USE_API) && defined(USE_RADIO_FREQUENCY)
  // Before control(): blocking transmitters report completion from inside it, and the
  // non-blocking RMT path flushes the previous frame's completion there
  if (this->api_connection_ != nullptr)
    this->parent_->expect_api_reply_(this->api_connection_);
#endif
  const bool started = this->parent_->control(*this);
#if defined(USE_API) && defined(USE_RADIO_FREQUENCY)
  if (!started && this->api_connection_ != nullptr)
    this->parent_->finish_api_reply_(false);
#endif
  return started;
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

void RadioFrequency::notify_transmit_complete_() {
#if defined(USE_API) && defined(USE_RADIO_FREQUENCY)
  if (this->api_reply_ == ApiReply::API_REPLY_WAITING)
    this->finish_api_reply_(true);
#endif
}

#if defined(USE_API) && defined(USE_RADIO_FREQUENCY)
// Safety net for a transmitter that never reports completion (for example a failed component)
static constexpr uint32_t API_REPLY_TIMEOUT_MS = 30000;

void RadioFrequency::expect_api_reply_(api::APIConnection *conn) {
  if (this->api_reply_ == ApiReply::API_REPLY_WAITING) {
    // only an unpaced client gets here; its earlier request is answered as not started
    this->finish_api_reply_(false);
  }
  this->api_reply_connection_ = conn;
  this->api_reply_registered_ms_ = App.get_loop_component_start_time();
  this->api_reply_ = ApiReply::API_REPLY_WAITING;
  this->enable_loop();
}

void RadioFrequency::finish_api_reply_(bool success) {
  this->api_reply_ = success ? ApiReply::API_REPLY_OWED_OK : ApiReply::API_REPLY_OWED_FAILED;
  this->send_api_reply_();
}

bool RadioFrequency::send_api_reply_() {
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
void RadioFrequency::loop() {
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

void RadioFrequency::on_api_connection_closed(api::APIConnection *conn) {
  if (this->api_reply_connection_ == conn) {
    this->api_reply_ = ApiReply::API_REPLY_NONE;
    this->disable_loop();
  }
}
#endif

}  // namespace esphome::radio_frequency
