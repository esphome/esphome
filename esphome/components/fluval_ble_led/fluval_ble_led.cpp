#include "fluval_ble_led.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/time.h"

#ifdef USE_ESP32

namespace esphome {
namespace fluval_ble_led {

static const char *const TAG = "fluval_ble_led";

void FluvalBleLed::dump_config() {
  ESP_LOGCONFIG(TAG, "Fluval LED");
  ESP_LOGCONFIG(TAG, "  Address: %s", this->parent_->address_str().c_str());
  ESP_LOGCONFIG(TAG, "  Number of channels: %i", this->number_of_channels_);
}

void FluvalBleLed::setup() { ESP_LOGD(TAG, "Setup called"); }

std::string FluvalBleLed::pkt_to_hex_(const uint8_t *data, uint16_t len) {
  char buffer[len * 3];
  memset(buffer, 0, len * 3);
  for (int i = 0; i < len; i++)
    sprintf(&buffer[i * 3], "%02x ", data[i]);
  std::string ret = buffer;
  return ret;
}

uint8_t FluvalBleLed::get_crc_(const uint8_t *data, uint16_t len) {
  uint8_t crc = 0x00;
  for (int i = 0; i < len; i++) {
    crc = (crc ^ data[i]);
  }
  ESP_LOGV(TAG, "CRC: 0x%02x", crc);
  return crc;
}

void FluvalBleLed::add_crc_to_vector_(std::vector<uint8_t> &data) {
  uint8_t crc = 0x00;
  for (auto &i : data) {
    crc = (i ^ crc);
  }

  data.push_back(crc);
}

void FluvalBleLed::decrypt_(const uint8_t *data, uint16_t len, uint8_t *decrypted) {
  uint8_t key = data[0] ^ data[2];
  ESP_LOGV(TAG, "Found decyption key: 0x%02x", key);

  for (int i = 3; i < len; i++) {
    decrypted[i - 3] = (data[i] ^ key);
  }
}

void FluvalBleLed::encrypt_(std::vector<uint8_t> &data) {
  for (auto &i : data) {
    i = (i ^ 0xE);
  }

  uint8_t secret = (data.size() + 1) ^ 0x54;
  const uint8_t header[3] = {0x54, secret, 0x5a};
  data.insert(data.begin(), header, header + 3);
}

void FluvalBleLed::update_channel(uint8_t channel, float value) {
  switch (channel) {
    case 1:
      this->status_.channel1 = value;
      break;
    case 2:
      this->status_.channel2 = value;
      break;
    case 3:
      this->status_.channel3 = value;
      break;
    case 4:
      this->status_.channel4 = value;
      break;
    case 5:
      this->status_.channel5 = value;
      break;
    default:
      break;
  }

  uint8_t channel1byte1 = static_cast<uint8_t>((static_cast<uint16_t>(this->status_.channel1) >> 8));
  uint8_t channel1byte2 = static_cast<uint8_t>((static_cast<uint16_t>(this->status_.channel1) & 0xFF));

  uint8_t channel2byte1 = static_cast<uint8_t>((static_cast<uint16_t>(this->status_.channel2) >> 8));
  uint8_t channel2byte2 = static_cast<uint8_t>((static_cast<uint16_t>(this->status_.channel2) & 0xFF));

  uint8_t channel3byte1 = static_cast<uint8_t>((static_cast<uint16_t>(this->status_.channel3) >> 8));
  uint8_t channel3byte2 = static_cast<uint8_t>((static_cast<uint16_t>(this->status_.channel3) & 0xFF));

  uint8_t channel4byte1 = static_cast<uint8_t>((static_cast<uint16_t>(this->status_.channel4) >> 8));
  uint8_t channel4byte2 = static_cast<uint8_t>((static_cast<uint16_t>(this->status_.channel4) & 0xFF));

  if (this->number_of_channels_ == 5) {
    // 5 Channel LED
    uint8_t channel5byte1 = static_cast<uint8_t>((static_cast<uint16_t>(this->status_.channel5) >> 8));
    uint8_t channel5byte2 = static_cast<uint8_t>((static_cast<uint16_t>(this->status_.channel5) & 0xFF));

    std::vector<uint8_t> led{0x68,          0x04,          channel1byte1, channel1byte2, channel2byte1, channel2byte2,
                             channel3byte1, channel3byte2, channel4byte1, channel4byte2, channel5byte1, channel5byte2};
    this->send_packet_(led);
  } else {
    // 4 Channel LED
    std::vector<uint8_t> led{0x68,          0x04,          channel1byte1, channel1byte2, channel2byte1,
                             channel2byte2, channel3byte1, channel3byte2, channel4byte1, channel4byte2};
    this->send_packet_(led);
  }

  std::vector<uint8_t> update{0x68, 0x05};
  this->send_packet_(update);
}

void FluvalBleLed::send_packet_(std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "Writing packet original: %s ", this->pkt_to_hex_(data.data(), data.size()).c_str());
  add_crc_to_vector_(data);
  ESP_LOGV(TAG, "Writing packet original + CRC: %s ", this->pkt_to_hex_(data.data(), data.size()).c_str());
  encrypt_(data);
  ESP_LOGV(TAG, "Writing packet encoded: %s to handle %x", this->pkt_to_hex_(data.data(), data.size()).c_str(),
           this->write_handle_);
  esp_err_t status = esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                              this->write_handle_, data.size(), const_cast<uint8_t *>(data.data()),
                                              ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  ESP_LOGV(TAG, "Writing packet result: %s", esp_err_to_name(status));
}

void FluvalBleLed::decode_(const uint8_t *data, uint16_t len) {
  // Not a fluval data packet
  if (data[0] != 0x68) {
    ESP_LOGW(TAG, "Received a packet without fluval header");
    return;
  }

  // Not a read request
  if (data[1] != 0x05) {
    ESP_LOGW(TAG, "Received a packet without fluval read command");
    return;
  }

  // check CRC
  if (this->get_crc_(data, len) != 0x00) {
    ESP_LOGW(TAG, "Received a packet with CRC error");
    return;
  }

  this->status_.mode = data[2];
  this->status_.led_on_off = data[3] == 0x00 ? 0 : 1;

  // Decode channels in manual mode only
  if (this->status_.mode == MANUAL_MODE) {
    this->status_.channel1 = (data[6] << 8) | (data[5] & 0xFF);
    this->status_.channel2 = (data[8] << 8) | (data[7] & 0xFF);
    this->status_.channel3 = (data[10] << 8) | (data[9] & 0xFF);
    this->status_.channel4 = (data[12] << 8) | (data[11] & 0xFF);
    if (this->number_of_channels_ == 5) {
      this->status_.channel5 = (data[14] << 8) | (data[13] & 0xFF);
    }
  } else {
    // Auto and Pro mode do not publish channel information. Set to 0 instead
    // of garbage from the packet
    this->status_.channel1 = 0;
    this->status_.channel2 = 0;
    this->status_.channel3 = 0;
    this->status_.channel4 = 0;
    this->status_.channel5 = 0;
  }

  this->notify_clients_();
}

void FluvalBleLed::notify_clients_() {
  for (auto *client : this->clients_) {
    client->notify();
  }
}

void FluvalBleLed::set_led_state(bool state) {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "Cannot write to BLE characteristic - not connected");
    return;
  }

  if (state) {
    ESP_LOGD(TAG, "Turning LED on");
    std::vector<uint8_t> value{0x68, 0x03, 0x01};
    this->send_packet_(value);
  } else {
    ESP_LOGD(TAG, "Turning LED off");
    std::vector<uint8_t> value{0x68, 0x03, 0x00};
    this->send_packet_(value);
  }
}

void FluvalBleLed::set_mode(uint8_t mode) {
  switch (mode) {
    case 0: {  // manual
      ESP_LOGD(TAG, "Setting mode to manual");
      std::vector<uint8_t> value{0x68, 0x02, 0x00};
      this->send_packet_(value);
      break;
    }
    case 1: {  // auto
      ESP_LOGD(TAG, "Setting mode to auto");
      std::vector<uint8_t> value{0x68, 0x02, 0x01};
      this->send_packet_(value);
      break;
    }
    case 2: {  // pro
      ESP_LOGD(TAG, "Setting mode to pro");
      std::vector<uint8_t> value{0x68, 0x02, 0x02};
      this->send_packet_(value);
      break;
    }
    default:
      break;
  }
}

void FluvalBleLed::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                       esp_ble_gattc_cb_param_t *param) {
  ESP_LOGV(TAG, "GOT GATTC EVENT: %d", event);
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGW(TAG, "Disconnected from Fluval LED. Resetting handshake.");
      this->handshake_step_ = 0;
      break;
    }

    // CHARACTERISTIC SEARCH COMPLETE
    // ==============================
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *chr = this->parent()->get_characteristic(FLUVAL_SERVICE_UUID, FLUVAL_CHARACTERISTIC_READ);
      auto status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
                                                      chr->handle);
      if (status == 0) {
        ESP_LOGD(TAG, "esp_ble_gattc_register_for_notify success, status=%d", status);
      } else {
        ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
      }

      auto *read_handle = this->parent_->get_characteristic(FLUVAL_SERVICE_UUID, FLUVAL_CHARACTERISTIC_READ);
      if (read_handle == nullptr) {
        ESP_LOGE(TAG, "[%s] No read handle found?", this->parent_->address_str().c_str());
      } else {
        ESP_LOGD(TAG, "[%s] Read handle found: %x", this->parent_->address_str().c_str(), read_handle->handle);
        this->read_handle_ = read_handle->handle;
      }

      auto *write_handle = this->parent_->get_characteristic(FLUVAL_SERVICE_UUID, FLUVAL_CHARACTERISTIC_WRITE);
      if (write_handle == nullptr) {
        ESP_LOGE(TAG, "[%s] No write handle found?", this->parent_->address_str().c_str());
      } else {
        ESP_LOGD(TAG, "[%s] Write handle found: %x", this->parent_->address_str().c_str(), write_handle->handle);
        this->write_handle_ = write_handle->handle;
      }

      auto *write_reg_id_handle =
          this->parent_->get_characteristic(FLUVAL_SERVICE_UUID, FLUVAL_CHARACTERISTIC_WRITE_REG_ID);
      if (write_reg_id_handle == nullptr) {
        ESP_LOGE(TAG, "[%s] No write reg id handle found?", this->parent_->address_str().c_str());
      } else {
        ESP_LOGD(TAG, "[%s] Write reg id found: %x", this->parent_->address_str().c_str(), write_reg_id_handle->handle);
        this->write_reg_id_handle_ = write_reg_id_handle->handle;
      }

      auto *read_reg_handle = this->parent_->get_characteristic(FLUVAL_SERVICE_UUID, FLUVAL_CHARACTERISTIC_READ_REG);
      if (read_reg_handle == nullptr) {
        ESP_LOGE(TAG, "[%s] No read reg handle found?", this->parent_->address_str().c_str());
      } else {
        ESP_LOGD(TAG, "[%s] read reg found: %x", this->parent_->address_str().c_str(), read_reg_handle->handle);
        this->read_reg_handle_ = read_reg_handle->handle;
      }

      break;
    }

    // READ CHAR EVENT
    // ===============
    case ESP_GATTC_READ_CHAR_EVT: {
      ESP_LOGD(TAG, "[%s] ESP_GATTC_READ_CHAR_EVT (Received READ)", this->parent_->address_str().c_str());
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGD(TAG, "Error reading char at handle %d, status=%d", param->read.handle, param->read.status);
        break;
      }

      ESP_LOGVV(TAG, "READ CHARS FROM %d with status %d: %s ", param->read.handle, param->read.status,
                this->pkt_to_hex_(param->read.value, param->read.value_len - 1).c_str());
      
      // HANDSHAKE STEP 1
      // ================
      if (this->handshake_step_ == 1) {
        std::vector<uint8_t> header2{0x47};
        ESP_LOGD(TAG, "Writing Header 2 (REG 0x47");
        esp_err_t errb1 = esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                                   this->write_reg_id_handle_, 1, const_cast<uint8_t *>(header2.data()),
                                                   ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
        ESP_LOGD(TAG, "Writing 0x47 RESULT: %s", esp_err_to_name(errb1));
        this->handshake_step_ = 2;
      }

      // HANDSHAKE STEP 3
      // ================
      if (this->handshake_step_ == 3) {   
        this->sync_time();     
        ESP_LOGI(TAG, "Connected to Fluval LED [%s]", this->parent_->address_str().c_str());
        // Sending device read request as last part of the handshake
        std::vector<uint8_t> update{0x68, 0x05};
        this->send_packet_(update);        
        this->handshake_step_ = 4;                
      }

      break;
    }

    // WRITE CHAR EVENT
    // ================
    case ESP_GATTC_WRITE_CHAR_EVT: {
      ESP_LOGD(TAG, "[%s] ESP_GATTC_WRITE_CHAR_EVT (Write confirmed)", this->parent_->address_str().c_str());
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Error writing value to char at handle %d, status=%d", param->write.handle, param->write.status);
        break;
      }

      // HANDSHAKE STEP 0
      // ================
      if (this->handshake_step_ == 0) {
        ESP_LOGD(TAG, "Reading result of Header 1 (REG 0x0F");
        esp_err_t erra2 = esp_ble_gattc_read_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                                  this->read_reg_handle_, ESP_GATT_AUTH_REQ_NONE);
        ESP_LOGD(TAG, "READING 0x0F RESULT: %s", esp_err_to_name(erra2));
        this->handshake_step_ = 1;
      }

      // HANDSHAKE STEP 2
      // ================
      if (this->handshake_step_ == 2) {
        ESP_LOGD(TAG, "Reading result of Header 2 (REG 0x47");
        esp_err_t errb2 = esp_ble_gattc_read_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                                  this->read_reg_handle_, ESP_GATT_AUTH_REQ_NONE);
        ESP_LOGD(TAG, "READING 0x47 RESULT: %s", esp_err_to_name(errb2));
        this->handshake_step_ = 3;
      }
  
      break;
    }  // ESP_GATTC_WRITE_CHAR_EVT

    case ESP_GATTC_NOTIFY_EVT: {
      ESP_LOGVV(TAG, "[%s] ESP_GATTC_NOTIFY_EVT: handle=0x%x, value=0x%x, len=%d", this->parent_->address_str().c_str(),
                param->notify.handle, param->notify.value[0], param->notify.value_len);

      ESP_LOGVV(TAG, "Data Encrypted: %s ", this->pkt_to_hex_(param->notify.value, param->notify.value_len).c_str());

      if (param->notify.value_len <= 3) {
        ESP_LOGE(TAG, "Received packet too small");
        break;
      }

      uint8_t decrypted_data[param->notify.value_len - 3];
      memset(decrypted_data, 0, param->notify.value_len - 3);

      ESP_LOGVV(TAG, "Data Ecrypted: %s ", this->pkt_to_hex_(param->notify.value, param->notify.value_len - 1).c_str());
      this->decrypt_(param->notify.value, param->notify.value_len, decrypted_data);

      uint8_t decrypted_length = param->notify.value_len - 3;
      // Start of packet - this is the header 0x68
      if (decrypted_data[0] == 0x68) {
        // Full packet, expecting more
        if (decrypted_length == 17) {
          ESP_LOGVV(TAG, "GOT START PACKET");
          this->received_packet_buffer_.clear();
          this->received_packet_buffer_.insert(this->received_packet_buffer_.end(), &decrypted_data[0],
                                               &decrypted_data[decrypted_length]);
          ESP_LOGVV(
              TAG, "New Vector contents: %s ",
              this->pkt_to_hex_(this->received_packet_buffer_.data(), this->received_packet_buffer_.size()).c_str());
        }
        // Packet was not full. Decode it right away
        else {
          ESP_LOGVV(TAG, "Final Vector contents: %s ", this->pkt_to_hex_(decrypted_data, decrypted_length).c_str());
          this->decode_(decrypted_data, decrypted_length);
        }
      }
      // Follow-up-Packet received
      else {
        // Another full packet received
        if (decrypted_length == 17) {
          ESP_LOGVV(TAG, "GOT INTERMEDIATE PACKET");
          this->received_packet_buffer_.insert(this->received_packet_buffer_.end(), &decrypted_data[0],
                                               &decrypted_data[decrypted_length]);
          ESP_LOGVV(
              TAG, "Intermediate Vector contents: %s ",
              this->pkt_to_hex_(this->received_packet_buffer_.data(), this->received_packet_buffer_.size()).c_str());
        }
        // Final follow up - decode the vector
        else {
          ESP_LOGVV(TAG, "GOT FINAL PACKET");
          this->received_packet_buffer_.insert(this->received_packet_buffer_.end(), &decrypted_data[0],
                                               &decrypted_data[decrypted_length]);
          ESP_LOGVV(
              TAG, "Final Vector contents: %s ",
              this->pkt_to_hex_(this->received_packet_buffer_.data(), this->received_packet_buffer_.size()).c_str());
          this->decode_(received_packet_buffer_.data(), received_packet_buffer_.size());
          this->received_packet_buffer_.clear();
        }
      }

      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      ESP_LOGD(TAG, "[%s] Received Notification Registration", this->parent_->address_str().c_str());
      std::vector<uint8_t> descriptor = {0x01};
      auto status2 = esp_ble_gattc_write_char_descr(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), 0x25,
                                                    1, const_cast<uint8_t *>(descriptor.data()),
                                                    ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
      if (status2) {
        ESP_LOGW(TAG, "writing descriptor (notify) failed, status=%d", status2);
      } else {
        ESP_LOGD(TAG, "writing descriptor (notify) success, status=%d", status2);
      }

      this->synchronize_device_time_ = true;
      this->node_state = espbt::ClientState::ESTABLISHED;
      break;
    }

    case ESP_GATTC_CONNECT_EVT: {
      ESP_LOGD(TAG, "[%s] Connected", this->parent_->address_str().c_str());
      break;
    }

    default:
      break;
  }
}

#ifdef USE_TIME
void FluvalBleLed::loop() {
  if (this->loop_counter_ > 0) {
    this->loop_counter_ = this->loop_counter_ - 1;
    return;
  }

  this->loop_counter_ = 255;

  if (this->synchronize_device_time_ && !this->time_.has_value()) {
    ESP_LOGD(TAG, "Waiting for time source to come up");
  }

  if (this->synchronize_device_time_ && this->time_.has_value()) {
    std::vector<uint8_t> header1{0x0F};
    ESP_LOGVV(TAG, "Writing Header 1 (REG 0x0F)");
    esp_err_t erra1 = esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                               this->write_reg_id_handle_, 1, const_cast<uint8_t *>(header1.data()),
                                               ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
    ESP_LOGVV(TAG, "Writing 0x0F RESULT: %s", esp_err_to_name(erra1));
    this->synchronize_device_time_ = false;
  }
}

void FluvalBleLed::sync_time() {
  auto *time_id = *this->time_;
  ESPTime now = time_id->now();

  if (now.is_valid()) {
    uint8_t year = now.year - 2000;
    uint8_t month = now.month; // 00 in the app?
    uint8_t day = now.day_of_month;
    uint8_t day_of_week = now.day_of_week == 1 ? 7 : now.day_of_week - 1;
    uint8_t hour = now.hour;
    uint8_t minute = now.minute;
    uint8_t second = now.second;
    uint8_t checksum = 0x68 ^ 0x0E ^ year ^ month ^ day ^ day_of_week ^ hour ^ minute ^ second;

    ESP_LOGD(TAG,
             "Syncing time with Fluval: Year: %d / Month: %d / Day: %d / Day of week: %d / Hour: %d / Minute: %d / Second: %d / Checksumm: %d",
             year, month, day, day_of_week, hour, minute, second, checksum);

    std::vector<uint8_t> value{0x68, 0x0E, year, month, day, day_of_week, hour, minute, second, checksum};
    this->send_packet_(value);
  }
}
#endif

void FluvalBleLed::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
      if (param->ble_security.auth_cmpl.success) {
        ESP_LOGI(TAG, "[%s] Why is there a pairing from Fluval?", this->parent_->address_str().c_str());
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace fluval_ble_led
}  // namespace esphome

#endif
