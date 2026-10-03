#include "ble_midi.h"

#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome::ble_midi {

static const char *const TAG = "ble_midi";

// Standard BLE-MIDI service and characteristic, as defined by the BLE-MIDI
// specification.
static const char *const SERVICE_UUID = "03B80E5A-EDE8-4B33-A751-6CE34EC4C700";
static const char *const CHARACTERISTIC_UUID = "7772E5DB-3868-4112-A1A9-F2669D106BF3";

// A BLE-MIDI packet starts with a header byte and a timestamp byte, both of
// which have bit 7 set. The remaining timestamp bits are used for jitter
// reconstruction only and are left at zero when transmitting.
static const uint8_t PACKET_HEADER = 0x80;
static const uint8_t PACKET_TIMESTAMP = 0x80;

// MIDI status bytes for the messages this component can transmit.
static const uint8_t STATUS_NOTE_OFF = 0x80;
static const uint8_t STATUS_NOTE_ON = 0x90;
static const uint8_t STATUS_CONTROL_CHANGE = 0xB0;
static const uint8_t STATUS_PROGRAM_CHANGE = 0xC0;
static const uint8_t STATUS_PITCH_BEND = 0xE0;
static const uint8_t STATUS_SYSEX_START = 0xF0;
static const uint8_t STATUS_SYSEX_END = 0xF7;

// Number of received bytes included in verbose packet logs.
static const size_t LOG_MAX_BYTES = 32;

template<typename... Ts> static void fire_triggers(const std::vector<Trigger<Ts...> *> &triggers, const Ts &...args) {
  for (auto *trigger : triggers)
    trigger->trigger(args...);
}

void BLEMidi::setup() { this->parser_.init_sysex_buffer(this->max_sysex_size_); }

void BLEMidi::loop() {
  // Messages arrive through gattc_event_handler() and are dispatched from
  // there, so this component does not need to run on every loop iteration.
  this->disable_loop();
}

void BLEMidi::dump_config() {
  ESP_LOGCONFIG(TAG,
                "BLE MIDI:\n"
                "  MAC address: %s\n"
                "  Maximum SysEx size: %u\n"
                "  Pair on connect: %s",
                this->parent()->address_str(), static_cast<unsigned>(this->max_sysex_size_), YESNO(this->pair_));
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
      // Both events can arrive for the same disconnect, so the triggers are
      // fired only for the one that follows an established MIDI connection.
      bool was_ready = this->notify_registered_;
      this->notify_registered_ = false;
      this->node_state = espbt::ClientState::IDLE;
      if (was_ready) {
        for (auto *trigger : this->disconnect_triggers_)
          trigger->trigger();
      }
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *characteristic = this->parent()->get_characteristic(espbt::ESPBTUUID::from_raw(SERVICE_UUID),
                                                                espbt::ESPBTUUID::from_raw(CHARACTERISTIC_UUID));
      if (characteristic == nullptr) {
        ESP_LOGW(TAG, "No BLE MIDI characteristic found on this device");
        break;
      }
      this->characteristic_handle_ = characteristic->handle;
      this->write_without_response_ = (characteristic->properties & ESP_GATT_CHAR_PROP_BIT_WRITE_NR) != 0;
      esp_err_t err = this->parent()->register_for_notify(characteristic->handle);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "register_for_notify failed, error=%d", err);
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->reg_for_notify.handle != this->characteristic_handle_)
        break;
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Enabling notifications failed, status=%d", param->reg_for_notify.status);
        break;
      }
      this->notify_registered_ = true;
      this->node_state = espbt::ClientState::ESTABLISHED;
      ESP_LOGI(TAG, "MIDI notifications enabled");
      // The device can only be sent to once notifications are enabled, so this
      // is the point where on_connect is useful.
      for (auto *trigger : this->connect_triggers_)
        trigger->trigger();
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

  if (!this->message_triggers_.empty()) {
    std::vector<uint8_t> bytes(message.data, message.data + message.length);
    fire_triggers(this->message_triggers_, bytes);
  }

  switch (message.type) {
    case MessageType::NOTE_ON:
      fire_triggers(this->note_on_triggers_, message.note(), message.velocity(), message.channel());
      break;
    case MessageType::NOTE_OFF:
      fire_triggers(this->note_off_triggers_, message.note(), message.velocity(), message.channel());
      break;
    case MessageType::CONTROL_CHANGE:
      fire_triggers(this->control_change_triggers_, message.control_number(), message.control_value(),
                    message.channel());
      break;
    case MessageType::PROGRAM_CHANGE:
      fire_triggers(this->program_change_triggers_, message.program(), message.channel());
      break;
    case MessageType::PITCH_BEND:
      fire_triggers(this->pitch_bend_triggers_, message.pitch_bend(), message.channel());
      break;
    case MessageType::SYSEX: {
      if (this->sysex_triggers_.empty())
        break;
      // Hand the payload to the automation without the 0xF0/0xF7 wrappers.
      std::vector<uint8_t> payload;
      if (message.length > 2)
        payload.assign(message.data + 1, message.data + message.length - 1);
      fire_triggers(this->sysex_triggers_, payload);
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
  if (length == 0)
    return false;
  if (!this->is_ready() || this->characteristic_handle_ == 0) {
    ESP_LOGW(TAG, "Cannot send: not connected");
    return false;
  }
  if (length > MAX_TX_MIDI_BYTES) {
    ESP_LOGW(TAG, "Cannot send: message of %u bytes exceeds the %u byte limit", static_cast<unsigned>(length),
             static_cast<unsigned>(MAX_TX_MIDI_BYTES));
    return false;
  }

  uint8_t packet[MAX_TX_MIDI_BYTES + 2];
  packet[0] = PACKET_HEADER;
  packet[1] = PACKET_TIMESTAMP;
  std::memcpy(packet + 2, midi_bytes, length);
  size_t packet_length = length + 2;

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

void BLEMidi::send_sysex(const std::vector<uint8_t> &payload) {
  if (payload.empty())
    return;
  bool has_start = payload.front() == STATUS_SYSEX_START;
  bool has_end = payload.back() == STATUS_SYSEX_END;
  size_t length = payload.size() + (has_start ? 0 : 1) + (has_end ? 0 : 1);
  if (length > MAX_TX_MIDI_BYTES) {
    ESP_LOGW(TAG, "Cannot send: SysEx of %u bytes exceeds the %u byte limit", static_cast<unsigned>(length),
             static_cast<unsigned>(MAX_TX_MIDI_BYTES));
    return;
  }

  uint8_t bytes[MAX_TX_MIDI_BYTES];
  size_t offset = 0;
  if (!has_start)
    bytes[offset++] = STATUS_SYSEX_START;
  std::memcpy(bytes + offset, payload.data(), payload.size());
  offset += payload.size();
  if (!has_end)
    bytes[offset++] = STATUS_SYSEX_END;
  this->write_midi_(bytes, offset);
}

void BLEMidi::send_raw(const std::vector<uint8_t> &bytes) { this->write_midi_(bytes.data(), bytes.size()); }

}  // namespace esphome::ble_midi

#endif  // USE_ESP32
