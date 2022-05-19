#include "pn7150.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pn7150 {

static const char *const TAG = "pn7150";

void PN7150::setup() {
  this->irq_pin_->setup();
  this->ven_pin_->setup();

  if (!this->reset_core_(true, true)) {
    ESP_LOGE(TAG, "Failed to reset NCI core");
    this->mark_failed();
    return;
  }

  std::vector<uint8_t> data;

  if (!this->init_core_(data)) {
    ESP_LOGE(TAG, "Failed to init NCI core");
    this->mark_failed();
    return;
  }

  this->version_[0] = data[17 + data[8]];
  this->version_[1] = data[18 + data[8]];
  this->version_[2] = data[19 + data[8]];

  if (this->version_[0] == 0x08) {
    this->generation_ = 1;
  } else if (this->version_[0] == 0x10) {
    this->generation_ = 2;
  }

  ESP_LOGD(TAG, "Core Version: 0x%2X", this->version_[0]);
  ESP_LOGD(TAG, "Firmware version: 0x%02X.0x%02X", this->version_[1], this->version_[2]);
  ESP_LOGD(TAG, "Generation: %d", this->generation_);

  if (!this->send_config_()) {
    ESP_LOGE(TAG, "Failed to send config");
    this->mark_failed();
    return;
  }
}

bool PN7150::reset_core_(bool reset_config, bool power) {
  if (power) {
    this->ven_pin_->digital_write(true);
    delay(2);
    this->ven_pin_->digital_write(false);
    delay(2);
    this->ven_pin_->digital_write(true);
    delay(20);
  }

  std::vector<uint8_t> response;

  std::vector<uint8_t> write_data;

  if (reset_config)
    write_data.push_back(0x01);
  else
    write_data.push_back(0x00);

  if (!this->write_and_read_(NCI_CORE_GID, NCI_CORE_RESET_OID, write_data, response, 20)) {
    ESP_LOGE(TAG, "Error sending reset command");
    return false;
  }

  if (response[3] != STATUS_OK) {
    ESP_LOGE(TAG, "Invalid reset response");
    return false;
  }
  this->state_ = PN7150State::INIT;

  return true;
}

bool PN7150::init_core_(std::vector<uint8_t> &response, PN7150State next_state) {
  if (!this->write_and_read_(NCI_CORE_GID, NCI_CORE_INIT_OID, {}, response)) {
    ESP_LOGE(TAG, "Error sending initialise command");
    return false;
  }

  if (response[3] != STATUS_OK) {
    ESP_LOGE(TAG, "Invalid initialise response");
    return false;
  }
  this->state_ = next_state;
  return true;
}

bool PN7150::send_config_() {
  uint8_t currentTS[32] = __TIMESTAMP__;

  std::vector<uint8_t> response;

  if (!this->write_and_read_(NCI_CORE_GID, NCI_CORE_SET_CONFIG_OID, CORE_CONFIG, sizeof(CORE_CONFIG), response)) {
    ESP_LOGE(TAG, "Error sending core config");
    return false;
  }

  response.clear();

  // if (!this->write_and_read_(0x2f, 0x00, {0x01}, response)) {
  //   ESP_LOGE(TAG, "Error sending standby config");
  //   return false;
  // }

  // if (response[3] != 0x00) {
  //   ESP_LOGE(TAG, "Incorrect standby config response");
  //   return false;
  // }

  // response.clear();

  // std::vector<uint8_t> read_ts = {0x01, 0xA0, 0x14};
  // if (this->generation_ == 1)
  //   read_ts[2] = 0x0F;
  // if (!this->write_and_read_(0x20, 0x03, read_ts, response)) {
  //   ESP_LOGE(TAG, "Error sending read timestamp config");
  //   return false;
  // }

  // if (response[3] != 0x00) {
  //   ESP_LOGE(TAG, "Incorrect timestamp response");
  //   return false;
  // }

  // if (!memcmp(&response[8], currentTS, sizeof(currentTS))) {
  //   // No need to update as settings could not have changed since last setup.
  // } else {
  // response.clear();

  if (!this->write_and_read_(NCI_CORE_GID, NCI_CORE_SET_CONFIG_OID, CORE_CONF_EXTENSION, sizeof(CORE_CONF_EXTENSION),
                             response)) {
    ESP_LOGE(TAG, "Error sending core config extension");
    return false;
  }

  response.clear();

  if (!this->write_and_read_(NCI_CORE_GID, NCI_CORE_SET_CONFIG_OID, CLOCK_CONFIG, sizeof(CLOCK_CONFIG), response)) {
    ESP_LOGE(TAG, "Error sending clock config");
    return false;
  }

  response.clear();

  if (!this->write_and_read_(NCI_CORE_GID, NCI_CORE_SET_CONFIG_OID, RF_TVDD_CONFIG, sizeof(RF_TVDD_CONFIG), response)) {
    ESP_LOGE(TAG, "Error sending RF TVDD config");
    return false;
  }

  response.clear();

  if (!this->write_and_read_(NCI_CORE_GID, NCI_CORE_SET_CONFIG_OID, RF_CONF, sizeof(RF_CONF), response, 20)) {
    ESP_LOGE(TAG, "Error sending RF config");
    return false;
  }

  // response.clear();

  // std::vector<uint8_t> write_ts = {0x01, 0xA0, 0x14, 0x20};
  // write_ts.resize(36);

  // if (this->generation_ == 1)
  //   write_ts[3] = 0x0F;
  // memcpy(&write_ts[4], currentTS, sizeof(currentTS));
  // if (!this->write_and_read_(0x20, 0x02, write_ts, response)) {
  //   ESP_LOGE(TAG, "Error sending TS config");
  //   return false;
  // }

  // if (response[3] != 0x00 || response[4] != 0x00) {
  //   ESP_LOGE(TAG, "Incorrect TS config response");
  //   return false;
  // }

  // response.clear();
  // }

  if (!this->reset_core_(false, false))
    return false;

  if (!this->init_core_(response))
    return false;

  this->state_ = PN7150State::SET_MODE;

  return true;
}

bool PN7150::set_mode_(PN7150Mode mode) {
  if (mode == PN7150Mode::READ_WRITE) {
    std::vector<uint8_t> response;

    std::vector<uint8_t> write_data;
    write_data.resize(1 + sizeof(READ_WRITE_MODE));
    write_data[0] = sizeof(READ_WRITE_MODE) / 3;
    memcpy(&write_data[1], READ_WRITE_MODE, sizeof(READ_WRITE_MODE));

    if (!this->write_and_read_(RF_GID, RF_DISCOVER_MAP_OID, write_data, response, 10)) {
      ESP_LOGE(TAG, "Error sending discover map");
      return false;
    }

    this->state_ = PN7150State::DISCOVERY;

    return true;
  }
  return false;
}

bool PN7150::start_discovery_(PN7150Mode mode) {
  std::vector<uint8_t> response;

  uint8_t length = sizeof(DISCOVERY_READ_WRITE);
  std::vector<uint8_t> write_data = std::vector<uint8_t>((length * 2) + 1);
  write_data[0] = length;
  for (uint8_t i = 0; i < length; i++) {
    write_data[(i * 2) + 1] = DISCOVERY_READ_WRITE[i];
    write_data[(i * 2) + 2] = 0x01;
  }

  if (!this->write_and_read_(RF_GID, RF_DISCOVER_OID, write_data, response)) {
    ESP_LOGE(TAG, "Error sending discovery");
    return false;
  }

  this->state_ = PN7150State::WAITING_FOR_TAG;

  return true;
}

bool PN7150::deactivate_(uint8_t type) {
  std::vector<uint8_t> response;
  if (!this->write_and_read_(RF_GID, RF_DEACTIVATE_OID, {type}, response, 20)) {
    ESP_LOGE(TAG, "Error sending deactivate");
    return false;
  }

  this->state_ = PN7150State::DEACTIVATING;
  return true;
}

std::unique_ptr<nfc::NfcTag> PN7150::build_tag_(uint8_t mode_tech, const std::vector<uint8_t> &data) {
  switch (mode_tech) {
    case (MODE_POLL | TECH_PASSIVE_NFCA): {
      uint8_t uid_length = data[2];
      std::vector<uint8_t> uid(data.begin() + 3, data.begin() + 3 + uid_length);
      return make_unique<nfc::NfcTag>(uid, nfc::MIFARE_CLASSIC);
    }
  }
  return nullptr;
}

void PN7150::select_tag_() {
  std::vector<uint8_t> response;
  uint8_t interface = INTF_UNDETERMINED;
  switch (this->current_protocol_) {
    case PROT_ISODEP:
      interface = INTF_ISODEP;
      break;
    case PROT_NFCDEP:
      interface = INTF_NFCDEP;
      break;
    case PROT_MIFARE:
      interface = INTF_TAGCMD;
      break;
    default:
      interface = INTF_FRAME;
      break;
  }
  if (!this->write_and_read_(RF_GID, RF_DISCOVER_SELECT_OID, {0x01, this->current_protocol_, interface}, response)) {
    ESP_LOGE(TAG, "Error sending discover select");
    return;
  }
  this->state_ = PN7150State::SELECTING;
}

bool PN7150::presence_check_() {
  std::vector<uint8_t> response;
  switch (this->current_protocol_) {
    case PROT_MIFARE: {
      this->deactivate_(DEACTIVATION_TYPE_SLEEP);
      this->next_function_ = std::bind(&PN7150::select_tag_, this);
    }
    default:
      break;
  }
  return false;
}

void PN7150::dump_config() {
  ESP_LOGCONFIG(TAG, "PN7150:");
  LOG_I2C_DEVICE(this);
}

void PN7150::loop() {
  switch (this->state_) {
    case PN7150State::SET_MODE:
      this->set_mode_(PN7150Mode::READ_WRITE);
      return;
    case PN7150State::DISCOVERY:
      this->start_discovery_(PN7150Mode::READ_WRITE);
      return;
    case PN7150State::WAITING_FOR_REMOVAL:
      this->presence_check_();
    // These cases are waiting for NOTIFICATION messages.
    case PN7150State::WAITING_FOR_TAG:
    case PN7150State::DEACTIVATING:
    case PN7150State::SELECTING:
      break;
    default:
      return;
  }

  if (!this->irq_pin_->digital_read()) {
    // this->current_uid_ = {};
    return;  // No data to read
  }

  std::vector<uint8_t> response;
  if (!this->read_data_(response, 5, false)) {
    // No data
    // this->current_uid_ = {};
    return;
  }

  uint8_t mt = response[0] & MT_MASK;
  uint8_t gid = response[0] & GID_MASK;
  uint8_t oid = response[1] & OID_MASK;

  if (mt == MT_RESPONSE) {
    // Unimplemented
  } else if (mt == MT_NOTIFICATION) {
    if (gid == RF_GID) {
      switch (oid) {
        case RF_INTF_ACTIVATED_OID: {
          // Card detected?
          uint8_t interface = response[4];
          uint8_t protocol = response[5];
          uint8_t mode_tech = response[6];
          this->current_protocol_ = protocol;
          auto tag = this->build_tag_(mode_tech, std::vector<uint8_t>(response.begin() + 10, response.end()));
          if (tag->get_uid().size() == this->current_uid_.size()) {
            bool same_uid = true;
            for (uint8_t i = 0; i < this->current_uid_.size(); i++)
              same_uid &= this->current_uid_[i] == tag->get_uid()[i];
            if (same_uid) {
              this->state_ = PN7150State::WAITING_FOR_REMOVAL;
              return;
            }
          }
          if (tag != nullptr) {
            for (auto *trigger : this->triggers_ontag_) {
              trigger->process(tag);
            }
            this->current_uid_ = tag->get_uid();
          }
          this->state_ = PN7150State::NONE;
          this->set_timeout(200, [this]() { this->state_ = PN7150State::WAITING_FOR_REMOVAL; });
          return;
        }
        case RF_DISCOVER_OID: {
          // Multiple detections?
          break;
        }
        case RF_DEACTIVATE_OID: {
          if (this->next_function_ != nullptr) {
            this->next_function_();
            this->next_function_ = nullptr;
          }
          break;
          // if (response[3] == DEACTIVATION_TYPE_SLEEP) {
          // }
        }
      }
    } else if (gid == NCI_CORE_GID) {
      if (oid == NCI_CORE_GENERIC_ERROR_OID) {
        switch (response[3]) {
          case DISCOVERY_TARGET_ACTIVATION_FAILED:
            // Tag removed
            this->current_uid_ = {};
            if (!this->deactivate_(DEACTIVATION_TYPE_IDLE)) {
              ESP_LOGE(TAG, "Error deactivating");
            }
            this->state_ = PN7150State::DISCOVERY;
            break;
        }
      }
    }
  }
}

bool PN7150::write_and_read_(uint8_t gid, uint8_t oid, const std::vector<uint8_t> &data, std::vector<uint8_t> &response,
                             uint16_t timeout, bool warn) {
  return this->write_and_read_(gid, oid, data.data(), data.size(), response, timeout, warn);
}

bool PN7150::write_and_read_(uint8_t gid, uint8_t oid, const uint8_t *data, const uint8_t len,
                             std::vector<uint8_t> &response, uint16_t timeout, bool warn) {
  if (gid != (gid & GID_MASK)) {
    ESP_LOGE(TAG, "Invalid GID");
    return false;
  }
  if (oid != (oid & OID_MASK)) {
    ESP_LOGE(TAG, "Invalid OID");
    return false;
  }

  std::vector<uint8_t> write_data = std::vector<uint8_t>(len + 3);
  write_data[0] = MT_COMMAND | (gid & GID_MASK);
  write_data[1] = oid & OID_MASK;
  write_data[2] = len;

  if (len > 0) {
    for (uint8_t i = 0; i < len; i++) {
      write_data[i + 3] = data[i];
    }
  }

  if (!this->write_data_(write_data)) {
    ESP_LOGE(TAG, "Error sending data");
    return false;
  }

  if (!this->read_data_(response, timeout)) {
    if (warn)
      ESP_LOGE(TAG, "Error reading response");
    return false;
  }

  if ((response[0] & GID_MASK) != gid || (response[1] & OID_MASK) != oid) {
    ESP_LOGE(TAG, "Incorrect response");
    return false;
  }

  if (response[3] != STATUS_OK) {
    ESP_LOGE(TAG, "Error in response: %d", response[3]);
    return false;
  }

  return true;
}

bool PN7150::write_data_(const std::vector<uint8_t> &data) {
  return this->write(data.data(), data.size()) == i2c::ERROR_OK;
}

bool PN7150::wait_for_irq_(uint16_t timeout, bool warn) {
  uint32_t start_time = millis();
  uint32_t count = 0;
  while (true) {
    if (this->irq_pin_->digital_read()) {
      // ESP_LOGD(TAG, "IRQ pin is high. %u lows", count);
      return true;
    }
    count++;
    // ESP_LOGD(TAG, "IRQ pin is low");

    if (millis() - start_time > timeout) {
      if (warn)
        ESP_LOGV(TAG, "Timed out waiting for data! (This can be normal)");
      // ESP_LOGD(TAG, "IRQ pin is low still. %u lows", count);
      return false;
    }
  }
}

bool PN7150::read_data_(std::vector<uint8_t> &data, uint16_t timeout, bool warn) {
  if (!this->wait_for_irq_(timeout, warn)) {
    return false;
  }

  data.resize(3);
  if (this->read(data.data(), 3) != i2c::ERROR_OK) {
    ESP_LOGV(TAG, "No response header");
    return false;
  }
  uint8_t length = data[2];
  if (length > 0) {
    data.resize(length + 3);
    uint8_t start = 0;

    while (start < length) {
      if (!this->wait_for_irq_(timeout)) {
        return false;
      }

      uint8_t read_length = std::min(length - start, 10);
      if (this->read(data.data() + 3 + start, read_length) != i2c::ERROR_OK) {
        ESP_LOGV(TAG, "No response data");
        return false;
      }
      start += read_length;
    }
  }
  return true;
}

}  // namespace pn7150
}  // namespace esphome
