#include "simplesevse.h"
#include "esphome/core/log.h"
#include "esphome/components/json/json_util.h"

namespace esphome {
namespace simpleevse {

const char *TAG = "SimpleEVSE";


static uint16_t crc16(std::vector<uint8_t>::const_iterator start, std::vector<uint8_t>::const_iterator end) {
  const uint16_t INITIAL{0xFFFF};
  const uint16_t POLYNOM{0xA001};

  uint16_t crc = std::accumulate(start, end, INITIAL, [](uint16_t crc, uint8_t value){
    crc ^= value;
    for (uint8_t i = 0; i < 8; i++) {
      if ((crc & 0x01) != 0) {
        crc >>= 1;
        crc ^= POLYNOM;
      } else {
        crc >>= 1;
      }
    }
    return crc;
  });

  // swap bytes
  return ((crc << 8) & 0xff00) | ((crc >> 8) & 0x00ff);
}

std::vector<uint8_t> ModbusTransaction::encode_request() {
  std::vector<uint8_t> buffer;

  // Reserve space to avoid reallocations
  buffer.reserve(this->request_size_ + this->frame_size_);

  BufferStreamWriter writer(std::back_inserter(buffer));

  // Address | Fuction
  writer << MODBUS_ADDRESS << this->function_code_;

  // Data
  this->on_get_data(writer);

  // CRC
  uint16_t crc = crc16(buffer.cbegin(), buffer.cend());
  writer << crc;

  ESP_LOGD(TAG, "Sending request with function=0x%02X.", this->function_code_);
  this->send_time_ = millis();
  this->attempt_--;

  return buffer;
}

bool ModbusTransaction::decode_response(const std::vector<uint8_t> &buffer) {
  size_t len = buffer.size();
  if (len < MIN_FRAME_LEN) {
    ESP_LOGW(TAG, "Incomplete data received with length %d.", len);
    return false;
  }

  // check CRC
  uint16_t remote_crc = encode_uint16(buffer[len - 2], buffer[len - 1]);
  uint16_t computed_crc = crc16(buffer.cbegin(), buffer.cend() - 2);
  if (remote_crc != computed_crc) {
    ESP_LOGW(TAG, "Modbus CRC Check failed! Expected 0x%04X, got 0x%04X.", computed_crc, remote_crc);
    return false;
  }

  // Frame should be correct because CRC matched
  BufferStreamReader reader(buffer.cbegin(), buffer.cend() - 2);

  uint8_t address, function;
  reader >> address >> function;
  if (address != MODBUS_ADDRESS) {
    ESP_LOGE(TAG, "Response has invalid address: %d.", address);
    return false;
  }

  if ((function & EXCEPTION_MASK) != this->function_code_) {
    ESP_LOGE(TAG, "Response has invalid function code! Expected 0x%02X, got 0x%02X.", this->function_code_, function);
    return false;
  }

  if (function & (~EXCEPTION_MASK)) {
    ESP_LOGE(TAG, "Exception response: 0x%02X.", buffer[2]);
    this->on_error(ModbusTransactionResult::EXCEPTION);
    return true;
  } 

  if (reader.remaining_size() != this->response_size_) {
    ESP_LOGE(TAG, "Invalid response size. Expected %d, got %d.", reader.remaining_size(), this->response_size_);
    return false;
  }

  // handle data
  ESP_LOGD(TAG, "Receive response with function=0x%02X.", function);
  this->on_handle_data(reader);

  // check if to much data was read
  if (reader.error()) {
    ESP_LOGE(TAG, "Too much data was read from the frame.");
  }

  return true;
}

void ModbusReadHoldingRegistersTransaction::on_get_data(BufferStreamWriter &writer) {
  // address | register count
  writer << this->address_ << this->count_;
}

void ModbusReadHoldingRegistersTransaction::on_handle_data(BufferStreamReader &reader) {
  // check received size with data size
  uint8_t count;
  reader >> count;
  if (count != reader.remaining_size()) {
    ESP_LOGE(TAG, "Message length does not match received data. Expected %d, got %d.", count, reader.remaining_size());
    return;
  }

  // decode 16-bit registers
  std::vector<uint16_t> regs;
  regs.reserve(this->count_);
  uint16_t reg;
  for (int i = 0; i < this->count_; i++) {
    reader >> reg;
    regs.push_back(reg);
  }

  this->callback_(ModbusTransactionResult::SUCCESS, regs);
}

void ModbusReadHoldingRegistersTransaction::on_error(ModbusTransactionResult result) {
  this->callback_(result, std::vector<uint16_t>());
}

void ModbusWriteHoldingRegistersTransaction::on_get_data(BufferStreamWriter &writer) {
  // address | register count | byte count | value(s)
  writer << this->address_ << uint16_t(1) << uint8_t(2) << this->value_;
}

void ModbusWriteHoldingRegistersTransaction::on_handle_data(BufferStreamReader &reader) {
  this->callback_(ModbusTransactionResult::SUCCESS);
}

void ModbusWriteHoldingRegistersTransaction::on_error(ModbusTransactionResult result) {
  this->callback_(result);
}

void ModbusDeviceComponent::loop() {
  const uint32_t now = micros();

  // collect received bytes into buffer
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    this->rx_buffer_.push_back(byte);
    this->last_modbus_byte_ = now;
    this->after_char_delay_ = true;
  }

  // decode receive buffer after t1.5 silence
  if (this->rx_buffer_.size() > 0) {
    if ((now - this->last_modbus_byte_) > this->inter_char_timeout_) {
      if (this->active_transaction_ != nullptr) {
        if (this->active_transaction_->decode_response(this->rx_buffer_)) {
          this->active_transaction_ = nullptr;
        }
      } else {
        ESP_LOGW(TAG, "Receive data but no one is waiting for it.");
      }
      this->rx_buffer_.clear();
    }
  }

  // wait time after last activity
  if (this->after_char_delay_) {
    if ((now - this->last_modbus_byte_) > this->inter_frame_timeout_) {
      this->after_char_delay_ = false;
    }
  }

  // check for response timeout
  if (this->active_transaction_ != nullptr && this->active_transaction_->check_timeout()) {
    ESP_LOGE(TAG, "Timeout waiting for response.");
    if (this->active_transaction_->should_retry())
    {
      // most errors result eventually in a timeout so actualy retry handles
      // more cases than just timeouts
      ESP_LOGI(TAG, "Retry request.");
      this->send_request();
    }
    else
    {
      this->active_transaction_->timeout();
      this->active_transaction_ = nullptr;
    }
  }

  // no modbus activity and ready for a new request
  if (!this->after_char_delay_ && !this->active_transaction_) {
    this->idle();
  }
}

void ModbusDeviceComponent::execute(std::unique_ptr<ModbusTransaction> &&transaction) {
  if (this->active_transaction_ != nullptr) {
    ESP_LOGW(TAG, "Trying to execute a modbus transaction although one is already running.");
    transaction->cancel();
    return;
  }

  this->active_transaction_ = std::move(transaction);
  this->send_request();
}

void ModbusDeviceComponent::send_request() {
  std::vector<uint8_t> buffer = std::move(this->active_transaction_->encode_request());
  this->write_array(buffer);
  this->after_char_delay_ = true;
  this->last_modbus_byte_ = micros();  
}

void SimpleEvseComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SimpleEvse:");
  this->check_uart_settings(BAUD_RATE, STOP_BITS, uart::UARTParityOptions::UART_CONFIG_PARITY_NONE, PARITY_BITS);
  LOG_UPDATE_INTERVAL(this);  
}

void SimpleEvseComponent::add_transaction(std::unique_ptr<ModbusTransaction> &&transaction) {
  if (this->running_) {
    this->transactions_.push(std::move(transaction));
  } else {
    ESP_LOGW(TAG, "Cancelled transaction because connection is not running.");
    transaction->cancel();
  }
}

void SimpleEvseComponent::on_status_received(ModbusTransactionResult result, const std::vector<uint16_t> &reg)
{
  if (result == ModbusTransactionResult::SUCCESS) {
    if (reg.size() == COUNT_STATUS_REGISTER) {
      std::copy(reg.cbegin(), reg.cend(), this->status_register_.begin());
      this->running_ = true;
      this->status_clear_error();
      this->process_triggers();
    } else {
      ESP_LOGW(TAG, "Invalid number of register: expected %d, got %d", COUNT_STATUS_REGISTER, reg.size());
      this->running_ = false;
      this->status_set_error();
    }
  } else {
    this->status_set_error();
    if (this->running_) {
      this->running_ = false;
      
      ESP_LOGW(TAG, "No connection to EVSE - reset pending transactions (size=%d).", this->transactions_.size());
      while (!this->transactions_.empty()) {
        auto transaction = std::move(this->transactions_.front());
        this->transactions_.pop();
        transaction->cancel();
      }
    }
  }

  for (auto it = std::begin(this->observer_); it != std::end(this->observer_); ++it) {
    (*it)->update(this->running_, this->status_register_);
  }
}

void SimpleEvseComponent::process_triggers()
{
    uint16_t vehicle_state = this->status_register_[REGISTER_VEHICLE_STATE];

    if (!this->was_plugged_ && (vehicle_state == VehicleState::VEHICLE_EV_PRESENT || vehicle_state == VehicleState::VEHICLE_CHARGING || vehicle_state == VehicleState::VEHICLE_CHARGING_WITH_VENT)) {
        // vehicle is now plugged 
        this->was_plugged_ = true;

        if (this->unplugged_trigger_ && this->unplugged_trigger_->is_action_running()) {
          this->unplugged_trigger_->stop_action();
        }

        if (this->plugged_trigger_) {
          this->plugged_trigger_->trigger();
        }
    } else if (this->was_plugged_ && vehicle_state == VehicleState::VEHICLE_READY) {
        // vehicle is now unplugged
        this->was_plugged_ = false;

        if (this->plugged_trigger_ && this->plugged_trigger_->is_action_running()) {
          this->plugged_trigger_->stop_action();
        }

        if (this->unplugged_trigger_) {
          this->unplugged_trigger_->trigger();
        }
    }
}

void SimpleEvseComponent::idle()
{
  const uint32_t now = millis();

  // take next queued transaction and execute it
  if (!transactions_.empty()) {
    ESP_LOGD(TAG, "Executed next queued transaction (size=%d).", this->transactions_.size());
    auto transaction = std::move(this->transactions_.front());
    transactions_.pop();
    this->execute(std::move(transaction));

    // force update of status because the transaction might change it
    this->force_update_ = true;
    return;
  }

  // poll status
  if (this->force_update_ || (now - this->last_state_udpate_) >= this->update_interval_) {
    this->force_update_ = false;
    this->last_state_udpate_ = now;
    auto status = make_unique<ModbusReadHoldingRegistersTransaction>(FIRST_STATUS_REGISTER, COUNT_STATUS_REGISTER, std::bind(&SimpleEvseComponent::on_status_received, this, std::placeholders::_1, std::placeholders::_2));

    this->execute(std::move(status));
    return;
  }
}

#ifdef USE_SENSOR
void SimpleEvseSensors::update(bool running, const std::array<uint16_t, COUNT_STATUS_REGISTER> &status_register)
{
  if (this->connected_) {
    this->connected_->publish_state(running);
  }

  if (running) {
    if (this->set_charge_current_) {
      this->set_charge_current_->publish_state(status_register[REGISTER_CHARGE_CURRENT]);
    }

    if (this->actual_charge_current_) {
      this->actual_charge_current_->publish_state(status_register[REGISTER_ACTUAL_CURRENT]);
    }

    if (this->vehicle_state_sensor_){
      switch (status_register[REGISTER_VEHICLE_STATE]) {
        case VehicleState::VEHICLE_UNKNOWN:
          this->vehicle_state_sensor_->publish_state("unknown");
          break;
        case VehicleState::VEHICLE_READY:
          this->vehicle_state_sensor_->publish_state("ready");
          break;
        case VehicleState::VEHICLE_EV_PRESENT:
          this->vehicle_state_sensor_->publish_state("EV is present");
          break;
        case VehicleState::VEHICLE_CHARGING:
          this->vehicle_state_sensor_->publish_state("charging");
          break;
        case VEHICLE_CHARGING_WITH_VENT:
          this->vehicle_state_sensor_->publish_state("charging with ventilation");
          break;
        default:
          this->vehicle_state_sensor_->publish_state("?");
          ESP_LOGW(TAG, "Invalid state received: %d", status_register[REGISTER_VEHICLE_STATE]);
          break;
      }
    }

    if (this->max_current_limit_) {
      this->max_current_limit_->publish_state(status_register[REGISTER_MAX_CURRENT]);
    }  

    if (this->firmware_revision_) {
      this->firmware_revision_->publish_state(status_register[REGISTER_FIRMWARE]);
    }

    if (this->evse_state_sensor_) {
      switch (status_register[REGISTER_EVSE_STATE]) {
        case 0:
          this->evse_state_sensor_->publish_state("unknown");
          break;
        case 1:
          this->evse_state_sensor_->publish_state("steady 12V");
          break;
        case 2:
          this->evse_state_sensor_->publish_state("PWM");
          break;
        case 3:
          this->evse_state_sensor_->publish_state("OFF");
          break;
        default:
          this->evse_state_sensor_->publish_state("?");
          ESP_LOGW(TAG, "Invalid state received: %d", status_register[REGISTER_EVSE_STATE]);
          break;        
      }
    }
  }
}
#endif

#ifdef USE_SWITCH
void SimpleEvseChargingSwitch::update(bool running, const std::array<uint16_t, COUNT_STATUS_REGISTER> &status_register) {
  // do not update as long a write is pending
  if (!this->active_writes_) {
    this->publish_state((status_register[REGISTER_CTRL_BITS] & CHARGING_ENABLED_MASK) == CHARGING_ENABLED_ON);
  }
}

void SimpleEvseChargingSwitch::write_state(bool state) {
  this->active_writes_++;
  ESP_LOGD(TAG, "Queue command to change charging to %d. (Active writes=%d)", state, this->active_writes_);
  auto trans = make_unique<ModbusWriteHoldingRegistersTransaction>(FIRST_STATUS_REGISTER + REGISTER_CTRL_BITS, state ? CHARGING_ENABLED_ON : CHARGING_ENABLED_OFF, [this, state](ModbusTransactionResult result) {
    this->active_writes_--;
    ESP_LOGD(TAG, "Command executed. (Success=%d, Active writes=%d)", static_cast<int>(result), this->active_writes_);
    if (result == ModbusTransactionResult::SUCCESS) {
      this->publish_state(state);
    } else {
      this->publish_state(this->state);
    }
  });
  this->parent_->add_transaction(std::move(trans));
}
#endif

#ifdef USE_SIMPLEEVSE_WEB_CONFIG
void SimpleEvseHttpHandler::handleRequest(AsyncWebServerRequest *req) {
  if (req->url() == this->set_value_path && req->method() == HTTP_POST) {
    this->handleSetConfig(req);
  } else {
    this->handleIndex(req);
  }
}

void SimpleEvseHttpHandler::handleIndex(AsyncWebServerRequest *req) {
  this->defer([this, req](){
    AsyncResponseStream *stream = req->beginResponseStream("text/html");
    stream->print(F(
      R"(<!DOCTYPE html>)"
      R"(<html lang="en"><head>)"
      R"(<meta charset=UTF-8>)"
      R"(<style>*{font-family:sans-serif;}table{border-collapse:collapse;}table td,table th{border:1px solid #dfe2e5;padding:6px 13px;}table th{font-weight:600;text-align:center;}tr:nth-child(2n){background-color: #f6f8fa;}</style>)"
      R"(<title>SimpleEvse Config</title>)"
      R"(</head><body>)"
      R"(<h1>SimpleEVSE Configuration Register</h1>)"
    ));
    
    auto trans = make_unique<ModbusReadHoldingRegistersTransaction>(FIRST_CONFIG_REGISTER, COUNT_CONFIG_REGISTER, [this, req, stream](ModbusTransactionResult result, std::vector<uint16_t> regs){
      if (result == ModbusTransactionResult::SUCCESS) {
        stream->print(F(
          R"(<table>)"
          R"(<thead><tr><th>Register</th><th>Value</th><th>Update</th></thead>)"
          R"(<tbody>)"
        ));
        for (uint16_t i = 0; i < regs.size(); ++i) {
          stream->print(F(R"(<tr><td>)"));
          stream->print(i + FIRST_CONFIG_REGISTER);
          stream->print(F(R"(</td><td>)"));
          stream->print(regs[i]);
          stream->print(F(
            R"(</td><td>)"
            R"(<form action=")"
          ));
          stream->print(this->set_value_path);
          stream->print(F(
            R"(" method="post">)"
            R"(<input type="hidden" name="register" value=")"
          ));
          stream->print(i + FIRST_CONFIG_REGISTER);
          stream->print(F(
            R"(">)"
            R"(<input type="number" min="0" max="65535" name="value">)"
            R"(<input type="submit">)"
            R"(</form>)"
            R"(</td></tr>)"
          ));
        }
        stream->print(F(
          R"(</table>)"
          R"(</tbody>)"
        ));
      } else {
        stream->print(F(R"(<p>Error requesting registers.</p>)"));
      }

      stream->print(F(
        R"(</body></html>)"
      ));
      req->send(stream);
    });
    this->parent_->add_transaction(std::move(trans));
  });
}

void SimpleEvseHttpHandler::handleSetConfig(AsyncWebServerRequest *req) {
  if (!req->hasParam("register", true) || !req->hasParam("value", true)) {
    ESP_LOGW(TAG, "Request with missing register and value parameter.");
    req->send(400, "text/plain", "Missing register and/or value.");
    return;
  }

  auto reg = req->getParam("register", true)->value().toInt();
  auto val = req->getParam("value", true)->value().toInt();

  if (reg < FIRST_CONFIG_REGISTER || reg >= (FIRST_CONFIG_REGISTER + COUNT_CONFIG_REGISTER)) {
    ESP_LOGW(TAG, "Invalid register %ld.", reg);
    req->send(400, "text/plain", "Invalid register.");
    return;
  }

  if (val < 0 || val > 0xFFFF) {
    ESP_LOGW(TAG, "Invalid value %ld.", val);
    req->send(400, "text/plain", "Invalid value.");
    return;
  }
  this->defer([this, reg, val, req]() {
    auto trans = make_unique<ModbusWriteHoldingRegistersTransaction>(reg, val, [this, req](ModbusTransactionResult result){
      switch (result) {
        case ModbusTransactionResult::SUCCESS: {
          AsyncWebServerResponse *response = req->beginResponse(303); // See Other
          response->addHeader("Location", this->index_path);
          req->send(response);
          break;
        }
        case ModbusTransactionResult::EXCEPTION:
          req->send(500, "text/plain", "Error setting value.");
          break;
        case ModbusTransactionResult::TIMEOUT:
          req->send(504, "text/plain", "Timeout setting value.");
          break;
        case ModbusTransactionResult::CANCELLED:
          req->send(503, "text/plain", "Request was cancelled.");
          break;
        default:
          req->send(500, "text/plain", "Unknown error.");
          break;
      }
    });
    this->parent_->add_transaction(std::move(trans));
  });
}
#endif

}  // namespace simpleevse
}  // namespace esphome


