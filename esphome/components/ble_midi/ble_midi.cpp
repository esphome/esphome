#include "ble_midi.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include <algorithm>

namespace esphome::ble_midi {

ESPHOME_LOG_TAG(TAG, "ble_midi");

// Standard BLE-MIDI service and characteristic, as defined by the BLE-MIDI
// specification.
static constexpr const char *SERVICE_UUID = "03B80E5A-EDE8-4B33-A751-6CE34EC4C700";
static constexpr const char *CHARACTERISTIC_UUID = "7772E5DB-3868-4112-A1A9-F2669D106BF3";

// MIDI status bytes for the messages this component can transmit.
static constexpr uint8_t STATUS_NOTE_OFF = 0x80;
static constexpr uint8_t STATUS_NOTE_ON = 0x90;
static constexpr uint8_t STATUS_CONTROL_CHANGE = 0xB0;
static constexpr uint8_t STATUS_PROGRAM_CHANGE = 0xC0;
static constexpr uint8_t STATUS_PITCH_BEND = 0xE0;

// Number of received bytes included in verbose packet logs.
static constexpr size_t LOG_MAX_BYTES = 32;

void BLEMidi::dump_config() {
  ESP_LOGCONFIG(TAG,
                "BLE MIDI:\n"
                "  MAC address: %s\n"
                "  Maximum SysEx size: %u\n"
                "  Pair on connect: %s",
                this->parent()->address_str(), this->parser_.sysex_capacity(), YESNO(this->pair_));
}

void BLEMidi::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status != ESP_GATT_OK)
        break;
      this->parser_.reset();
      if (this->pair_) {
        esp_err_t err = this->parent()->pair();
        if (err != ESP_OK) {
          ESP_LOGW(TAG, "Pairing failed, error=%d", err);
        }
      }
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT:
    case ESP_GATTC_CLOSE_EVT: {
      this->parser_.reset();
      this->characteristic_handle_ = 0;
      // Both events can arrive for the same disconnect, so the automation runs
      // only for the one that follows an established MIDI connection.
      const bool was_ready = this->notify_registered_;
      this->notify_registered_ = false;
      this->node_state = espbt::ClientState::IDLE;
      if (was_ready)
        this->disconnect_callback_.call();
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *characteristic = this->parent()->get_characteristic(espbt::ESPBTUUID::from_raw(SERVICE_UUID),
                                                                espbt::ESPBTUUID::from_raw(CHARACTERISTIC_UUID));
      if (characteristic == nullptr) {
        this->status_set_warning(LOG_STR("No BLE MIDI characteristic found on this device"));
        break;
      }
      this->characteristic_handle_ = characteristic->handle;
      this->write_without_response_ = (characteristic->properties & ESP_GATT_CHAR_PROP_BIT_WRITE_NR) != 0;
      esp_err_t err = this->parent()->register_for_notify(characteristic->handle);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "register_for_notify failed, error=%d", err);
        this->status_set_warning();
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->reg_for_notify.handle != this->characteristic_handle_)
        break;
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Enabling notifications failed, status=%d", param->reg_for_notify.status);
        this->status_set_warning();
        break;
      }
      this->status_clear_warning();
      this->notify_registered_ = true;
      this->node_state = espbt::ClientState::ESTABLISHED;
      ESP_LOGI(TAG, "MIDI notifications enabled");
      // The device can only be sent to once notifications are enabled, so this
      // is the point where on_connect is useful.
      this->connect_callback_.call();
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->characteristic_handle_ || param->notify.value_len == 0)
        break;
      // format_hex_to() truncates to the buffer size, so long packets are logged
      // only in part.
      char hex[format_hex_size(LOG_MAX_BYTES)];
      ESP_LOGV(TAG, "Received %u bytes: %s", param->notify.value_len,
               format_hex_to(hex, param->notify.value, param->notify.value_len));
      this->parser_.feed(param->notify.value, param->notify.value_len,
                         [this](const MidiMessage &message) { this->handle_message_(message); });
      break;
    }
    default:
      break;
  }
}

void BLEMidi::handle_message_(const MidiMessage &message) {
  ESP_LOGV(TAG, "Decoded %s", message_type_name(message.type));
  this->message_callback_.call(ByteSpan(message.data, message.length));

  switch (message.type) {
    case MessageType::NOTE_ON:
      this->note_on_callback_.call(message.note(), message.velocity(), message.channel());
      break;
    case MessageType::NOTE_OFF:
      this->note_off_callback_.call(message.note(), message.velocity(), message.channel());
      break;
    case MessageType::CONTROL_CHANGE:
      this->control_change_callback_.call(message.control_number(), message.control_value(), message.channel());
      break;
    case MessageType::PROGRAM_CHANGE:
      this->program_change_callback_.call(message.program(), message.channel());
      break;
    case MessageType::PITCH_BEND:
      this->pitch_bend_callback_.call(message.pitch_bend(), message.channel());
      break;
    case MessageType::SYSEX: {
      if (message.truncated) {
        ESP_LOGW(TAG, "SysEx longer than %u bytes was cut; raise max_sysex_size", this->parser_.sysex_capacity());
      }
      // Hand the payload to the automation without the wrappers; a truncated or
      // cut-short message has no 0xF7, so only the bytes present are dropped
      ByteSpan payload(message.data, message.length);
      if (!payload.empty() && payload.front() == STATUS_SYSEX_START)
        payload = payload.subspan(1);
      if (!payload.empty() && payload.back() == STATUS_SYSEX_END)
        payload = payload.first(payload.size() - 1);
      this->sysex_callback_.call(payload);
      break;
    }
    case MessageType::UNKNOWN:
      ESP_LOGW(TAG, "Undecodable MIDI byte 0x%02X", message.status());
      break;
    default:
      // Polyphonic and channel aftertouch, system common and real-time messages
      // are only reported through on_message.
      break;
  }
}

bool BLEMidi::write_midi_(const uint8_t *midi_bytes, size_t length) {
  if (length == 0) {
    ESP_LOGW(TAG, "Cannot send: empty message");
    return false;
  }
  if (!this->is_ready() || this->characteristic_handle_ == 0) {
    ESP_LOGW(TAG, "Cannot send: not connected");
    return false;
  }
  uint8_t packet[MAX_TX_MIDI_BYTES + 2];
  // A write without response has to fit one ATT packet, which the stack does not check for us
  const size_t limit =
      this->write_without_response_ ? std::min<size_t>(sizeof(packet), this->parent()->get_mtu() - 3) : sizeof(packet);
  const size_t packet_length = length + 2;
  if (packet_length > limit) {
    ESP_LOGW(TAG, "Cannot send: message of %u bytes exceeds the %u byte limit", static_cast<unsigned>(length),
             static_cast<unsigned>(limit - 2));
    return false;
  }

  packet[0] = PACKET_HEADER;
  packet[1] = PACKET_TIMESTAMP;
  std::memcpy(packet + 2, midi_bytes, length);

  esp_err_t err = esp_ble_gattc_write_char(
      this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->characteristic_handle_, packet_length,
      packet, this->write_without_response_ ? ESP_GATT_WRITE_TYPE_NO_RSP : ESP_GATT_WRITE_TYPE_RSP,
      ESP_GATT_AUTH_REQ_NONE);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Write failed, error=%d", err);
    return false;
  }

  char hex[format_hex_size(LOG_MAX_BYTES)];
  ESP_LOGV(TAG, "Sent %u bytes: %s", static_cast<unsigned>(packet_length), format_hex_to(hex, packet, packet_length));
  return true;
}

void BLEMidi::send_note_on(uint8_t note, uint8_t velocity, uint8_t channel) {
  const uint8_t bytes[3] = {static_cast<uint8_t>(STATUS_NOTE_ON | (channel & 0x0F)), static_cast<uint8_t>(note & 0x7F),
                            static_cast<uint8_t>(velocity & 0x7F)};
  this->write_midi_(bytes, sizeof(bytes));
}

void BLEMidi::send_note_off(uint8_t note, uint8_t velocity, uint8_t channel) {
  const uint8_t bytes[3] = {static_cast<uint8_t>(STATUS_NOTE_OFF | (channel & 0x0F)), static_cast<uint8_t>(note & 0x7F),
                            static_cast<uint8_t>(velocity & 0x7F)};
  this->write_midi_(bytes, sizeof(bytes));
}

void BLEMidi::send_control_change(uint8_t control_number, uint8_t value, uint8_t channel) {
  const uint8_t bytes[3] = {static_cast<uint8_t>(STATUS_CONTROL_CHANGE | (channel & 0x0F)),
                            static_cast<uint8_t>(control_number & 0x7F), static_cast<uint8_t>(value & 0x7F)};
  this->write_midi_(bytes, sizeof(bytes));
}

void BLEMidi::send_program_change(uint8_t program, uint8_t channel) {
  const uint8_t bytes[2] = {static_cast<uint8_t>(STATUS_PROGRAM_CHANGE | (channel & 0x0F)),
                            static_cast<uint8_t>(program & 0x7F)};
  this->write_midi_(bytes, sizeof(bytes));
}

void BLEMidi::send_pitch_bend(int16_t value, uint8_t channel) {
  uint16_t unsigned_value = static_cast<uint16_t>(clamp<int32_t>(value, -8192, 8191) + 8192);
  const uint8_t bytes[3] = {static_cast<uint8_t>(STATUS_PITCH_BEND | (channel & 0x0F)),
                            static_cast<uint8_t>(unsigned_value & 0x7F),
                            static_cast<uint8_t>((unsigned_value >> 7) & 0x7F)};
  this->write_midi_(bytes, sizeof(bytes));
}

void BLEMidi::send_sysex(const uint8_t *payload, size_t length) {
  uint8_t bytes[MAX_TX_MIDI_BYTES];
  const size_t framed = frame_sysex(payload, length, bytes, sizeof(bytes));
  if (framed == 0) {
    ESP_LOGW(TAG, "Cannot send: SysEx of %u bytes is empty or exceeds the %u byte limit", static_cast<unsigned>(length),
             static_cast<unsigned>(MAX_TX_MIDI_BYTES));
    return;
  }
  this->write_midi_(bytes, framed);
}

void BLEMidi::send_raw(const uint8_t *bytes, size_t length) { this->write_midi_(bytes, length); }

}  // namespace esphome::ble_midi

#endif  // USE_ESP32
