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
        size_t comma_pos = message.find(',');
        int rssi_value = atoi(message.substr(6, comma_pos - 6).c_str());
        float quality = 0.0f;

        if (rssi_value >= 2 && rssi_value <= 30) {
          quality = ((rssi_value - 2.0f) / 30.0f) * 100.0f;
        }

        if (this->rssi_sensor_ != nullptr) {
          this->rssi_sensor_->publish_state(quality);
        }
      }

      // ICCID Bilgisini Al
      send_cmd_("AT+CCID");
      this->state_ = STATE_GET_ICCID;
      this->expect_ack_ = true;
      break;
    case STATE_GET_ICCID:
      if (message.compare(0, 7, "+CCID:") == 0) {
        std::string iccid = message.substr(7);
        iccid.erase(std::remove(iccid.begin(), iccid.end(), '\r'), iccid.end());
        iccid.erase(std::remove(iccid.begin(), iccid.end(), '\n'), iccid.end());

        if (this->iccid_sensor_ != nullptr) {
          this->iccid_sensor_->publish_state(iccid);
        }
      }

      // Durumu Sıfırla ve SMS kontrolüne geç
      this->state_ = STATE_CHECK_SMS;
      break;
    case STATE_SENDING_SMS_1:
      send_cmd_("AT+CMGS=\"" + this->send_to_ + "\"");
      this->state_ = STATE_SENDING_SMS_2;
      break;
    case STATE_SENDING_SMS_2:
      send_cmd_(this->message_ + std::string(1, static_cast<char>(26)));
      this->state_ = STATE_SENDING_SMS_WAIT;
      break;
    case STATE_SENDING_SMS_WAIT:
      if (message == "OK") {
        ESP_LOGI(TAG, "Send SMS: '%s' ok", this->message_.c_str());
        this->state_ = STATE_INIT;
        this->send_pending_ = false;
      } else {
        this->state_ = STATE_INIT;
        this->set_registered_(false);
        ESP_LOGW(TAG, "Send SMS: '%s' failed", this->message_.c_str());
      }
      break;
    case STATE_DIALING1:
      send_cmd_("ATD" + this->send_to_ + ";");
      this->state_ = STATE_DIALING2;
      break;
    case STATE_DIALING2:
      if (message == "OK") {
        ESP_LOGI(TAG, "Dial ok");
        this->state_ = STATE_INIT;
        this->dial_pending_ = false;
      } else {
        this->state_ = STATE_INIT;
        this->set_registered_(false);
        ESP_LOGW(TAG, "Dial: '%s' failed", this->send_to_.c_str());
      }
      break;
    case STATE_ATA_SENT:
      if (message == "OK") {
        ESP_LOGI(TAG, "Connect OK");
        this->state_ = STATE_INIT;
        this->call_state_ = 5;
        this->call_received_callback_.call();
      } else if (message == "NO CARRIER") {
        this->state_ = STATE_INIT;
        ESP_LOGW(TAG, "Connect failed");
      }
      break;
    case STATE_RECEIVE_SMS:
      this->state_ = STATE_PARSE_SMS;
      this->incoming_sms_.clear();
      this->incoming_sms_.append(message);
      break;
    case STATE_PARSE_SMS:
      if (message.empty()) {
        ESP_LOGW(TAG, "Invalid SMS message");
        this->state_ = STATE_INIT;
      } else if (message == "OK") {
        this->state_ = STATE_RECEIVED_SMS;
        ESP_LOGI(TAG, "Received SMS: '%s'", this->incoming_sms_.c_str());
        if (this->sms_received_callback_)
          this->sms_received_callback_.call(this->incoming_sms_);
      } else {
        this->incoming_sms_.append(message);
        this->incoming_sms_.append("\n");
      }
      break;
    case STATE_PARSE_SMS_RESPONSE:
      if (message.compare(0, 6, "+CMGL:") == 0) {
        // Example response:
        // +CMGL: 1,"REC UNREAD","+31612345678",,"17/06/28,21:04:40+08"
        // This matches the response index
        this->parse_index_ = atoi(message.substr(7).c_str());
        this->state_ = STATE_RECEIVE_SMS;
        this->expect_ack_ = true;
        this->parse_index_ = message[7] - '0';
      } else if (message == "OK") {
        this->state_ = STATE_INIT;
      }
      break;
    case STATE_CHECK_CALL:
      if (message.compare(0, 6, "+CLCC:") == 0) {
        this->state_ = STATE_INIT;
        int comma_index = message.find(',', 7);
        if (comma_index != std::string::npos) {
          if (message[comma_index + 1] == '0') {  // Outgoing call in progress
            if (this->call_state_ != 0) {
              this->call_state_ = 0;
              this->call_dialing_callback_.call();
            }
          } else if (message[comma_index + 1] == '6') {  // Call alerting
            if (this->call_state_ != 1) {
              this->call_state_ = 1;
              this->call_dialing_callback_.call();
            }
          } else if (message[comma_index + 1] == '2') {  // Incoming call
            if (this->call_state_ != 2) {
              this->call_state_ = 2;
              this->call_received_callback_.call();
            }
          } else if (message[comma_index + 1] == '3') {  // Call active
            if (this->call_state_ != 3) {
              this->call_state_ = 3;
              this->call_received_callback_.call();
            }
          } else if (message[comma_index + 1] == '0') {  // Call disconnected
            if (this->call_state_ != 6) {
              this->call_state_ = 6;
              this->call_disconnected_callback_.call();
            }
          }
        }
      } else if (message == "OK") {
        this->state_ = STATE_INIT;
      }
      break;
    default:
      ESP_LOGW(TAG, "Unhandled state %d", this->state_);
  }
}

}  // namespace sim800l
}  // namespace esphome
