#include "ble_midi.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

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
      // A warning left by an earlier session no longer applies
      this->status_clear_warning();
      this->parser_.reset();
      if (this->pair_) {
        esp_err_t err = this->parent()->pair();
        if (err != ESP_OK) {
          ESP_LOGW(TAG, "Pairing failed, error=%d", err);
        }
      }
      break;
    }
    case ESP_GATTC_CFG_MTU_EVT: {
      if (param->cfg_mtu.status == ESP_GATT_OK)
        this->mtu_ = param->cfg_mtu.mtu;
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT:
    case ESP_GATTC_CLOSE_EVT: {
      this->parser_.reset();
      this->characteristic_handle_ = 0;
      this->cccd_handle_ = 0;
      this->mtu_ = 23;
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
      if (param->search_cmpl.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Service discovery failed, status=%d", param->search_cmpl.status);
        this->fail_session_();
        break;
      }
      auto *characteristic = this->parent()->get_characteristic(espbt::ESPBTUUID::from_raw(SERVICE_UUID),
                                                                espbt::ESPBTUUID::from_raw(CHARACTERISTIC_UUID));
      // Reconnecting would not help a device without them, so the link is left as it is
      if (characteristic == nullptr) {
        this->status_set_warning(LOG_STR("No BLE MIDI characteristic found on this device"));
        break;
      }
      auto *cccd = characteristic->get_descriptor(ESP_GATT_UUID_CHAR_CLIENT_CONFIG);
      if (cccd == nullptr) {
        this->status_set_warning(LOG_STR("BLE MIDI characteristic has no notification descriptor"));
        break;
      }
      this->characteristic_handle_ = characteristic->handle;
      this->cccd_handle_ = cccd->handle;
      this->write_without_response_ = (characteristic->properties & ESP_GATT_CHAR_PROP_BIT_WRITE_NR) != 0;
      esp_err_t err = this->parent()->register_for_notify(characteristic->handle);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "register_for_notify failed, error=%d", err);
        this->fail_session_();
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->reg_for_notify.handle != this->characteristic_handle_)
        break;
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Registering for notifications failed, status=%d", param->reg_for_notify.status);
        this->fail_session_();
      }
      // On success the parent writes the descriptor; the device has accepted it once WRITE_DESCR_EVT reports OK
      break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT: {
      if (this->cccd_handle_ == 0 || param->write.handle != this->cccd_handle_ || this->notify_registered_)
        break;
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Device refused to enable notifications, status=%d", param->write.status);
        this->fail_session_();
        break;
      }
      this->notify_registered_ = true;
      this->node_state = espbt::ClientState::ESTABLISHED;
      ESP_LOGI(TAG, "MIDI notifications enabled");
      // The device can only be sent to once notifications are enabled, so this
      // is the point where on_connect is useful.
      this->connect_callback_.call();
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT: {
      // A write with response reports its outcome here; one without response reports nothing
      if (param->write.handle == this->characteristic_handle_ && param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Write rejected, status=%d", param->write.status);
      }
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
      this->parser_.feed(param->notify.value, param->notify.value_len, BLEMidi::dispatch_message, this);
      break;
    }
    default:
      break;
  }
}

void BLEMidi::fail_session_() {
  this->status_set_warning();
  // ble_client reconnects, which runs the setup again
  this->parent()->disconnect();
}

void BLEMidi::handle_message_(const MidiMessage &message) {
  ESP_LOGV(
      TAG, "Decoded %s",
      LOG_STR_ARG(MessageTypeNames::get_log_str(static_cast<uint8_t>(message.type), MessageTypeNames::LAST_INDEX)));
  // Only decoded messages reach on_message
  if (message.type == MessageType::SYSEX_INCOMPLETE) {
    ESP_LOGW(TAG, "SysEx of %u bytes dropped: cut short, or longer than max_sysex_size (%u)", message.length,
             this->parser_.sysex_capacity());
    return;
  }
  if (message.type == MessageType::INCOMPLETE) {
    ESP_LOGW(TAG, "MIDI message 0x%02X dropped: cut short by another status byte", message.status());
    return;
  }
  if (message.type == MessageType::UNKNOWN) {
    ESP_LOGW(TAG, "Undecodable MIDI byte 0x%02X", message.status());
    return;
  }
  this->message_callback_.call(ByteSpan(message.data, message.length));

  switch (message.type) {
    case MessageType::NOTE_ON:
      this->note_on_callback_.call(message.data1(), message.data2(), message.channel());
      break;
    case MessageType::NOTE_OFF:
      this->note_off_callback_.call(message.data1(), message.data2(), message.channel());
      break;
    case MessageType::CONTROL_CHANGE:
      this->control_change_callback_.call(message.data1(), message.data2(), message.channel());
      break;
    case MessageType::PROGRAM_CHANGE:
      this->program_change_callback_.call(message.data1(), message.channel());
      break;
    case MessageType::PITCH_BEND:
      this->pitch_bend_callback_.call(message.pitch_bend(), message.channel());
      break;
    case MessageType::SYSEX:
      // The payload without the 0xF0/0xF7 wrappers; a complete SysEx always has both
      this->sysex_callback_.call(ByteSpan(message.data + 1, message.length - 2));
      break;
    default:
      // Polyphonic and channel aftertouch, system common and real-time messages
      // are only reported through on_message.
      break;
  }
}

bool BLEMidi::write_packet_(uint8_t *packet, size_t midi_length) {
  if (!this->is_ready()) {
    ESP_LOGW(TAG, "Cannot send: not connected");
    return false;
  }
  // Every BLE-MIDI packet has to fit one ATT packet
  const size_t packet_length = midi_length + 2;
  const size_t limit = std::min<size_t>(MAX_TX_MIDI_BYTES + 2, this->mtu_ - 3);
  if (packet_length > limit) {
    ESP_LOGW(TAG, "Cannot send: message of %u bytes exceeds the %u byte limit", static_cast<unsigned>(midi_length),
             static_cast<unsigned>(limit - 2));
    return false;
  }
  packet[0] = PACKET_HEADER;
  packet[1] = PACKET_TIMESTAMP;
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

bool BLEMidi::write_midi_(const uint8_t *midi_bytes, size_t length) {
  if (length == 0 || length > MAX_TX_MIDI_BYTES) {
    ESP_LOGW(TAG, "Cannot send: message of %u bytes, the limit is %u", static_cast<unsigned>(length),
             static_cast<unsigned>(MAX_TX_MIDI_BYTES));
    return false;
  }
  uint8_t packet[MAX_TX_MIDI_BYTES + 2];
  std::memcpy(packet + 2, midi_bytes, length);
  return this->write_packet_(packet, length);
}

void BLEMidi::send_channel_(uint8_t status, int32_t channel, int32_t data1, int32_t data2, size_t length) {
  // Validation covers constants only; a lambda can return any value, and masking it would send a different message.
  // The values arrive unnarrowed so a negative or wrapped one is caught here (the unsigned compare covers negatives).
  if (static_cast<uint32_t>(channel) > 0x0F || static_cast<uint32_t>(data1) > 0x7F ||
      static_cast<uint32_t>(data2) > 0x7F) {
    ESP_LOGW(TAG, "Cannot send: channel %" PRId32 " or value %" PRId32 "/%" PRId32 " out of range", channel, data1,
             data2);
    return;
  }
  const uint8_t bytes[3] = {static_cast<uint8_t>(status | channel), static_cast<uint8_t>(data1),
                            static_cast<uint8_t>(data2)};
  this->write_midi_(bytes, length);
}

void BLEMidi::send_note_on(int32_t note, int32_t velocity, int32_t channel) {
  this->send_channel_(STATUS_NOTE_ON, channel, note, velocity, 3);
}

void BLEMidi::send_note_off(int32_t note, int32_t velocity, int32_t channel) {
  this->send_channel_(STATUS_NOTE_OFF, channel, note, velocity, 3);
}

void BLEMidi::send_control_change(int32_t control_number, int32_t value, int32_t channel) {
  this->send_channel_(STATUS_CONTROL_CHANGE, channel, control_number, value, 3);
}

void BLEMidi::send_program_change(int32_t program, int32_t channel) {
  this->send_channel_(STATUS_PROGRAM_CHANGE, channel, program, 0, 2);
}

void BLEMidi::send_pitch_bend(int32_t value, int32_t channel) {
  if (value < -8192 || value > 8191) {
    ESP_LOGW(TAG, "Cannot send: pitch bend %" PRId32 " out of range", value);
    return;
  }
  const uint16_t unsigned_value = static_cast<uint16_t>(value + 8192);
  this->send_channel_(STATUS_PITCH_BEND, channel, unsigned_value & 0x7F, unsigned_value >> 7, 3);
}

void BLEMidi::send_sysex(const uint8_t *payload, size_t length) {
  uint8_t packet[MAX_TX_MIDI_BYTES + 2];
  const size_t framed = frame_sysex(payload, length, packet + 2, MAX_TX_MIDI_BYTES);
  if (framed == 0) {
    ESP_LOGW(TAG, "Cannot send: SysEx of %u bytes is empty or exceeds the %u byte limit", static_cast<unsigned>(length),
             static_cast<unsigned>(MAX_TX_MIDI_BYTES));
    return;
  }
  this->write_packet_(packet, framed);
}

void BLEMidi::send_raw(const uint8_t *bytes, size_t length) {
  // The packet carries one timestamp, in front of the first byte, so the payload has to be a single complete
  // message with its status byte first; a SysEx goes through send_sysex
  bool one_message = length > 0 && (bytes[0] & 0x80) != 0 && bytes[0] != STATUS_SYSEX_START &&
                     bytes[0] != STATUS_SYSEX_END && length == 1u + MidiParser::expected_data_bytes(bytes[0]);
  for (size_t i = 1; one_message && i < length; i++) {
    one_message = (bytes[i] & 0x80) == 0;
  }
  if (!one_message) {
    ESP_LOGW(TAG, "Cannot send: raw payload must be one message, a status byte followed by data bytes");
    return;
  }
  this->write_midi_(bytes, length);
}

}  // namespace esphome::ble_midi

#endif  // USE_ESP32
