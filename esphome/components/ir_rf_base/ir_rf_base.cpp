#include "ir_rf_base.h"

#include <algorithm>
#include <cinttypes>

#include "esphome/core/log.h"

#ifdef USE_API
#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_server.h"
#include "esphome/core/application.h"
#endif

namespace esphome::ir_rf_base {

static const char *const TAG = "ir_rf";

#ifdef USE_IR_RF_TRANSMIT_COMPLETE
// Safety net for a transmitter that never reports completion (for example a failed component):
// the request is answered as failed this long after its frame should have left the wire
static constexpr uint32_t API_REPLY_TIMEOUT_MS = 30000;
// Longest air time added to the deadline, in 16 ms ticks (8 min); with the 30 s above the
// deadline stays within the signed 16 bit tick window loop() compares against
static constexpr uint16_t API_REPLY_MAX_AIR_TICKS = 30000;
#endif

void IrRfEntity::setup() {
  // merged, not assigned: a platform may have set a flag for its own hardware before setup()
  if (this->has_transmitter())
    this->supports_transmitter_ = true;
  if (this->has_receiver())
    this->supports_receiver_ = true;

  if (this->receiver_ != nullptr) {
    this->receiver_->register_listener(this);
  }
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

#ifdef USE_IR_RF_TRANSMIT_COMPLETE
  if (call.wants_api_reply()) {
    // only an API frame claims the seq, so a YAML transmit cannot take over a pending reply
    this->inflight_seq_ = transmit_call.get_seq();
    // a long frame must not be answered as failed while still on the wire: the 30 s safety net
    // starts after this frame's own air time (capped so the tick comparison cannot wrap)
    uint64_t frame_us = 0;
    for (int32_t timing : transmit_data->get_data())
      frame_us += static_cast<uint32_t>(timing < 0 ? -timing : timing);
    const uint64_t air_ticks = (frame_us * std::max<uint32_t>(call.get_repeat_count(), 1) / 1000) >> 4;
    this->api_reply_deadline_ += static_cast<uint16_t>(std::min<uint64_t>(air_ticks, API_REPLY_MAX_AIR_TICKS));
  }
#endif
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

#if defined(USE_API) && defined(USE_IR_RF)

#ifdef USE_IR_RF_TRANSMIT_COMPLETE
void IrRfEntity::on_transmit_complete(remote_base::RemoteTransmitterBase *transmitter, uint16_t seq, bool sent) {
  // only the frame this entity submitted; YAML automations and other entities share the transmitter
  if (transmitter == this->transmitter_ && seq == this->inflight_seq_ &&
      this->api_reply_ == ApiReply::API_REPLY_WAITING)
    this->finish_api_reply_(sent);
}
#endif

void IrRfEntity::expect_api_reply_(api::APIConnection *conn) {
  // only an unpaced client gets here: its earlier request is answered as not started, and a reply
  // the buffer still owes gets one last try, since the slot goes to the new request
  if (this->api_reply_ == ApiReply::API_REPLY_WAITING)
    this->finish_api_reply_(false);
  if (this->api_reply_ != ApiReply::API_REPLY_NONE && !this->send_api_reply_()) {
    ESP_LOGW(TAG, "'%s': transmit %s", this->get_name().c_str(), LOG_STR_LITERAL("reply displaced"));
  }
  this->api_reply_connection_ = conn;
#ifdef USE_IR_RF_TRANSMIT_COMPLETE
  this->api_reply_deadline_ =
      static_cast<uint16_t>((App.get_loop_component_start_time() >> 4) + (API_REPLY_TIMEOUT_MS >> 4));
#endif
  this->api_reply_ = ApiReply::API_REPLY_WAITING;
}

void IrRfEntity::finish_api_reply_(bool success) {
  this->api_reply_ = success ? ApiReply::API_REPLY_OWED_OK : ApiReply::API_REPLY_OWED_FAILED;
  this->send_api_reply_();
}

bool IrRfEntity::send_api_reply_() {
  uint32_t device_id = 0;
#ifdef USE_DEVICES
  device_id = this->get_device_id();
#endif
  // Refused by a full TCP buffer: the reply stays owed and loop() retries it, since a lost
  // reply would stall the client's pacing for good (same shape as bluetooth_proxy)
  if (!this->api_reply_connection_->send_infrared_rf_transmit_complete(device_id, this->get_object_id_hash(),
                                                                       this->api_reply_ == ApiReply::API_REPLY_OWED_OK))
    return false;
  this->clear_api_reply_();
  return true;
}

// Only runs while an API reply is pending: retries an owed one until it goes out or the client is
// gone, and expires a transmit that never reported (a platform overriding control() without wiring
// its transmitter's completion)
void IrRfEntity::loop() {
  if (this->api_reply_ == ApiReply::API_REPLY_NONE) {
    this->disable_loop();
    return;
  }
  if (this->api_reply_ != ApiReply::API_REPLY_WAITING) {
    this->send_api_reply_();
    return;
  }
#ifdef USE_IR_RF_TRANSMIT_COMPLETE
  const auto remaining = static_cast<int16_t>(this->api_reply_deadline_ - (App.get_loop_component_start_time() >> 4));
  if (remaining > 0)
    return;
#endif
  ESP_LOGW(TAG, "'%s': transmit %s", this->get_name().c_str(), LOG_STR_LITERAL("never reported completion"));
  this->finish_api_reply_(false);
}

void IrRfEntity::on_api_connection_closed(api::APIConnection *conn) {
  if (this->api_reply_connection_ == conn) {
    this->clear_api_reply_();
  }
}
#endif

}  // namespace esphome::ir_rf_base

#ifdef USE_IR_RF_TRANSMIT_COMPLETE
namespace esphome::remote_base {

void ir_rf_transmit_complete(RemoteTransmitterBase *transmitter, uint16_t seq, bool sent) {
#ifdef USE_INFRARED
  for (auto *entity : App.get_infrareds()) {
    entity->on_transmit_complete(transmitter, seq, sent);
  }
#endif
#ifdef USE_RADIO_FREQUENCY
  for (auto *entity : App.get_radio_frequencies()) {
    entity->on_transmit_complete(transmitter, seq, sent);
  }
#endif
}

}  // namespace esphome::remote_base
#endif
