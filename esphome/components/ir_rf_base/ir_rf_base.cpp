#include "ir_rf_base.h"

#include <cinttypes>

#include "esphome/core/log.h"

#ifdef USE_API
#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/application.h"
#endif

namespace esphome::ir_rf_base {

static const char *const TAG = "ir_rf";

void IrRfEntity::setup_transport_(IrRfTraits &traits) {
  traits.set_supports_transmitter(this->has_transmitter());
  traits.set_supports_receiver(this->has_receiver());

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

bool IrRfEntity::transmit_raw_(const IrRfCallData &call, uint32_t carrier_frequency_hz) {
  if (this->transmitter_ == nullptr) {
    ESP_LOGW(TAG, "No transmitter configured");
    return false;
  }

  if (!call.has_raw_timings()) {
    ESP_LOGE(TAG, "No raw timings provided");
    return false;
  }

  auto transmit_call = this->transmitter_->transmit();
  auto *transmit_data = transmit_call.get_data();
  transmit_data->set_carrier_frequency(carrier_frequency_hz);

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

  if (call.get_repeat_count() > 0) {
    transmit_call.set_send_times(call.get_repeat_count());
  }

  this->inflight_seq_ = transmit_call.get_seq();
  transmit_call.perform();
  return true;
}

bool IrRfEntity::on_receive(remote_base::RemoteReceiveData data) {
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

void IrRfEntity::notify_transmit_complete_() {
#if defined(USE_API) && defined(USE_IR_RF)
  if (this->api_reply_ == ApiReply::API_REPLY_WAITING)
    this->finish_api_reply_(true);
#endif
}

#if defined(USE_API) && defined(USE_IR_RF)
// Safety net for a transmitter that never reports completion (for example a failed component)
static constexpr uint32_t API_REPLY_TIMEOUT_MS = 30000;

void IrRfEntity::expect_api_reply_(api::APIConnection *conn) {
  if (this->api_reply_ == ApiReply::API_REPLY_WAITING) {
    // only an unpaced client gets here; its earlier request is answered as not started
    this->finish_api_reply_(false);
  }
  this->api_reply_connection_ = conn;
  this->api_reply_registered_ms_ = App.get_loop_component_start_time();
  this->api_reply_ = ApiReply::API_REPLY_WAITING;
  this->enable_loop();
}

void IrRfEntity::finish_api_reply_(bool success) {
  this->api_reply_ = success ? ApiReply::API_REPLY_OWED_OK : ApiReply::API_REPLY_OWED_FAILED;
  this->send_api_reply_();
}

bool IrRfEntity::send_api_reply_() {
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
void IrRfEntity::loop() {
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

void IrRfEntity::on_api_connection_closed(api::APIConnection *conn) {
  if (this->api_reply_connection_ == conn) {
    this->api_reply_ = ApiReply::API_REPLY_NONE;
    this->disable_loop();
  }
}
#endif

}  // namespace esphome::ir_rf_base
