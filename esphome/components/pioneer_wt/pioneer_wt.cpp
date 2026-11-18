#include "pioneer_wt.h"

#include <cinttypes>
#include <cmath>
#include <cstring>
#include <iterator>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::pioneer_wt {

ESPHOME_LOG_TAG(TAG, "pioneer_wt");

static constexpr uint32_t RESEND_INTERVAL_MS = 500;
static constexpr uint8_t MAX_ATTEMPTS = 4;
// Ask for a full status again if the AC has reported nothing for this long.
static constexpr uint32_t STATUS_REFRESH_MS = 5 * 60 * 1000;
// Until the first full status has arrived, ask again this often.
static constexpr uint32_t STATUS_RETRY_MS = 30 * 1000;
// A frame arrives in one burst; after a pause this long a partly received frame is dropped.
static constexpr uint32_t RX_GAP_MS = 100;
static constexpr float MIN_TEMPERATURE_C = 16.0f;
static constexpr float MAX_TEMPERATURE_C = 31.0f;

static constexpr uint8_t FAN_SPEED_COUNT = 7;
static constexpr const char *const FAN_SPEED_NAMES[FAN_SPEED_COUNT] PROGMEM = {
    "Speed 1", "Speed 2", "Speed 3", "Speed 4", "Speed 5", "Speed 6", "Speed 7"};

// Reply to the AC's once-a-minute check, as the stock module sends it. The last body byte is the
// WiFi signal in dBm; a fixed -48 is enough for the AC.
static constexpr uint8_t PRESENCE_REPLY_FRAME[] PROGMEM = {0xA5, 0x01, 0x00, 0x23, 0x00, 0x00, 0x00, 0x12, 0x47,
                                                           0x09, 0x80, 0x0C, 0x02, 0x64, 0xFF, 0xFF, 0xFF, 0xD0};

// Climate mode for each value of ATTR_MODE
static constexpr uint8_t CLIMATE_MODES[] PROGMEM = {
    climate::CLIMATE_MODE_HEAT_COOL, climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_DRY,
    climate::CLIMATE_MODE_FAN_ONLY,  climate::CLIMATE_MODE_HEAT,
};

void PioneerWT::setup() {
  this->set_supported_custom_fan_modes_static(FAN_SPEED_NAMES, FAN_SPEED_COUNT);
  this->request_status_();
#ifdef USE_SWITCH
  if (this->display_switch_ != nullptr)
    this->display_switch_->restore();
  if (this->beeper_switch_ != nullptr)
    this->beeper_switch_->restore();
#endif
}

climate::ClimateTraits PioneerWT::traits() {
  auto traits = climate::ClimateTraits();
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT_COOL, climate::CLIMATE_MODE_COOL,
                              climate::CLIMATE_MODE_HEAT, climate::CLIMATE_MODE_DRY, climate::CLIMATE_MODE_FAN_ONLY});
  traits.set_supported_fan_modes({climate::CLIMATE_FAN_AUTO});
  traits.set_supported_swing_modes({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL});
  traits.set_supported_presets({climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_ECO});
  traits.set_visual_min_temperature(MIN_TEMPERATURE_C);
  traits.set_visual_max_temperature(MAX_TEMPERATURE_C);
  // The AC keeps its setpoint in whole degrees Fahrenheit, so half degrees Celsius would not all be kept.
  traits.set_visual_target_temperature_step(1.0f);
  traits.set_visual_current_temperature_step(0.1f);
  return traits;
}

void PioneerWT::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (const size_t count = this->available()) {
    this->last_rx_ = now;
    this->read_available(
        [this](uint8_t byte) {
          if (const size_t len = this->reader_.feed(byte))
            this->handle_frame_(this->reader_.frame(), len);
        },
        count);
    if (this->reader_.take_checksum_error()) {
      ESP_LOGD(TAG, "Dropped a frame with a wrong checksum");
    }
  } else if (this->reader_.partial() && now - this->last_rx_ >= RX_GAP_MS) {
    // Only on an idle pass: after a slow loop the rest of the frame may already be buffered.
    this->reader_.reset();
  }
  if (!this->queue_.empty() && now - this->last_send_ >= RESEND_INTERVAL_MS)
    this->send_pending_();
  // The target temperature only arrives in a full status, so it tells whether one has been received.
  const uint32_t status_interval = std::isnan(this->target_temperature) ? STATUS_RETRY_MS : STATUS_REFRESH_MS;
  if (now - this->last_status_ >= status_interval)
    this->request_status_();
}

void PioneerWT::handle_frame_(const uint8_t *frame, size_t len) {
  const uint8_t flags = frame[2];
  const uint8_t type = frame[3];
  const uint8_t seq = frame[4];
  const uint8_t ack = frame[5];
  const uint8_t *payload = frame + HEADER_SIZE;
  const size_t payload_len = len - HEADER_SIZE;
  ESP_LOGVV_HEX(TAG, frame, len, MAX_FRAME_SIZE, "RX %s");

  if (type == TYPE_REPLY) {
    if (!this->queue_.empty() && this->queue_.front().seq == ack) {
      // Pop first: the warning log can run automations that queue another command.
      this->pop_command_();
      this->status_clear_warning();
      this->last_send_ = 0;  // send the next one right away
    }
    return;
  }
  if (type != TYPE_DATA || payload_len < 2)
    return;

  const uint8_t msg_class = payload[0];
  if (flags & FLAG_WANT_ACK)
    this->write_frame_(FLAG_WANT_ACK, TYPE_REPLY, 0, seq, CLASS_REPLY, msg_class, nullptr, 0);

  if (msg_class != CLASS_STATUS)
    return;
  if (payload_len == 2) {
    if (!(flags & FLAG_WANT_ACK))
      this->write_array_progmem(PRESENCE_REPLY_FRAME, sizeof(PRESENCE_REPLY_FRAME));
    return;
  }
  this->handle_status_(payload + 2, payload_len - 2);
  // Only a full status counts, so the boot request is repeated until one arrives.
  if (!std::isnan(this->target_temperature))
    this->last_status_ = App.get_loop_component_start_time();
}

void PioneerWT::handle_status_(const uint8_t *body, size_t len) {
  AttributeReader attrs(body, len);
  bool changed = false;
  while (attrs.next()) {
    const uint32_t v = attrs.value();
    switch (attrs.id()) {
      case ATTR_POWER:
        this->power_ = v != 0;
        // A queued command may still change the power; keep that intent until the queue drains.
        if (this->queue_.empty())
          this->power_wanted_ = this->power_;
        break;
      case ATTR_MODE:
        if (v < std::size(CLIMATE_MODES)) {
          this->ac_mode_ = v;
        } else {
          ESP_LOGD(TAG, "Unknown mode %" PRIu32, v);
        }
        break;
      case ATTR_SETPOINT_F: {
        const float target = fahrenheit_to_celsius(v);
        changed |= target != this->target_temperature;
        this->target_temperature = target;
        break;
      }
      case ATTR_INDOOR_TEMP_C100: {
        const float current = static_cast<float>(v) / 100.0f;
        changed |= current != this->current_temperature;
        this->current_temperature = current;
        break;
      }
      case ATTR_FAN_SPEED:
        if (v == 0) {
          changed |= this->set_fan_mode_(climate::CLIMATE_FAN_AUTO);
        } else if (v <= FAN_SPEED_COUNT) {
          changed |= this->set_custom_fan_mode_(FAN_SPEED_NAMES[v - 1]);
        } else {
          ESP_LOGD(TAG, "Unknown fan speed %" PRIu32, v);
        }
        break;
      case ATTR_SWING_VERTICAL: {
        const auto swing = v == SWING_ON ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
        changed |= swing != this->swing_mode;
        this->swing_mode = swing;
        break;
      }
      case ATTR_ECO:
        changed |= this->set_preset_(v ? climate::CLIMATE_PRESET_ECO : climate::CLIMATE_PRESET_NONE);
        break;
#ifdef USE_SWITCH
      case ATTR_DISPLAY:
        PUBLISH_STATE(this->display_switch_, v != 0);
        break;
      case ATTR_BEEPER:
        PUBLISH_STATE(this->beeper_switch_, v != 0);
        break;
#endif
      default:
        break;
    }
  }
  if (attrs.stopped_on_unknown()) {
    ESP_LOGD(TAG, "Status attribute %04X has an unknown size; ignored the rest of the frame", attrs.id());
  }

  const auto mode = this->power_ ? static_cast<climate::ClimateMode>(progmem_read_byte(&CLIMATE_MODES[this->ac_mode_]))
                                 : climate::CLIMATE_MODE_OFF;
  changed |= mode != this->mode;
  this->mode = mode;
  if (changed)
    this->publish_state();
}

void PioneerWT::control(const climate::ClimateCall &call) {
  uint8_t body[MAX_BODY_SIZE];
  uint8_t *p = body;

  if (call.get_mode().has_value()) {
    const auto mode = *call.get_mode();
    if (mode == climate::CLIMATE_MODE_OFF) {
      p = put_attr(p, ATTR_POWER, 0);
      this->power_wanted_ = false;
    } else {
      uint8_t ac_mode = 0;
      for (size_t i = 0; i < std::size(CLIMATE_MODES); i++) {
        if (progmem_read_byte(&CLIMATE_MODES[i]) == mode) {
          ac_mode = i;
          break;
        }
      }
      // An "off" that is still waiting to be sent counts, so the AC is turned back on after it.
      if (!this->power_wanted_)
        p = put_attr(p, ATTR_POWER, 1);
      this->power_wanted_ = true;
      p = put_attr(p, ATTR_MODE, ac_mode);
    }
  }

  if (call.get_target_temperature().has_value()) {
    // Automations can ask for any value; the visual range is not enforced for them.
    const float celsius = clamp(*call.get_target_temperature(), MIN_TEMPERATURE_C, MAX_TEMPERATURE_C);
    p = put_be32(put_be16(p, ATTR_SETPOINT_F), lroundf(celsius_to_fahrenheit(celsius)));
  }

  // The app always sends fan speed with these companions; match it exactly.
  int fan_speed = -1;
  if (call.get_fan_mode().has_value() && *call.get_fan_mode() == climate::CLIMATE_FAN_AUTO) {
    fan_speed = 0;
  } else if (call.has_custom_fan_mode()) {
    // The call holds the pointer from FAN_SPEED_NAMES, so the pointers can be compared.
    const char *name = call.get_custom_fan_mode().c_str();
    for (uint8_t i = 0; i < FAN_SPEED_COUNT; i++) {
      if (name == FAN_SPEED_NAMES[i]) {
        fan_speed = i + 1;
        break;
      }
    }
  }
  if (fan_speed >= 0) {
    p = put_attr(p, ATTR_FAN_EXTRA_1, 0);
    p = put_attr(p, ATTR_FAN_EXTRA_2, 0);
    p = put_attr(p, ATTR_FAN_SPEED, fan_speed);
    p = put_attr(p, ATTR_FAN_AUTO, fan_speed == 0);
  }

  if (call.get_swing_mode().has_value()) {
    const bool swing = *call.get_swing_mode() == climate::CLIMATE_SWING_VERTICAL;
    p = put_attr(p, ATTR_SWING_VERTICAL, swing ? SWING_ON : SWING_OFF);
  }

  if (call.get_preset().has_value())
    p = put_attr(p, ATTR_ECO, *call.get_preset() == climate::CLIMATE_PRESET_ECO);

  if (p != body)
    this->queue_command_(CLASS_COMMAND, body, p - body);
  // State is published when the AC reports the new values back.
}

void PioneerWT::send_setting_(uint16_t attr, bool on) {
  uint8_t body[3];
  put_attr(body, attr, on);
  this->queue_command_(CLASS_COMMAND, body, sizeof(body));
}

void PioneerWT::request_status_() {
  // Asks the AC to send every setting, as the stock module does at boot.
  static constexpr uint8_t REQUEST_ALL[] = {0xFF, 0xFF};
  this->queue_command_(CLASS_NETWORK, REQUEST_ALL, sizeof(REQUEST_ALL));
  this->last_status_ = App.get_loop_component_start_time();
}

void PioneerWT::queue_command_(uint8_t msg_class, const uint8_t *body, size_t len) {
  if (this->queue_.size() == TX_QUEUE_SIZE) {
    ESP_LOGW(TAG, "Command queue full; dropping the oldest command");
    this->drop_command_();
  }
  PendingCommand pending{msg_class, this->next_seq_++, static_cast<uint8_t>(len), {}};
  std::memcpy(pending.body.data(), body, len);
  this->queue_.push(pending);
  if (this->next_seq_ == 0)
    this->next_seq_ = 1;
}

void PioneerWT::pop_command_() {
  this->queue_.pop();
  this->attempts_ = 0;
}

void PioneerWT::drop_command_() {
  this->pop_command_();
  // A dropped command may have carried a power change; go back to what the AC last reported.
  this->power_wanted_ = this->power_;
}

void PioneerWT::send_pending_() {
  const PendingCommand &pending = this->queue_.front();
  if (this->attempts_ >= MAX_ATTEMPTS) {
    const uint8_t seq = pending.seq;
    // Drop first: the warning log can run automations that queue another command.
    this->drop_command_();
    ESP_LOGD(TAG, "No acknowledgement for command seq %02X; giving up", seq);
    this->status_set_warning(LOG_STR("No acknowledgement"));
    return;
  }
  this->attempts_++;
  this->write_frame_(FLAG_WANT_ACK, TYPE_DATA, pending.seq, 0, pending.msg_class, pending.msg_class,
                     pending.body.data(), pending.len);
  this->last_send_ = App.get_loop_component_start_time();
}

void PioneerWT::write_frame_(uint8_t flags, uint8_t type, uint8_t seq, uint8_t ack, uint8_t class_1, uint8_t class_2,
                             const uint8_t *body, size_t len) {
  uint8_t frame[HEADER_SIZE + 2 + MAX_BODY_SIZE];
  const size_t total = build_frame(frame, flags, type, seq, ack, class_1, class_2, body, len);
  ESP_LOGVV_HEX(TAG, frame, total, sizeof(frame), "TX %s");
  this->write_array(frame, total);
}

void PioneerWT::dump_config() {
  LOG_CLIMATE("", "Pioneer WT", this);
#ifdef USE_SWITCH
  LOG_SWITCH("  ", "Display", this->display_switch_);
  LOG_SWITCH("  ", "Beeper", this->beeper_switch_);
#endif
}

}  // namespace esphome::pioneer_wt
