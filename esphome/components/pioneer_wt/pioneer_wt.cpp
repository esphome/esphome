#include "pioneer_wt.h"

#include <cinttypes>
#include <cmath>
#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
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

static const char *const FAN_SPEED_NAMES[FAN_SPEED_COUNT] = {"Speed 1", "Speed 2", "Speed 3", "Speed 4",
                                                             "Speed 5", "Speed 6", "Speed 7"};

// Reply to the AC's once-a-minute check, as the stock module sends it. The last byte is the
// WiFi signal in dBm; a fixed -48 is enough for the AC.
static const uint8_t PRESENCE_REPLY[] PROGMEM = {0x02, 0x64, 0xFF, 0xFF, 0xFF, 0xD0};

// Climate mode for each value of ATTR_MODE
static const uint8_t CLIMATE_MODES[MODE_COUNT] PROGMEM = {
    climate::CLIMATE_MODE_HEAT_COOL, climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_DRY,
    climate::CLIMATE_MODE_FAN_ONLY,  climate::CLIMATE_MODE_HEAT,
};

void PioneerWT::setup() {
  this->set_supported_custom_fan_modes_static(FAN_SPEED_NAMES, FAN_SPEED_COUNT);
  this->request_status_();
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
  if (this->available()) {
    if (this->reader_.partial() && now - this->last_rx_ >= RX_GAP_MS)
      this->reader_.reset();
    this->last_rx_ = now;
    this->read_available([this](uint8_t byte) {
      const size_t len = this->reader_.feed(byte);
      if (len)
        this->handle_frame_(this->reader_.frame(), len);
    });
    if (this->reader_.take_checksum_error()) {
      ESP_LOGD(TAG, "Dropped a frame with a wrong checksum");
    }
  }
  if (this->queue_count_ && now - this->last_send_ >= RESEND_INTERVAL_MS)
    this->send_pending_();
  // The target temperature only arrives in a full status, so it tells whether one has been received.
  const bool need_full_status = std::isnan(this->target_temperature);
  if (need_full_status ? now - this->last_request_ >= STATUS_RETRY_MS : now - this->last_status_ >= STATUS_REFRESH_MS) {
    this->request_status_();
  }
}

void PioneerWT::handle_frame_(const uint8_t *frame, size_t len) {
  const uint8_t flags = frame[2];
  const uint8_t type = frame[3];
  const uint8_t seq = frame[4];
  const uint8_t ack = frame[5];
  const uint8_t *payload = frame + HEADER_SIZE;
  const size_t payload_len = len - HEADER_SIZE;
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
  char hex[format_hex_pretty_size(MAX_FRAME_SIZE)];
  ESP_LOGVV(TAG, "RX %s", format_hex_pretty_to(hex, frame, len, ' '));
#endif

  if (type == TYPE_REPLY) {
    if (this->queue_count_ && this->queue_[0].seq == ack) {
      this->status_clear_warning();
      for (uint8_t i = 1; i < this->queue_count_; i++)
        this->queue_[i - 1] = this->queue_[i];
      this->queue_count_--;
      this->last_send_ = 0;  // send the next one right away
    }
    return;
  }
  if (type != TYPE_DATA || payload_len < 2)
    return;

  const uint8_t msg_class = payload[0];
  if (flags & FLAG_WANT_ACK)
    this->send_reply_(FLAG_WANT_ACK, seq, msg_class, nullptr, 0);

  if (msg_class == CLASS_STATUS) {
    if (payload_len == 2) {
      if (!(flags & FLAG_WANT_ACK)) {
        uint8_t reply[sizeof(PRESENCE_REPLY)];
        progmem_memcpy(reply, PRESENCE_REPLY, sizeof(reply));
        this->send_reply_(FLAG_NO_ACK, 0, CLASS_STATUS, reply, sizeof(reply));
      }
      return;
    }
    // The presence check above must not count: it arrives even when no status ever does.
    this->last_status_ = App.get_loop_component_start_time();
    this->handle_status_(payload + 2, payload_len - 2);
  }
}

void PioneerWT::handle_status_(const uint8_t *body, size_t len) {
  AttributeReader attrs(body, len);
  bool changed = false;
  while (attrs.next()) {
    const uint32_t v = attrs.value();
    switch (attrs.id()) {
      case ATTR_POWER:
        this->power_ = this->power_wanted_ = v != 0;
        changed = true;
        break;
      case ATTR_MODE:
        this->ac_mode_ = v;
        changed = true;
        break;
      case ATTR_SETPOINT_F:
        this->target_temperature = (static_cast<float>(v) - 32.0f) * 5.0f / 9.0f;
        changed = true;
        break;
      case ATTR_INDOOR_TEMP_C100:
        this->current_temperature = static_cast<float>(v) / 100.0f;
        changed = true;
        break;
      case ATTR_FAN_SPEED:
        if (v == 0) {
          this->set_fan_mode_(climate::CLIMATE_FAN_AUTO);
        } else if (v <= FAN_SPEED_COUNT) {
          this->set_custom_fan_mode_(FAN_SPEED_NAMES[v - 1]);
        } else {
          ESP_LOGD(TAG, "Unknown fan speed %" PRIu32, v);
        }
        changed = true;
        break;
      case ATTR_SWING_VERTICAL:
        this->swing_mode = v == SWING_ON ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
        changed = true;
        break;
      case ATTR_ECO:
        this->set_preset_(v ? climate::CLIMATE_PRESET_ECO : climate::CLIMATE_PRESET_NONE);
        changed = true;
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
  if (!changed)
    return;

  if (!this->power_) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else if (this->ac_mode_ < MODE_COUNT) {
    this->mode = static_cast<climate::ClimateMode>(progmem_read_byte(&CLIMATE_MODES[this->ac_mode_]));
  } else {
    ESP_LOGD(TAG, "Unknown mode %u", this->ac_mode_);
  }
  this->publish_state();
}

void PioneerWT::control(const climate::ClimateCall &call) {
  uint8_t body[MAX_BODY_SIZE];
  size_t len = 0;

  if (call.get_mode().has_value()) {
    const auto mode = *call.get_mode();
    if (mode == climate::CLIMATE_MODE_OFF) {
      len = put_attr(body, len, ATTR_POWER, 0, 1);
      this->power_wanted_ = false;
    } else {
      uint8_t ac_mode = MODE_AUTO;
      for (uint8_t i = 0; i < MODE_COUNT; i++) {
        if (progmem_read_byte(&CLIMATE_MODES[i]) == mode)
          ac_mode = i;
      }
      // An "off" that is still waiting to be sent counts, so the AC is turned back on after it.
      if (!this->power_wanted_)
        len = put_attr(body, len, ATTR_POWER, 1, 1);
      this->power_wanted_ = true;
      len = put_attr(body, len, ATTR_MODE, ac_mode, 1);
    }
  }

  if (call.get_target_temperature().has_value()) {
    // Automations can ask for any value; the visual range is not enforced for them.
    const float celsius = clamp(*call.get_target_temperature(), MIN_TEMPERATURE_C, MAX_TEMPERATURE_C);
    const float fahrenheit = celsius * 9.0f / 5.0f + 32.0f;
    len = put_attr(body, len, ATTR_SETPOINT_F, static_cast<uint32_t>(lroundf(fahrenheit)), 4);
  }

  // The app always sends fan speed with these companions; match it exactly.
  int fan_speed = -1;
  if (call.get_fan_mode().has_value() && *call.get_fan_mode() == climate::CLIMATE_FAN_AUTO) {
    fan_speed = 0;
  } else if (call.has_custom_fan_mode()) {
    const StringRef name = call.get_custom_fan_mode();
    for (uint8_t i = 0; i < FAN_SPEED_COUNT; i++) {
      if (name == FAN_SPEED_NAMES[i])
        fan_speed = i + 1;
    }
  }
  if (fan_speed >= 0) {
    len = put_attr(body, len, ATTR_FAN_EXTRA_1, 0, 1);
    len = put_attr(body, len, ATTR_FAN_EXTRA_2, 0, 1);
    len = put_attr(body, len, ATTR_FAN_SPEED, fan_speed, 1);
    len = put_attr(body, len, ATTR_FAN_AUTO, fan_speed == 0 ? 1 : 0, 1);
  }

  if (call.get_swing_mode().has_value()) {
    const bool swing = *call.get_swing_mode() == climate::CLIMATE_SWING_VERTICAL;
    len = put_attr(body, len, ATTR_SWING_VERTICAL, swing ? SWING_ON : SWING_OFF, 1);
  }

  if (call.get_preset().has_value())
    len = put_attr(body, len, ATTR_ECO, *call.get_preset() == climate::CLIMATE_PRESET_ECO ? 1 : 0, 1);

  if (len)
    this->queue_command_(CLASS_COMMAND, body, len);
  // State is published when the AC reports the new values back.
}

void PioneerWT::send_setting_(uint16_t attr, bool on) {
  uint8_t body[3];
  this->queue_command_(CLASS_COMMAND, body, put_attr(body, 0, attr, on ? 1 : 0, 1));
}

void PioneerWT::request_status_() {
  // Asks the AC to send every setting, as the stock module does at boot.
  const uint8_t request_all[] = {0xFF, 0xFF};
  this->queue_command_(CLASS_NETWORK, request_all, sizeof(request_all));
  this->last_request_ = this->last_status_ = App.get_loop_component_start_time();
}

void PioneerWT::queue_command_(uint8_t msg_class, const uint8_t *body, size_t body_len) {
  if (body_len > MAX_BODY_SIZE)
    return;
  if (this->queue_count_ == TX_QUEUE_SIZE) {
    ESP_LOGW(TAG, "Command queue full; dropping the oldest command");
    for (uint8_t i = 1; i < TX_QUEUE_SIZE; i++)
      this->queue_[i - 1] = this->queue_[i];
    this->queue_count_--;
  }
  PendingFrame &pending = this->queue_[this->queue_count_++];
  pending.payload[0] = msg_class;
  pending.payload[1] = msg_class;
  std::memcpy(pending.payload.data() + 2, body, body_len);
  pending.len = body_len + 2;
  pending.attempts = 0;
  pending.seq = this->next_seq_++;
  if (this->next_seq_ == 0)
    this->next_seq_ = 1;
}

void PioneerWT::send_pending_() {
  PendingFrame &pending = this->queue_[0];
  if (pending.attempts >= MAX_ATTEMPTS) {
    ESP_LOGW(TAG, "No acknowledgement for command seq %02X; giving up", pending.seq);
    this->status_set_warning();
    for (uint8_t i = 1; i < this->queue_count_; i++)
      this->queue_[i - 1] = this->queue_[i];
    this->queue_count_--;
    return;
  }
  pending.attempts++;
  this->write_frame_(FLAG_WANT_ACK, TYPE_DATA, pending.seq, 0, pending.payload.data(), pending.len);
  this->last_send_ = App.get_loop_component_start_time();
}

void PioneerWT::send_reply_(uint8_t flags, uint8_t ack_seq, uint8_t msg_class, const uint8_t *extra, size_t extra_len) {
  uint8_t payload[2 + sizeof(PRESENCE_REPLY)];
  if (extra_len > sizeof(PRESENCE_REPLY))
    return;
  payload[0] = 0x80;
  payload[1] = msg_class;
  if (extra_len)
    std::memcpy(payload + 2, extra, extra_len);
  this->write_frame_(flags, TYPE_REPLY, 0, ack_seq, payload, 2 + extra_len);
}

void PioneerWT::write_frame_(uint8_t flags, uint8_t type, uint8_t seq, uint8_t ack, const uint8_t *payload,
                             size_t len) {
  uint8_t frame[HEADER_SIZE + MAX_BODY_SIZE + 2];
  const size_t total = build_frame(frame, sizeof(frame), flags, type, seq, ack, payload, len);
  if (total == 0)
    return;
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
  char hex[format_hex_pretty_size(sizeof(frame))];
  ESP_LOGVV(TAG, "TX %s", format_hex_pretty_to(hex, frame, total, ' '));
#endif
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
