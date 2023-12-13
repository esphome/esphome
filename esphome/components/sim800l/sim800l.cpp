#include "sim800l.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace sim800l {

static const char *const TAG = "sim800l";

const char ASCII_CR = 0x0D;
const char ASCII_LF = 0x0A;

void Sim800LComponent::update() {
  if (this->watch_dog_++ == 2) {
    this->state_ = STATE_INIT;
    this->write(26);
  }

  if (this->expect_ack_)
    return;

  if (state_ == STATE_INIT) {
    if (this->registered_ && this->send_pending_) {
      this->send_cmd_("AT+CSCS=\"GSM\"");
      this->state_ = STATE_SENDING_SMS_1;
    } else if (this->registered_ && this->dial_pending_) {
      this->send_cmd_("AT+CSCS=\"GSM\"");
      this->state_ = STATE_DIALING1;
    } else if (this->registered_ && this->connect_pending_) {
      this->connect_pending_ = false;
      ESP_LOGI(TAG, "Connecting...");
      this->send_cmd_("ATA");
      this->state_ = STATE_ATA_SENT;
    } else if (this->registered_ && this->send_ussd_pending_) {
      this->send_cmd_("AT+CSCS=\"GSM\"");
      this->state_ = STATE_SEND_USSD1;
    } else if (this->registered_ && this->disconnect_pending_) {
      this->disconnect_pending_ = false;
      ESP_LOGI(TAG, "Disconnecting...");
      this->send_cmd_("ATH");
    } else if (this->registered_ && this->call_state_ != 6) {
      send_cmd_("AT+CLCC");
      this->state_ = STATE_CHECK_CALL;
      return;
    } else {
      this->send_cmd_("AT");
      this->state_ = STATE_SETUP_CMGF;
    }
    this->expect_ack_ = true;
  } else if (state_ == STATE_RECEIVED_SMS) {
    // Serial Buffer should have flushed.
    // Send cmd to delete received sms
    char delete_cmd[20];
    sprintf(delete_cmd, "AT+CMGD=%d", this->parse_index_);
    this->send_cmd_(delete_cmd);
    this->state_ = STATE_CHECK_SMS;
    this->expect_ack_ = true;
  }
}

void Sim800LComponent::send_cmd_(const std::string &message) {
  ESP_LOGV(TAG, "S: %s - %d", message.c_str(), this->state_);
  this->watch_dog_ = 0;
  this->write_str(message.c_str());
  this->write_byte(ASCII_CR);
  this->write_byte(ASCII_LF);
}

void Sim800LComponent::parse_cmd_(std::string message) {
  if (message.empty())
    return;

  ESP_LOGV(TAG, "R: %s - %d", message.c_str(), this->state_);

  if (this->state_ != STATE_RECEIVE_SMS) {
    if (message == "RING") {
      // Incoming call...
      this->state_ = STATE_PARSE_CLIP;
      this->expect_ack_ = false;
    } else if (message == "NO CARRIER") {
      if (this->call_state_ != 6) {
        this->call_state_ = 6;
        this->call_disconnected_callback_.call();
      }
    }
  }

  bool ok = message == "OK";
  if (this->expect_ack_) {
    this->expect_ack_ = false;
    if (!ok) {
      if (this->state_ == STATE_SETUP_CMGF && message == "AT") {
        // Expected ack but AT echo received
        this->state_ = STATE_DISABLE_ECHO;
        this->expect_ack_ = true;
      } else {
        ESP_LOGW(TAG, "Not ack. %d %s", this->state_, message.c_str());
        this->state_ = STATE_IDLE;  // Let it timeout
        return;
      }
    }
  } else if (ok && (this->state_ != STATE_PARSE_SMS_RESPONSE && this->state_ != STATE_CHECK_CALL &&
                    this->state_ != STATE_RECEIVE_SMS && this->state_ != STATE_DIALING2)) {
    ESP_LOGW(TAG, "Received unexpected OK. Ignoring");
    return;
  }

  switch (this->state_) {
    case STATE_INIT: {
      // While we were waiting for update to check for messages, this notifies a message
      // is available.
      bool message_available = message.compare(0, 6, "+CMTI:") == 0;
      if (!message_available) {
        if (message == "RING") {
          // Incoming call...
          this->state_ = STATE_PARSE_CLIP;
        } else if (message == "NO CARRIER") {
          if (this->call_state_ != 6) {
            this->call_state_ = 6;
            this->call_disconnected_callback_.call();
          }
        } else if (message.compare(0, 6, "+CUSD:") == 0) {
          // Incoming USSD MESSAGE
          this->state_ = STATE_CHECK_USSD;
        }
        break;
      }

      // Else fall thru ...
    }
    case STATE_CHECK_SMS:
      send_cmd_("AT+CMGL=\"ALL\"");
      this->state_ = STATE_PARSE_SMS_RESPONSE;
      this->parse_index_ = 0;
      break;
    case STATE_DISABLE_ECHO:
      send_cmd_("ATE0");
      this->state_ = STATE_SETUP_CMGF;
      this->expect_ack_ = true;
      break;
    case STATE_SETUP_CMGF:
      send_cmd_("AT+CMGF=1");
      this->state_ = STATE_SETUP_CLIP;
      this->expect_ack_ = true;
      break;
    case STATE_SETUP_CLIP:
      send_cmd_("AT+CLIP=1");
      this->state_ = STATE_CREG;
      this->expect_ack_ = true;
      break;
    case STATE_SETUP_USSD:
      send_cmd_("AT+CUSD=1");
      this->state_ = STATE_CREG;
      this->expect_ack_ = true;
      break;
    case STATE_SEND_USSD1:
      this->send_cmd_("AT+CUSD=1, \"" + this->ussd_ + "\"");
      this->state_ = STATE_SEND_USSD2;
      this->expect_ack_ = true;
      break;
    case STATE_SEND_USSD2:
      ESP_LOGD(TAG, "SendUssd2: '%s'", message.c_str());
      if (message == "OK") {
        // Dialing
        ESP_LOGD(TAG, "Dialing ussd code: '%s' done.", this->ussd_.c_str());
        this->state_ = STATE_CHECK_USSD;
        this->send_ussd_pending_ = false;
      } else {
        this->set_registered_(false);
        this->state_ = STATE_INIT;
        this->send_cmd_("AT+CMEE=2");
        this->write(26);
      }
      break;
    case STATE_CHECK_USSD:
      ESP_LOGD(TAG, "Check ussd code: '%s'", message.c_str());
      if (message.compare(0, 6, "+CUSD:") == 0) {
        this->state_ = STATE_RECEIVED_USSD;
        this->ussd_ = "";
        size_t start = 10;
        size_t end = message.find_last_of(',');
        if (end > start) {
          this->ussd_ = message.substr(start + 1, end - start - 2);
          this->ussd_received_callback_.call(this->ussd_);
        }
      }
      // Otherwise we receive another OK, we do nothing just wait polling to continuously check for SMS
      if (message == "OK")
        this->state_ = STATE_INIT;
      break;
    case STATE_CREG:
      send_cmd_("AT+CREG?");
      this->state_ = STATE_CREG_WAIT;
      break;
    case STATE_CREG_WAIT: {
      // Response: "+CREG: 0,1" -- the one there means registered ok
      //           "+CREG: -,-" means not registered ok
      bool registered = message.compare(0, 6, "+CREG:") == 0 && (message[9] == '1' || message[9] == '5');
      if (registered) {
        if (!this->registered_) {
          ESP_LOGD(TAG, "Registered OK");
        }
        this->state_ = STATE_CSQ;
        this->expect_ack_ = true;
      } else {
        ESP_LOGW(TAG, "Registration Fail");
        if (message[7] == '0') {  // Network registration is disable, enable it
          send_cmd_("AT+CREG=1");
          this->expect_ack_ = true;
          this->state_ = STATE_SETUP_CMGF;
        } else {
          // Keep waiting registration
          this->state_ = STATE_INIT;
        }
      }
      set_registered_(registered);
      break;
    }
    case STATE_CSQ:
      send_cmd_("AT+CSQ");
      this->state_ = STATE_CSQ_RESPONSE;
      break;
    case STATE_CSQ_RESPONSE:
      if (message.compare(0, 5, "+CSQ:") == 0) {
        size_t comma = message.find(',', 6);
        if (comma != 6) {
          int rssi = parse_number<int>(message.substr(6, comma - 6)).value_or(0);

#ifdef USE_SENSOR
          if (this->rssi_sensor_ != nullptr) {
            this->rssi_sensor_->publish_state(rssi);
          } else {
            ESP_LOGD(TAG, "RSSI: %d", rssi);
          }
#else
          ESP_LOGD(TAG, "RSSI: %d", rssi);
#endif
        }
      }
      this->expect_ack_ = true;
      this->state_ = STATE_CHECK_SMS;
      break;
    case STATE_PARSE_SMS_RESPONSE:
      if (message.compare(0, 6, "+CMGL:") == 0 && this->parse_index_ == 0) {
        size_t start = 7;
        size_t end = message.find(',', start);
        uint8_t item = 0;
        while (end != start) {
          item++;
          if (item == 1) {  // Slot Index
            this->parse_index_ = parse_number<uint8_t>(message.substr(start, end - start)).value_or(0);
          }
          // item 2 = STATUS, usually "REC UNREAD"
          if (item == 3) {  // recipient
            // Add 1 and remove 2 from substring to get rid of "quotes"
            this->sender_ = message.substr(start + 1, end - start - 2);
            this->message_.clear();
            break;
          }
          // item 4 = ""
          // item 5 = Received timestamp
          start = end + 1;
          end = message.find(',', start);
        }

        if (item < 2) {
          ESP_LOGD(TAG, "Invalid message %d %s", this->state_, message.c_str());
          return;
        }
        this->state_ = STATE_RECEIVE_SMS;
      }
      // Otherwise we receive another OK
      if (ok) {
        send_cmd_("AT+CLCC");
        this->state_ = STATE_CHECK_CALL;
      }
      break;
    case STATE_CHECK_CALL:
      if (message.compare(0, 6, "+CLCC:") == 0 && this->parse_index_ == 0) {
        this->expect_ack_ = true;
        size_t start = 7;
        size_t end = message.find(',', start);
        uint8_t item = 0;
        while (end != start) {
          item++;
          // item 1 call index for +CHLD
          // item 2 dir 0 Mobile originated; 1 Mobile terminated
          if (item == 3) {  // stat
            uint8_t current_call_state = parse_number<uint8_t>(message.substr(start, end - start)).value_or(6);
            if (current_call_state != this->call_state_) {
              ESP_LOGD(TAG, "Call state is now: %d", current_call_state);
              if (current_call_state == 0)
                this->call_connected_callback_.call();
            }
            this->call_state_ = current_call_state;
            break;
          }
          // item 4 = ""
          // item 5 = Received timestamp
          start = end + 1;
          end = message.find(',', start);
        }

        if (item < 2) {
          ESP_LOGD(TAG, "Invalid message %d %s", this->state_, message.c_str());
          return;
        }
      } else if (ok) {
        if (this->call_state_ != 6) {
          // no call in progress
          this->call_state_ = 6;  // Disconnect
          this->call_disconnected_callback_.call();
        }
      }
      this->state_ = STATE_INIT;
      break;
    case STATE_RECEIVE_SMS:
      /* Our recipient is set and the message body is in message
        kick ESPHome callback now
      */
      if (ok || message.compare(0, 6, "+CMGL:") == 0) {
        ESP_LOGD(TAG, "Received SMS from: %s", this->sender_.c_str());
        ESP_LOGD(TAG, "%s", this->message_.c_str());
        this->sms_received_callback_.call(this->message_, this->sender_);
        this->state_ = STATE_RECEIVED_SMS;
      } else {
        if (this->message_.length() > 0)
          this->message_ += "\n";
        this->message_ += message;
      }
      break;
    case STATE_RECEIVED_SMS:
    case STATE_RECEIVED_USSD:
      // Let the buffer flush. Next poll will request to delete the parsed index message.
      break;
    case STATE_SENDING_SMS_1:
      this->send_cmd_("AT+CMGS=\"" + this->recipient_ + "\"");
      this->state_ = STATE_SENDING_SMS_2;
      break;
    case STATE_SENDING_SMS_2:
      if (message == ">") {
        // Send sms body
        ESP_LOGI(TAG, "Sending to %s message: '%s'", this->recipient_.c_str(), this->outgoing_message_.c_str());
        this->write_str(this->outgoing_message_.c_str());
        this->write(26);
        this->state_ = STATE_SENDING_SMS_3;
      } else {
        set_registered_(false);
        this->state_ = STATE_INIT;
        this->send_cmd_("AT+CMEE=2");
        this->write(26);
      }
      break;
    case STATE_SENDING_SMS_3:
      if (message.compare(0, 6, "+CMGS:") == 0) {
        ESP_LOGD(TAG, "SMS Sent OK: %s", message.c_str());
        this->send_pending_ = false;
        this->state_ = STATE_CHECK_SMS;
        this->expect_ack_ = true;
      }
      break;
    case STATE_DIALING1:
      this->send_cmd_("ATD" + this->recipient_ + ';');
      this->state_ = STATE_DIALING2;
      break;
    case STATE_DIALING2:
      if (ok) {
        ESP_LOGI(TAG, "Dialing: '%s'", this->recipient_.c_str());
        this->dial_pending_ = false;
      } else {
        this->set_registered_(false);
        this->send_cmd_("AT+CMEE=2");
        this->write(26);
      }
      this->state_ = STATE_INIT;
      break;
    case STATE_PARSE_CLIP:
      if (message.compare(0, 6, "+CLIP:") == 0) {
        std::string caller_id;
        size_t start = 7;
        size_t end = message.find(',', start);
        uint8_t item = 0;
        while (end != start) {
          item++;
          if (item == 1) {  // Slot Index
            // Add 1 and remove 2 from substring to get rid of "quotes"
            caller_id = message.substr(start + 1, end - start - 2);
            break;
          }
          // item 4 = ""
          // item 5 = Received timestamp
          start = end + 1;
          end = message.find(',', start);
        }
        if (this->call_state_ != 4) {
          this->call_state_ = 4;
          ESP_LOGI(TAG, "Incoming call from %s", caller_id.c_str());
          incoming_call_callback_.call(caller_id);
        }
        this->state_ = STATE_INIT;
      }
      break;
    case STATE_ATA_SENT:
      ESP_LOGI(TAG, "Call connected");
      if (this->call_state_ != 0) {
        this->call_state_ = 0;
        this->call_connected_callback_.call();
      }
      this->state_ = STATE_INIT;
      break;
    default:
      ESP_LOGW(TAG, "Unhandled: %s - %d", message.c_str(), this->state_);
      break;
  }
}  // namespace sim800l

void Sim800LComponent::loop() {
  // Read message
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);

    if (this->read_pos_ == SIM800L_READ_BUFFER_LENGTH)
      this->read_pos_ = 0;

    ESP_LOGVV(TAG, "Buffer pos: %u %d", this->read_pos_, byte);  // NOLINT

    if (byte == ASCII_CR)
      continue;
    if (byte >= 0x7F)
      byte = '?';  // need to be valid utf8 string for log functions.
    this->read_buffer_[this->read_pos_] = byte;

    if (this->state_ == STATE_SENDING_SMS_2 && this->read_pos_ == 0 && byte == '>')
      this->read_buffer_[++this->read_pos_] = ASCII_LF;

    if (this->read_buffer_[this->read_pos_] == ASCII_LF) {
      this->read_buffer_[this->read_pos_] = 0;
      this->read_pos_ = 0;
      this->parse_cmd_(this->read_buffer_);
    } else {
      this->read_pos_++;
    }
  }
  if (state_ == STATE_INIT && this->registered_ &&
      (this->call_state_ != 6  // A call is in progress
       || this->send_pending_ || this->dial_pending_ || this->connect_pending_ || this->disconnect_pending_)) {
    this->update();
  }
}

void Sim800LComponent::send_sms(const std::string &recipient, const std::string &message) {
  this->recipient_ = recipient;
  this->outgoing_message_ = message;
  this->send_pending_ = true;
}

void Sim800LComponent::send_ussd(const std::string &ussd_code) {
  ESP_LOGD(TAG, "Sending USSD code: %s", ussd_code.c_str());
  this->ussd_ = ussd_code;
  this->send_ussd_pending_ = true;
  this->update();
}
void Sim800LComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SIM800L:");
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Registered", this->registered_binary_sensor_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Rssi", this->rssi_sensor_);
#endif
}
void Sim800LComponent::dial(const std::string &recipient) {
  this->recipient_ = recipient;
  this->dial_pending_ = true;
}
void Sim800LComponent::connect() { this->connect_pending_ = true; }
void Sim800LComponent::disconnect() { this->disconnect_pending_ = true; }

void Sim800LComponent::set_registered_(bool registered) {
  this->registered_ = registered;
#ifdef USE_BINARY_SENSOR
  if (this->registered_binary_sensor_ != nullptr)
    this->registered_binary_sensor_->publish_state(registered);
#endif
}

}  // namespace sim800l
}  // namespace esphome
