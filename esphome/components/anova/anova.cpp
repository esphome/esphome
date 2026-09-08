#include "anova.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome::anova {

static const char *const TAG = "anova";

using namespace esphome::climate;

void Anova::dump_config() { LOG_CLIMATE("", "Anova BLE Cooker", this); }

void Anova::setup() {
  this->codec_ = make_unique<AnovaCodec>();
  this->poll_step_ = PollStep::IDLE;
}

void Anova::loop() {
  // Parent BLEClientNode has a loop() method, but this component uses
  // polling via update() and BLE callbacks so loop isn't needed
  this->disable_loop();
}

void Anova::write_request_(AnovaPacket *pkt) {
  auto status =
      esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->char_handle_,
                               pkt->length, pkt->data, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str(), status);
  }
}

void Anova::control(const ClimateCall &call) {
  auto mode_val = call.get_mode();
  if (mode_val.has_value()) {
    ClimateMode mode = *mode_val;
    AnovaPacket *pkt;
    switch (mode) {
      case climate::CLIMATE_MODE_OFF:
        pkt = this->codec_->get_stop_request();
        break;
      case climate::CLIMATE_MODE_HEAT:
        pkt = this->codec_->get_start_request();
        break;
      default:
        ESP_LOGW(TAG, "Unsupported mode: %d", mode);
        return;
    }
    this->write_request_(pkt);
  }
  auto target_temp = call.get_target_temperature();
  if (target_temp.has_value()) {
    this->write_request_(this->codec_->get_set_target_temp_request(*target_temp));
  }
}

void Anova::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      this->current_temperature = NAN;
      this->target_temperature = NAN;
      this->poll_step_ = PollStep::IDLE;
      this->publish_state();
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *chr = this->parent_->get_characteristic(ANOVA_SERVICE_UUID, ANOVA_CHARACTERISTIC_UUID);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "[%s] No control service found at device, not an Anova..?", this->get_name().c_str());
        ESP_LOGW(TAG, "[%s] Note, this component does not currently support Anova Nano.", this->get_name().c_str());
        break;
      }
      this->char_handle_ = chr->handle;

      auto status = esp_ble_gattc_register_for_notify(this->parent_->get_gattc_if(), this->parent_->get_remote_bda(),
                                                      chr->handle);
      if (status) {
        ESP_LOGW(TAG, "[%s] esp_ble_gattc_register_for_notify failed, status=%d", this->get_name().c_str(), status);
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      this->node_state = espbt::ClientState::ESTABLISHED;
      this->poll_step_ = PollStep::IDLE;
      this->update();  // begin the first poll cycle immediately
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->char_handle_)
        break;
      this->codec_->decode(param->notify.value, param->notify.value_len);
      if (this->codec_->has_target_temp()) {
        this->target_temperature = this->codec_->target_temp_;
      }
      if (this->codec_->has_current_temp()) {
        this->current_temperature = this->codec_->current_temp_;
      }
      if (this->codec_->has_running()) {
        this->mode = this->codec_->running_ ? climate::CLIMATE_MODE_HEAT : climate::CLIMATE_MODE_OFF;
      }
      if (this->codec_->has_unit()) {
        ESP_LOGD(TAG, "Anova units is %s", (this->codec_->unit_ == 'f') ? "fahrenheit" : "celsius");
      }
      this->publish_state();

      // Advance the poll cycle to its next request based on the reply we got.
      switch (this->poll_step_) {
        case PollStep::SET_UNIT:
          this->poll_step_ = PollStep::STATUS;
          this->write_request_(this->codec_->get_read_device_status_request());
          break;
        case PollStep::STATUS:
          this->poll_step_ = PollStep::TARGET;
          this->write_request_(this->codec_->get_read_target_temp_request());
          break;
        case PollStep::TARGET:
          this->poll_step_ = PollStep::CURRENT;
          this->write_request_(this->codec_->get_read_current_temp_request());
          break;
        case PollStep::CURRENT:
          this->poll_step_ = PollStep::IDLE;  // full cycle complete
          break;
        default:
          // A reply to an ad-hoc control() write, outside a managed cycle.
          break;
      }
      break;
    }
    default:
      break;
  }
}

void Anova::set_unit_of_measurement(const char *unit) { this->want_fahrenheit_ = !strncmp(unit, "f", 1); }

void Anova::update() {
  if (this->node_state != espbt::ClientState::ESTABLISHED)
    return;
  if (this->poll_step_ != PollStep::IDLE) {
    // The previous cycle never finished within a full polling interval -- a
    // reply was missed or a write failed. Restart the cycle rather than stall;
    // the polling interval itself acts as the timeout. A late reply from the
    // abandoned cycle is harmless: state decoding happens on every notify
    // regardless of step, and each notify sends at most one follow-up request.
    ESP_LOGW(TAG, "[%s] Poll cycle incomplete (step %u); restarting cycle", this->parent_->address_str(),
             static_cast<uint8_t>(this->poll_step_));
  }
  // Re-assert the configured unit at the start of every poll cycle, then fall
  // through the status/temperature reads via the notification handler. Always
  // command the configured unit (want_fahrenheit_) -- never the last value the
  // device reported, or a drift to 'c' would lock itself in.
  this->poll_step_ = PollStep::SET_UNIT;
  this->write_request_(this->codec_->get_set_unit_request(this->want_fahrenheit_ ? 'f' : 'c'));
}

}  // namespace esphome::anova

#endif
