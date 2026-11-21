#include "tuya_uart.h"
#include "esphome/core/application.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include <cctype>
#include <cstring>

#ifdef USE_NETWORK
#include "esphome/components/network/util.h"
#endif

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

#ifdef USE_CAPTIVE_PORTAL
#include "esphome/components/captive_portal/captive_portal.h"
#endif

namespace esphome::tuya {

ESPHOME_LOG_TAG(TAG, "tuya");

static constexpr uint32_t HEARTBEAT_INTERVAL_ID = 0;
static constexpr uint32_t WIFI_STATUS_INTERVAL_ID = 1;
static constexpr uint32_t DATAPOINT_DUMP_TIMEOUT_ID = 2;
static constexpr uint32_t FORCE_TIME_SYNC_INTERVAL_ID = 3;
static const int COMMAND_DELAY = 10;
// Timing of Tuya's module firmware
static constexpr uint32_t INIT_HEARTBEAT_INTERVAL = 3000;
static constexpr uint32_t HEARTBEAT_INTERVAL = 15000;
static constexpr uint32_t RESPONSE_TIMEOUT = 1000;
static constexpr uint32_t RX_FRAME_TIMEOUT = 400;
static constexpr uint32_t FORCE_TIME_SYNC_CHECK_INTERVAL = 1000;
#ifdef USE_TUYA_MCU_WATCHDOG_RESET
// action: reset reboots the module when this many resets in a row brought no heartbeat answer
static constexpr uint8_t MAX_UNANSWERED_RESETS = 3;
#endif

// Command whose missing reply fails an init step. INIT_WIFI has no query of its own, the heartbeat stands in.
static TuyaCommandType init_step_query(TuyaInitState state) {
  switch (state) {
    case TuyaInitState::INIT_PRODUCT:
      return TuyaCommandType::PRODUCT_QUERY;
    case TuyaInitState::INIT_CONF:
      return TuyaCommandType::CONF_QUERY;
    case TuyaInitState::INIT_DATAPOINT:
      return TuyaCommandType::DATAPOINT_QUERY;
    default:
      return TuyaCommandType::HEARTBEAT;
  }
}

static bool network_is_connected() {
#ifdef USE_NETWORK
  return network::is_connected();
#else
  return false;
#endif
}

void TuyaUART::setup() {
  if (this->heartbeat_) {
    this->set_heartbeat_interval_(INIT_HEARTBEAT_INTERVAL);
  } else {
    // without a heartbeat the initialization starts at the product query
    this->init_state_ = TuyaInitState::INIT_PRODUCT;
    this->set_interval(HEARTBEAT_INTERVAL_ID, INIT_HEARTBEAT_INTERVAL, [this] { this->retry_init_(); });
    this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
  }
#ifdef USE_TIME
  if (this->force_time_sync_) {
    this->register_local_time_sync_callback_();
    this->set_interval(FORCE_TIME_SYNC_INTERVAL_ID, FORCE_TIME_SYNC_CHECK_INTERVAL,
                       [this] { this->check_force_time_sync_(); });
  }
#endif
  if (this->status_pin_ != nullptr) {
    this->status_pin_->digital_write(false);
  }
}

void TuyaUART::loop() {
#ifdef USE_TUYA_MCU_WATCHDOG_RESET
  if (this->tx_break_active_) {
    if (App.get_loop_component_start_time() - this->tx_break_start_ < this->watchdog_reset_time_)
      return;
    this->end_tx_break_();
  }
  // a reply handled while reading can start the break, nothing more is read then
  this->read_available([this](uint8_t b) {
    this->handle_char_(b);
    return !this->tx_break_active_;
  });
#else
  this->read_available([this](uint8_t b) { this->handle_char_(b); });
#endif
  process_command_queue_();
}

void TuyaUART::set_heartbeat_interval_(uint32_t interval) {
  this->set_interval(HEARTBEAT_INTERVAL_ID, interval,
                     [this] { this->send_empty_command_(TuyaCommandType::HEARTBEAT); });
}

void TuyaUART::retry_init_() {
  // a query of the current step may still wait for its reply
  if (!this->command_queue_.empty())
    return;
  switch (this->init_state_) {
    case TuyaInitState::INIT_PRODUCT:
      this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
      break;
    case TuyaInitState::INIT_WIFI:
      // the status has no reply timeout: the third retry without a reply is the failure
      if (this->wifi_status_retries_ < 3) {
        this->wifi_status_retries_++;
        if (this->wifi_status_retries_ == 3)
          this->log_init_failure_();
      }
      // sent again by the status interval
      this->wifi_status_ = -1;
      break;
    case TuyaInitState::INIT_DATAPOINT:
      this->send_empty_command_(TuyaCommandType::DATAPOINT_QUERY);
      break;
    default:
      break;
  }
}

void TuyaUART::log_init_failure_() {
  if (this->init_failure_logged_ && this->init_failure_state_ == this->init_state_)
    return;
  this->init_failure_logged_ = true;
  this->init_failure_state_ = this->init_state_;
  ESP_LOGE(TAG, "Initialization failed at init_state %u", static_cast<uint8_t>(this->init_state_));
}

void TuyaUART::start_network_status_() {
  if (this->status_pin_ != nullptr) {
    this->init_state_ = TuyaInitState::INIT_DATAPOINT;
    this->send_empty_command_(TuyaCommandType::DATAPOINT_QUERY);
    this->set_interval(WIFI_STATUS_INTERVAL_ID, 1000, [this] { this->set_status_pin_(); });
  } else {
    this->init_state_ = TuyaInitState::INIT_WIFI;
    this->wifi_status_ = -1;
    this->wifi_status_retries_ = 0;
    this->set_interval(WIFI_STATUS_INTERVAL_ID, 1000, [this] { this->send_wifi_status_(); });
  }
}

void TuyaUART::dump_config() {
  this->dump_mcu_config_();
  Tuya::dump_config();
}

// Also logged again once the MCU reports its datapoints
void TuyaUART::dump_mcu_config_() {
  ESP_LOGCONFIG(TAG, "Tuya:");
  if (!this->heartbeat_ || this->datapoint_ack_) {
    ESP_LOGCONFIG(TAG, "  Heartbeat: %s, Datapoint Ack: %s", YESNO(this->heartbeat_), YESNO(this->datapoint_ack_));
  }
#ifdef USE_TUYA_MCU_WATCHDOG
  if (this->watchdog_missed_heartbeats_ != 0) {
#ifdef USE_TUYA_MCU_WATCHDOG_RESET
    if (this->watchdog_reset_time_ != 0) {
      ESP_LOGCONFIG(TAG,
                    "  MCU Watchdog: reset after %u unanswered heartbeats\n"
                    "  Reset Time: %" PRIu32 " ms, MCU Resets: %" PRIu32,
                    this->watchdog_missed_heartbeats_, this->watchdog_reset_time_, this->mcu_resets_);
    } else
#endif
    {
      ESP_LOGCONFIG(TAG, "  MCU Watchdog: reboot after %u unanswered heartbeats", this->watchdog_missed_heartbeats_);
      if (!this->working_mode_received_) {
        ESP_LOGCONFIG(TAG, "  MCU Watchdog is not armed before the working mode reply of the MCU");
      }
    }
  }
#endif
  if (this->init_state_ != TuyaInitState::INIT_DONE) {
    // init_failure_logged_: with heartbeat: false the failed step is retried and init_failed_ stays clear
    if (this->init_failed_ || this->init_failure_logged_) {
      ESP_LOGCONFIG(TAG, "  Initialization failed. Current init_state: %u", static_cast<uint8_t>(this->init_state_));
    } else {
      ESP_LOGCONFIG(TAG, "  Configuration will be reported when setup is complete. Current init_state: %u",
                    static_cast<uint8_t>(this->init_state_));
    }
    ESP_LOGCONFIG(TAG, "  If no further output is received, confirm that this is a supported Tuya device.");
    return;
  }
  this->dump_datapoints_();
  if ((this->status_pin_reported_ != -1) || (this->reset_pin_reported_ != -1)) {
    ESP_LOGCONFIG(TAG, "  GPIO Configuration: status: pin %d, reset: pin %d", this->status_pin_reported_,
                  this->reset_pin_reported_);
  }
  LOG_PIN("  Status Pin: ", this->status_pin_);
  ESP_LOGCONFIG(TAG, "  Product: '%s'",
                this->product_invalid_ ? LOG_STR_LITERAL(R"({"p":"INVALID"})")
                                       : (this->product_ ? this->product_.get() : LOG_STR_LITERAL("")));
#ifdef USE_TIME
  if (this->time_id_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Force time sync: %s", YESNO(this->force_time_sync_));
  }
#endif
}

bool TuyaUART::validate_message_() {
  uint32_t at = this->rx_message_.size() - 1;
  auto *data = &this->rx_message_[0];
  uint8_t new_byte = data[at];

  // Byte 0: HEADER1 (always 0x55)
  if (at == 0)
    return new_byte == 0x55;
  // Byte 1: HEADER2 (always 0xAA)
  if (at == 1)
    return new_byte == 0xAA;

  // Byte 2: VERSION
  // no validation for the following fields:
  uint8_t version = data[2];
  if (at == 2)
    return true;
  // Byte 3: COMMAND
  uint8_t command = data[3];
  if (at == 3)
    return true;

  // Byte 4: LENGTH1
  // Byte 5: LENGTH2
  if (at <= 5) {
    // no validation for these fields
    return true;
  }

  uint16_t length = encode_uint16(data[4], data[5]);

  // wait until all data is read
  if (at - 6 < length)
    return true;

  // Byte 6+LEN: CHECKSUM - sum of all bytes (including header) modulo 256
  uint8_t rx_checksum = new_byte;
  const uint8_t calc_checksum = sum_bytes(data, 6 + length);

  if (rx_checksum != calc_checksum) {
    ESP_LOGW(TAG, "Tuya Received invalid message checksum %02X!=%02X", rx_checksum, calc_checksum);
    return false;
  }

  // valid message
  const uint8_t *message_data = data + 6;
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex_buf[format_hex_pretty_size(MAX_DATAPOINT_LOG_BYTES)];
  ESP_LOGV(TAG, "Received Tuya: CMD=0x%02X VERSION=%u DATA=[%s] INIT_STATE=%u", command, version,
           format_hex_pretty_to(hex_buf, message_data, length), static_cast<uint8_t>(this->init_state_));
#endif
  this->handle_command_(command, version, message_data, length);

  // return false to reset rx buffer
  return false;
}

void TuyaUART::handle_char_(uint8_t c) {
  this->rx_message_.push_back(c);
  if (!this->validate_message_()) {
    this->rx_message_.clear();
  } else {
    this->last_rx_char_timestamp_ = millis();
  }
}

void TuyaUART::handle_command_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len) {
  TuyaCommandType command_type = (TuyaCommandType) command;

  // a datapoint command is answered by either kind of report
  const TuyaCommandType reply_type =
      command_type == TuyaCommandType::DATAPOINT_REPORT_SYNC ? TuyaCommandType::DATAPOINT_REPORT_ASYNC : command_type;
  if (this->expected_response_.has_value() && this->expected_response_ == reply_type) {
    this->expected_response_.reset();
    this->command_queue_.erase(command_queue_.begin());
#ifdef USE_TUYA_MCU_WATCHDOG
    // two heartbeats in a row answered at the first send take one miss off: an MCU that skips one now and then
    // is not counted out. A stalled MCU answers late: an answer to the second send takes none off and ends the row
    if (command_type == TuyaCommandType::HEARTBEAT) {
      this->unarmed_misses_logged_ = false;
      if (this->resent_) {
        this->heartbeat_answers_ = 0;
      } else if (++this->heartbeat_answers_ == 2) {
        this->heartbeat_answers_ = 0;
        if (this->heartbeat_misses_ != 0)
          this->heartbeat_misses_--;
      }
    }
#endif
#ifdef USE_TUYA_MCU_WATCHDOG_RESET
    // only an answer to the first send: a stalled MCU answers late
    if (command_type == TuyaCommandType::HEARTBEAT && !this->resent_ && this->tx_break_unconfirmed_) {
      this->tx_break_unconfirmed_ = false;
      this->tx_break_failures_ = 0;
      this->status_clear_warning();
      ESP_LOGI(TAG, "MCU answers heartbeats again after the reset");
    }
#endif
    this->resent_ = false;
  }

  switch (command_type) {
    case TuyaCommandType::HEARTBEAT:
      ESP_LOGV(TAG, "MCU Heartbeat (0x%02X)", buffer[0]);
      this->protocol_version_ = version;
#ifndef USE_TUYA_MCU_WATCHDOG
      this->heartbeat_missed_ = false;
#endif
      if (buffer[0] == 0) {
        ESP_LOGI(TAG, "MCU restarted");
        this->init_state_ = TuyaInitState::INIT_HEARTBEAT;
        this->init_failure_logged_ = false;
        if (this->working_mode_received_) {
          this->working_mode_received_ = false;
          this->set_heartbeat_interval_(INIT_HEARTBEAT_INTERVAL);
        }
      }
      // a failed initialization starts again with the next heartbeat reply
      if (this->init_failed_) {
        this->init_failed_ = false;
        if (this->init_state_ != TuyaInitState::INIT_DONE)
          this->init_state_ = TuyaInitState::INIT_HEARTBEAT;
      }
      if (this->init_state_ == TuyaInitState::INIT_HEARTBEAT) {
        this->init_state_ = TuyaInitState::INIT_PRODUCT;
        this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
      }
      break;
    case TuyaCommandType::PRODUCT_QUERY: {
      // check it is a valid string made up of printable characters
      bool valid = true;
      for (size_t i = 0; i < len; i++) {
        if (!std::isprint(buffer[i])) {
          valid = false;
          break;
        }
      }
      if (valid) {
        this->product_ = std::make_unique<char[]>(len + 1);
        memcpy(this->product_.get(), buffer, len);
      } else {
        this->product_.reset();
      }
      this->product_invalid_ = !valid;
      if (this->init_state_ == TuyaInitState::INIT_PRODUCT) {
        if (this->heartbeat_) {
          this->init_state_ = TuyaInitState::INIT_CONF;
          this->send_empty_command_(TuyaCommandType::CONF_QUERY);
        } else {
          // no heartbeat reply to take the version from, and no working mode query
          this->protocol_version_ = version;
          this->start_network_status_();
        }
      }
      break;
    }
    case TuyaCommandType::CONF_QUERY: {
      if (len >= 2) {
        this->status_pin_reported_ = buffer[0];
        this->reset_pin_reported_ = buffer[1];
      }
      // Tuya's module firmware goes to the normal heartbeat interval with this reply
      if (this->heartbeat_ && !this->working_mode_received_) {
        this->working_mode_received_ = true;
        this->set_heartbeat_interval_(HEARTBEAT_INTERVAL);
#ifdef USE_TUYA_MCU_WATCHDOG
        // the reboot is armed here: misses from before do not count towards it
        this->heartbeat_misses_ = 0;
        this->heartbeat_answers_ = 0;
#endif
      }
      if (this->init_state_ == TuyaInitState::INIT_CONF) {
        // If mcu returned status gpio, then we can omit sending wifi state
        if (this->status_pin_reported_ != -1) {
          this->init_state_ = TuyaInitState::INIT_DATAPOINT;
          this->send_empty_command_(TuyaCommandType::DATAPOINT_QUERY);
          if (this->status_pin_ != nullptr) {
            if (this->status_pin_->get_pin() != this->status_pin_reported_) {
              ESP_LOGW(TAG, "Supplied status_pin does not equal the reported pin %i. Using supplied pin anyway.",
                       this->status_pin_reported_);
            }
            ESP_LOGV(TAG, "Configured status pin %i", this->status_pin_->get_pin());
            this->set_interval(WIFI_STATUS_INTERVAL_ID, 1000, [this] { this->set_status_pin_(); });
          } else {
            ESP_LOGW(TAG, "MCU reported status_pin %i but no status_pin was configured; running in limited mode.",
                     this->status_pin_reported_);
          }
        } else {
          this->init_state_ = TuyaInitState::INIT_WIFI;
          // the status is sent again when the initialization starts over
          this->wifi_status_ = -1;
          ESP_LOGV(TAG, "Configured WIFI_STATE periodic send");
          this->set_interval(WIFI_STATUS_INTERVAL_ID, 1000, [this] { this->send_wifi_status_(); });
        }
      }
      break;
    }
    case TuyaCommandType::WIFI_STATE:
      if (this->init_state_ == TuyaInitState::INIT_WIFI) {
        this->init_state_ = TuyaInitState::INIT_DATAPOINT;
        this->send_empty_command_(TuyaCommandType::DATAPOINT_QUERY);
      }
      break;
    case TuyaCommandType::WIFI_SELECT:
    case TuyaCommandType::WIFI_RESET: {
      const bool is_select = (len >= 1);
      // Send WIFI_SELECT ACK
      TuyaCommand ack;
      ack.cmd = is_select ? TuyaCommandType::WIFI_SELECT : TuyaCommandType::WIFI_RESET;
      ack.payload.clear();
      this->send_command_(ack);
      // Establish pairing mode for correct first WIFI_STATE byte, EZ (0x00) default
      uint8_t first = 0x00;
      const char *mode_str = "EZ";
      if (is_select && buffer[0] == 0x01) {
        first = 0x01;
        mode_str = "AP";
      }
      // Send WIFI_STATE response, MCU exits pairing mode
      TuyaCommand st;
      st.cmd = TuyaCommandType::WIFI_STATE;
      st.payload.resize(1);
      st.payload[0] = first;
      this->send_command_(st);
      st.payload[0] = 0x02;
      this->send_command_(st);
      st.payload[0] = 0x03;
      this->send_command_(st);
      st.payload[0] = 0x04;
      this->send_command_(st);
      ESP_LOGI(TAG, "%s received (%s), replied with WIFI_STATE confirming connection established",
               is_select ? LOG_STR_LITERAL("WIFI_SELECT") : LOG_STR_LITERAL("WIFI_RESET"), mode_str);
      break;
    }
    case TuyaCommandType::DATAPOINT_DELIVER:
      break;
    case TuyaCommandType::DATAPOINT_REPORT_ASYNC:
    case TuyaCommandType::DATAPOINT_REPORT_SYNC:
      if (this->init_state_ == TuyaInitState::INIT_DATAPOINT) {
        this->init_state_ = TuyaInitState::INIT_DONE;
        this->init_failed_ = false;
        this->init_failure_logged_ = false;
        if (!this->heartbeat_)
          this->cancel_interval(HEARTBEAT_INTERVAL_ID);
        this->set_timeout(DATAPOINT_DUMP_TIMEOUT_ID, 1000, [this] { this->dump_mcu_config_(); });
        this->mark_initialized_();
      }
      // a sync report has its own answer below
      this->handle_datapoints_(buffer, len,
                               this->datapoint_ack_ && command_type == TuyaCommandType::DATAPOINT_REPORT_ASYNC);

      if (command_type == TuyaCommandType::DATAPOINT_REPORT_SYNC) {
        this->send_command_(
            TuyaCommand{.cmd = TuyaCommandType::DATAPOINT_REPORT_ACK, .payload = std::vector<uint8_t>{0x01}});
      }
      break;
    case TuyaCommandType::DATAPOINT_QUERY:
      break;
    case TuyaCommandType::WIFI_TEST:
      this->send_command_(TuyaCommand{.cmd = TuyaCommandType::WIFI_TEST, .payload = std::vector<uint8_t>{0x00, 0x00}});
      break;
    case TuyaCommandType::WIFI_RSSI:
      this->send_command_(
          TuyaCommand{.cmd = TuyaCommandType::WIFI_RSSI, .payload = std::vector<uint8_t>{get_wifi_rssi_()}});
      break;
    case TuyaCommandType::LOCAL_TIME_QUERY:
#ifdef USE_TIME
      if (this->time_id_ != nullptr) {
        this->send_local_time_();
        this->register_local_time_sync_callback_();
      } else
#endif
      {
        ESP_LOGW(TAG, "LOCAL_TIME_QUERY is not handled because time is not configured");
      }
      break;
    case TuyaCommandType::GMT_TIME_QUERY:
#ifdef USE_TIME
      if (this->time_id_ != nullptr) {
        this->send_gmt_time_();

        if (!this->gmt_time_sync_callback_registered_) {
          // tuya mcu supports time, so we let them know when our time changed
          this->time_id_->add_on_time_sync_callback([this] { this->send_gmt_time_(); });
          this->gmt_time_sync_callback_registered_ = true;
        }
      } else
#endif
      {
        ESP_LOGW(TAG, "GMT_TIME_QUERY is not handled because time is not configured");
      }
      break;
    case TuyaCommandType::VACUUM_MAP_UPLOAD:
      this->send_command_(
          TuyaCommand{.cmd = TuyaCommandType::VACUUM_MAP_UPLOAD, .payload = std::vector<uint8_t>{0x01}});
      ESP_LOGW(TAG, "Vacuum map upload requested, responding that it is not enabled.");
      break;
    case TuyaCommandType::GET_NETWORK_STATUS: {
      uint8_t wifi_status = this->get_wifi_status_code_();

      this->send_command_(
          TuyaCommand{.cmd = TuyaCommandType::GET_NETWORK_STATUS, .payload = std::vector<uint8_t>{wifi_status}});
      ESP_LOGV(TAG, "Network status requested, reported as %i", wifi_status);
      break;
    }
    case TuyaCommandType::EXTENDED_SERVICES: {
      if (len == 0) {
        ESP_LOGD(TAG, "Extended services request without a subcommand is not handled");
        break;
      }
      uint8_t subcommand = buffer[0];
      switch ((TuyaExtendedServicesCommandType) subcommand) {
        case TuyaExtendedServicesCommandType::RESET_NOTIFICATION: {
          this->send_command_(
              TuyaCommand{.cmd = TuyaCommandType::EXTENDED_SERVICES,
                          .payload = std::vector<uint8_t>{
                              static_cast<uint8_t>(TuyaExtendedServicesCommandType::RESET_NOTIFICATION), 0x00}});
          ESP_LOGV(TAG, "Reset status notification enabled");
          break;
        }
        case TuyaExtendedServicesCommandType::MODULE_RESET: {
          ESP_LOGE(TAG, "EXTENDED_SERVICES::MODULE_RESET is not handled");
          break;
        }
        case TuyaExtendedServicesCommandType::UPDATE_IN_PROGRESS: {
          ESP_LOGE(TAG, "EXTENDED_SERVICES::UPDATE_IN_PROGRESS is not handled");
          break;
        }
        default:
          ESP_LOGE(TAG, "Invalid extended services subcommand (0x%02X) received", subcommand);
      }
      break;
    }
    default:
      ESP_LOGE(TAG, "Invalid command (0x%02X) received", command);
  }
}

void TuyaUART::handle_datapoints_(const uint8_t *buffer, size_t len, bool ack) {
  if (ack) {
    // own pass: a datapoint that cannot be parsed ends the parsing below, the ones after it are acknowledged too
    const uint8_t *entry = buffer;
    size_t rest = len;
    while (rest >= 4) {
      const size_t entry_size = 4 + encode_uint16(entry[2], entry[3]);
      // a datapoint cut off by the end of the frame is not acknowledged
      if (entry_size > rest)
        break;
      this->send_command_(
          TuyaCommand{.cmd = TuyaCommandType::DATAPOINT_REPORT_ASYNC, .payload = std::vector<uint8_t>{entry[0], 0x00}});
      entry += entry_size;
      rest -= entry_size;
    }
  }
  while (len >= 4) {
    TuyaDatapoint datapoint{};
    datapoint.id = buffer[0];
    datapoint.type = (TuyaDatapointType) buffer[1];
    datapoint.value_uint = 0;

    size_t data_size = encode_uint16(buffer[2], buffer[3]);
    const uint8_t *data = buffer + 4;
    size_t data_len = len - 4;
    if (data_size > data_len) {
      ESP_LOGW(TAG, "Datapoint %u is truncated and cannot be parsed (%zu > %zu)", datapoint.id, data_size, data_len);
      return;
    }

    datapoint.len = data_size;

    switch (datapoint.type) {
      case TuyaDatapointType::RAW:
        datapoint.value_raw = std::vector<uint8_t>(data, data + data_size);
        {
          char hex_buf[format_hex_pretty_size(MAX_DATAPOINT_LOG_BYTES)];
          ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id,
                   format_hex_pretty_to(hex_buf, datapoint.value_raw.data(), datapoint.value_raw.size()));
        }
        break;
      case TuyaDatapointType::BOOLEAN:
        if (data_size != 1) {
          ESP_LOGW(TAG, "Datapoint %u has bad boolean len %zu", datapoint.id, data_size);
          return;
        }
        datapoint.value_bool = data[0];
        ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, ONOFF(datapoint.value_bool));
        break;
      case TuyaDatapointType::INTEGER:
        if (data_size != 4) {
          ESP_LOGW(TAG, "Datapoint %u has bad integer len %zu", datapoint.id, data_size);
          return;
        }
        datapoint.value_uint = encode_uint32(data[0], data[1], data[2], data[3]);
        ESP_LOGD(TAG, "Datapoint %u update to %d", datapoint.id, datapoint.value_int);
        break;
      case TuyaDatapointType::STRING:
        datapoint.value_string = std::string(reinterpret_cast<const char *>(data), data_size);
        ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, datapoint.value_string.c_str());
        break;
      case TuyaDatapointType::ENUM:
        if (data_size != 1) {
          ESP_LOGW(TAG, "Datapoint %u has bad enum len %zu", datapoint.id, data_size);
          return;
        }
        datapoint.value_enum = data[0];
        ESP_LOGD(TAG, "Datapoint %u update to %d", datapoint.id, datapoint.value_enum);
        break;
      case TuyaDatapointType::BITMASK:
        switch (data_size) {
          case 1:
            datapoint.value_bitmask = encode_uint32(0, 0, 0, data[0]);
            break;
          case 2:
            datapoint.value_bitmask = encode_uint32(0, 0, data[0], data[1]);
            break;
          case 4:
            datapoint.value_bitmask = encode_uint32(data[0], data[1], data[2], data[3]);
            break;
          default:
            ESP_LOGW(TAG, "Datapoint %u has bad bitmask len %zu", datapoint.id, data_size);
            return;
        }
        ESP_LOGD(TAG, "Datapoint %u update to %#08" PRIX32, datapoint.id, datapoint.value_bitmask);
        break;
      default:
        ESP_LOGW(TAG, "Datapoint %u has unknown type %#02hhX", datapoint.id, static_cast<uint8_t>(datapoint.type));
        return;
    }

    len -= data_size + 4;
    buffer = data + data_size;

    this->handle_datapoint_(datapoint);
  }
}

void TuyaUART::send_raw_command_(TuyaCommand command) {
  uint8_t len_hi = (uint8_t) (command.payload.size() >> 8);
  uint8_t len_lo = (uint8_t) (command.payload.size() & 0xFF);
  uint8_t version = 0;

  this->last_command_timestamp_ = millis();
  switch (command.cmd) {
    case TuyaCommandType::HEARTBEAT:
      this->expected_response_ = TuyaCommandType::HEARTBEAT;
      break;
    case TuyaCommandType::PRODUCT_QUERY:
      this->expected_response_ = TuyaCommandType::PRODUCT_QUERY;
      break;
    case TuyaCommandType::CONF_QUERY:
      this->expected_response_ = TuyaCommandType::CONF_QUERY;
      break;
    case TuyaCommandType::DATAPOINT_DELIVER:
    case TuyaCommandType::DATAPOINT_QUERY:
      this->expected_response_ = TuyaCommandType::DATAPOINT_REPORT_ASYNC;
      break;
    default:
      break;
  }

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex_buf[format_hex_pretty_size(MAX_DATAPOINT_LOG_BYTES)];
  ESP_LOGV(TAG, "Sending Tuya: CMD=0x%02X VERSION=%u DATA=[%s] INIT_STATE=%u", static_cast<uint8_t>(command.cmd),
           version, format_hex_pretty_to(hex_buf, command.payload.data(), command.payload.size()),
           static_cast<uint8_t>(this->init_state_));
#endif

  this->write_array({0x55, 0xAA, version, (uint8_t) command.cmd, len_hi, len_lo});
  if (!command.payload.empty())
    this->write_array(command.payload.data(), command.payload.size());

  const uint8_t header_sum = 0x55 + 0xAA + (uint8_t) command.cmd + len_hi + len_lo;
  this->write_byte(sum_bytes(command.payload.data(), command.payload.size(), header_sum));
}

void TuyaUART::process_command_queue_() {
  uint32_t now = millis();
  uint32_t delay = now - this->last_command_timestamp_;

  if (now - this->last_rx_char_timestamp_ > RX_FRAME_TIMEOUT) {
    this->rx_message_.clear();
  }

  if (this->expected_response_.has_value() && delay > RESPONSE_TIMEOUT) {
    this->expected_response_.reset();
    if (!this->resent_) {
      // stays at the front of the queue and is sent once more
      this->resent_ = true;
    } else {
      this->resent_ = false;
      const TuyaCommandType dropped = this->command_queue_.front().cmd;
      this->command_queue_.erase(command_queue_.begin());
      if (this->init_state_ != TuyaInitState::INIT_DONE) {
        if (dropped == init_step_query(this->init_state_)) {
          // without a heartbeat the step is tried again by retry_init_
          this->init_failed_ = this->heartbeat_;
          this->log_init_failure_();
        } else {
          ESP_LOGD(TAG, "Command 0x%02X got no reply at init_state %u", static_cast<uint8_t>(dropped),
                   static_cast<uint8_t>(this->init_state_));
        }
      } else if (dropped == TuyaCommandType::DATAPOINT_DELIVER) {
        ESP_LOGW(TAG, "Datapoint command got no report from the MCU, dropped");
      } else if (dropped == TuyaCommandType::HEARTBEAT) {
#ifndef USE_TUYA_MCU_WATCHDOG
        // once until the next reply; with the watchdog the misses are counted below
        if (!this->heartbeat_missed_) {
          this->heartbeat_missed_ = true;
          ESP_LOGW(TAG, "MCU does not answer heartbeats");
        }
#endif
      } else {
        ESP_LOGD(TAG, "Command 0x%02X got no reply, dropped", static_cast<uint8_t>(dropped));
      }
#ifdef USE_TUYA_MCU_WATCHDOG
      if (dropped == TuyaCommandType::HEARTBEAT)
        this->note_missed_heartbeat_();
#endif
    }
  }

#ifdef USE_TUYA_MCU_WATCHDOG_RESET
  // nothing is written while TX is held low
  if (this->tx_break_active_)
    return;
#endif

  // Left check of delay since last command in case there's ever a command sent by calling send_raw_command_ directly
  if (delay > COMMAND_DELAY && !this->command_queue_.empty() && this->rx_message_.empty() &&
      !this->expected_response_.has_value()) {
    this->send_raw_command_(command_queue_.front());
    if (!this->expected_response_.has_value())
      this->command_queue_.erase(command_queue_.begin());
  }
}

#ifdef USE_TUYA_MCU_WATCHDOG
void TuyaUART::note_missed_heartbeat_() {
  this->heartbeat_answers_ = 0;
  if (this->watchdog_missed_heartbeats_ == 0 || ++this->heartbeat_misses_ < this->watchdog_missed_heartbeats_)
    return;
  this->heartbeat_misses_ = 0;
#ifdef USE_TUYA_MCU_WATCHDOG_RESET
  if (this->watchdog_reset_time_ != 0) {
    if (this->tx_break_unconfirmed_) {
      // resets that do not bring the MCU back end in a reboot, under the rule of action: reboot
      if (this->tx_break_failures_ < MAX_UNANSWERED_RESETS)
        this->tx_break_failures_++;
      if (this->tx_break_failures_ == MAX_UNANSWERED_RESETS && this->working_mode_received_) {
        ESP_LOGE(TAG, "MCU left %u heartbeats unanswered after %u resets, rebooting", this->watchdog_missed_heartbeats_,
                 MAX_UNANSWERED_RESETS);
        App.safe_reboot();
        return;
      }
      ESP_LOGE(TAG, "MCU left %u heartbeats unanswered after a reset, holding TX low again",
               this->watchdog_missed_heartbeats_);
    } else {
      ESP_LOGW(TAG, "MCU left %u heartbeats unanswered, holding TX low", this->watchdog_missed_heartbeats_);
    }
    this->start_tx_break_();
    return;
  }
#endif
  // Tuya's module firmware does not reset itself before the working mode reply
  if (!this->working_mode_received_) {
    // once until the next answered heartbeat: an MCU that is not there would repeat it without end
    if (!this->unarmed_misses_logged_) {
      this->unarmed_misses_logged_ = true;
      ESP_LOGW(TAG, "MCU left %u heartbeats unanswered before the working mode reply, not rebooting",
               this->watchdog_missed_heartbeats_);
    }
    return;
  }
  ESP_LOGE(TAG, "MCU left %u heartbeats unanswered, rebooting", this->watchdog_missed_heartbeats_);
  App.safe_reboot();
}

#ifdef USE_TUYA_MCU_WATCHDOG_RESET
void TuyaUART::start_tx_break_() {
  const uart::UARTFlushResult flushed = this->flush();
  if (flushed == uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT ||
      flushed == uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED) {
    // a break would cut into the bytes still sent; the next misses try again
    ESP_LOGW(TAG, "UART bus did not finish sending, MCU reset skipped");
    return;
  }
  if (!this->parent_->set_tx_break(true)) {
    // as with action: reboot, no reboot before the working mode reply
    if (!this->working_mode_received_) {
      ESP_LOGE(TAG, "UART bus cannot hold TX low");
      return;
    }
    ESP_LOGE(TAG, "UART bus cannot hold TX low, rebooting");
    App.safe_reboot();
    return;
  }
  // not the time of this loop pass: the flush above took a part of it and the break would end early
  this->tx_break_start_ = millis();
  this->tx_break_active_ = true;
  this->tx_break_unconfirmed_ = true;
  this->mcu_resets_++;
  // until the MCU answers a heartbeat again
  this->status_set_warning(LOG_STR("MCU does not answer heartbeats"));
}

void TuyaUART::end_tx_break_() {
  this->tx_break_active_ = false;
  if (!this->parent_->set_tx_break(false)) {
    // another break cannot bring the bus back
    ESP_LOGE(TAG, "UART bus did not release TX after the MCU reset, rebooting");
    App.safe_reboot();
    return;
  }
  // bytes received during the break are not replies to anything sent after it
  this->discard_available();
  this->rx_message_.clear();
  this->send_empty_command_(TuyaCommandType::DATAPOINT_QUERY);
}
#endif  // USE_TUYA_MCU_WATCHDOG_RESET
#endif  // USE_TUYA_MCU_WATCHDOG

void TuyaUART::send_command_(TuyaCommand command) {
  command_queue_.push_back(std::move(command));
  process_command_queue_();
}

void TuyaUART::send_empty_command_(TuyaCommandType command) {
  send_command_(TuyaCommand{.cmd = command, .payload = std::vector<uint8_t>{}});
}

void TuyaUART::send_datapoint_command(uint8_t datapoint_id, TuyaDatapointType datapoint_type, const uint8_t *data,
                                      size_t len) {
  std::vector<uint8_t> buffer;
  buffer.reserve(4 + len);
  buffer.push_back(datapoint_id);
  buffer.push_back(static_cast<uint8_t>(datapoint_type));
  buffer.push_back(len >> 8);
  buffer.push_back(len >> 0);
  buffer.insert(buffer.end(), data, data + len);

  this->send_command_(TuyaCommand{.cmd = TuyaCommandType::DATAPOINT_DELIVER, .payload = std::move(buffer)});
}

void TuyaUART::set_status_pin_() {
  bool is_network_ready = network_is_connected() && remote_is_connected();
  this->status_pin_->digital_write(is_network_ready);
}

uint8_t TuyaUART::get_wifi_status_code_() {
  uint8_t status = 0x02;

  if (network_is_connected()) {
    status = 0x03;

    // Protocol version 3 also supports specifying when connected to "the cloud"
    if (this->protocol_version_ >= 0x03 && remote_is_connected()) {
      status = 0x04;
    }
  } else {
#ifdef USE_CAPTIVE_PORTAL
    if (captive_portal::global_captive_portal != nullptr && captive_portal::global_captive_portal->is_active()) {
      status = 0x01;
    }
#endif
  };

  return status;
}

uint8_t TuyaUART::get_wifi_rssi_() {
#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr)
    return wifi::global_wifi_component->wifi_rssi();
#endif

  return 0;
}

void TuyaUART::send_wifi_status_() {
  uint8_t status = this->get_wifi_status_code_();

  if (status == this->wifi_status_) {
    return;
  }

  ESP_LOGD(TAG, "Sending WiFi Status");
  this->wifi_status_ = status;
  this->send_command_(TuyaCommand{.cmd = TuyaCommandType::WIFI_STATE, .payload = std::vector<uint8_t>{status}});
}

#ifdef USE_TIME
void TuyaUART::register_local_time_sync_callback_() {
  if (this->time_sync_callback_registered_) {
    return;
  }
  this->time_id_->add_on_time_sync_callback([this] { this->send_local_time_(); });
  this->time_sync_callback_registered_ = true;
}

void TuyaUART::check_force_time_sync_() {
  ESPTime now = this->time_id_->now();
  if (!now.is_valid() || now.minute == this->last_force_time_sync_minute_)
    return;
  this->last_force_time_sync_minute_ = now.minute;
  this->send_local_time_(true);
}

void TuyaUART::send_local_time_(bool periodic) {
  std::vector<uint8_t> payload;
  ESPTime now = this->time_id_->now();
  if (now.is_valid()) {
    uint8_t year = now.year - 2000;
    uint8_t month = now.month;
    uint8_t day_of_month = now.day_of_month;
    uint8_t hour = now.hour;
    uint8_t minute = now.minute;
    uint8_t second = now.second;
    // Tuya days starts from Monday, esphome uses Sunday as day 1
    uint8_t day_of_week = now.day_of_week - 1;
    if (day_of_week == 0) {
      day_of_week = 7;
    }
    // The forced send repeats every minute, so it stays out of the debug log
    if (periodic) {
      ESP_LOGV(TAG, "Sending local time");
    } else {
      ESP_LOGD(TAG, "Sending local time");
    }
    payload = std::vector<uint8_t>{0x01, year, month, day_of_month, hour, minute, second, day_of_week};
  } else {
    // By spec we need to notify MCU that the time was not obtained if this is a response to a query
    ESP_LOGW(TAG, "Sending missing local time");
    payload = std::vector<uint8_t>{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  }
  this->send_command_(TuyaCommand{.cmd = TuyaCommandType::LOCAL_TIME_QUERY, .payload = payload});
}
void TuyaUART::send_gmt_time_() {
  std::vector<uint8_t> payload;
  ESPTime now = this->time_id_->utcnow();
  if (now.is_valid()) {
    uint8_t year = now.year - 2000;
    uint8_t month = now.month;
    uint8_t day_of_month = now.day_of_month;
    uint8_t hour = now.hour;
    uint8_t minute = now.minute;
    uint8_t second = now.second;
    ESP_LOGD(TAG, "Sending gmt time");
    payload = std::vector<uint8_t>{0x01, year, month, day_of_month, hour, minute, second};
  } else {
    // By spec we need to notify MCU that the time was not obtained if this is a response to a query
    ESP_LOGW(TAG, "Sending missing gmt time");
    payload = std::vector<uint8_t>{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  }
  this->send_command_(TuyaCommand{.cmd = TuyaCommandType::GMT_TIME_QUERY, .payload = payload});
}
#endif

TuyaInitState TuyaUART::get_init_state() { return this->init_state_; }

}  // namespace esphome::tuya
