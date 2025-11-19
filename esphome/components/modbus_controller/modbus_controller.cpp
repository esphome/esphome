#include "modbus_controller.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_controller";

void ModbusController::setup() { this->create_register_ranges_(); }

void ModbusController::increment_non_response_count() { this->cmd_non_responses_++; }

void ModbusController::set_online(bool online, const ModbusCommandItem &command) {
  if (online && this->module_offline_) {
    ESP_LOGW(TAG, "Modbus device=%d back online", this->address_);
    this->cmd_non_responses_ = 0;  // reset send count on success

    // Restore module online state
    this->module_offline_ = false;
    this->online_callback_.call((int) command.function_code, command.register_address);

  } else if (!online) {
    this->clear_tx_queue_for_address(false);

    if (!this->module_offline_) {
      ESP_LOGW(TAG, "Modbus device=%d set offline", this->address_);
      this->module_offline_ = true;
      this->module_offline_at_ = this->update_counter_;
      this->offline_callback_.call((int) command.function_code, command.register_address);
    }
  }
}

// Queue incoming response
void ModbusCommandItem::on_modbus_data(const std::vector<uint8_t> &data) {
  this->controller->set_online(true, *this);

  if (this->on_data_func) {
    this->on_data_func(this->register_type, this->register_address, data);
  } else {
    if (this->function_code == ModbusFunctionCode::CUSTOM) {
      ESP_LOGI(TAG, "Custom Command sent");
    } else if (is_function_code_write((u_int8_t(this->function_code)))) {
      // Write command response
      for (auto *sensor : this->sensors) {
        sensor->on_write_response(data);
      }
    } else {
      // Default handling: send data to all sensors of this range
      for (auto *sensor : this->sensors) {
        sensor->parse_and_publish(data);
      }
    }
  }
  this->controller->unqueue_command(this);
}

// Modbus error message is a legit response from the device. Consider the device online.
void ModbusCommandItem::on_modbus_error(uint8_t function_code, uint8_t exception_code) {
  this->controller->set_online(true, *this);
  this->controller->unqueue_command(this);
}

// Command not being sent doesn't tell us whether device is online or offline
// So we just unqueue it.
void ModbusCommandItem::on_modbus_not_sent() { this->controller->unqueue_command(this); }

void ModbusCommandItem::on_modbus_no_response() {
  this->controller->increment_non_response_count();
  if (this->controller->can_send()) {
    this->send();
  } else {
    this->controller->set_online(false, *this);
    this->controller->unqueue_command(this);
  }
}

void SensorItem::on_write_response(const std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "Command ACK 0x%X %d ", get_data<uint16_t>(data, 0), get_data<int16_t>(data, 1));
}

void ModbusController::update_range_(ModbusCommandItem &cmd) {
  if (this->update_counter_ % (cmd.skip_updates + 1) != 0) {
    ESP_LOGVV(TAG, "Skipping update for range 0x%X", cmd.register_address);
    return;
  }

  ESP_LOGV(TAG, "Range : %X Size: %x (%d)", cmd.register_address, cmd.register_count, (int) cmd.register_type);
  cmd.send();
}

void ModbusController::update() {
  if (this->module_offline_) {
    if ((this->update_counter_ + 1 - this->module_offline_at_) % (this->offline_skip_updates_ + 1) != 0) {
      ESP_LOGV(TAG, "Module offline - skipping update");
    } else {  // time to try again
      ESP_LOGV(TAG, "Module offline - resuming updates");
      this->cmd_non_responses_ = 0;  // reset non-responsive send count after offline_skip_updates
    }
  }

  if (this->can_send()) {
    for (auto &cmd : this->polling_command_items_) {
      ESP_LOGVV(TAG, "Updating range 0x%X", cmd.register_address);
      update_range_(cmd);
    }
  }

  this->update_counter_++;
}

void ModbusController::queue_command(const ModbusCommandItem &command) {
  this->one_shot_command_items_.push_back(make_unique<ModbusCommandItem>(command));
  this->one_shot_command_items_.back()->send();
  ESP_LOGV(TAG, "Added item to one shot commands. %d items total", this->one_shot_command_items_.size());
}

void ModbusController::unqueue_command(const ModbusCommandItem *command) {
  auto erased = std::erase_if(this->one_shot_command_items_, [command](const std::unique_ptr<ModbusCommandItem> &item) {
    return command == item.get();
  });
  ESP_LOGV(TAG, "Erased %d items from one shot commands. %d items remaining", erased,
           this->one_shot_command_items_.size());
}

// walk through the sensors and determine the register ranges to read
size_t ModbusController::create_register_ranges_() {
  if (this->sensorset_.empty()) {
    ESP_LOGW(TAG, "No sensors registered");
    return 0;
  }

  // Clear the tx queue to remove any pending commands for this device
  this->polling_command_items_.clear();

  std::vector<RegisterRange> register_ranges;

  // iterator is sorted see SensorItemsComparator for details
  auto ix = this->sensorset_.begin();
  RegisterRange r = {};
  uint8_t buffer_offset = 0;
  SensorItem *prev = nullptr;
  while (ix != this->sensorset_.end()) {
    SensorItem *curr = *ix;
    if (curr->register_type == ModbusRegisterType::CUSTOM) {
      // skip custom registers for range creation
      ix++;
      continue;
    }

    ESP_LOGV(TAG, "Register: 0x%X %d %d %d offset=%u skip=%u addr=%p", curr->start_address, curr->register_count,
             curr->offset, curr->get_register_size(), curr->offset, curr->skip_updates, curr);

    if (r.register_count == 0) {
      // this is the first register in range
      r.start_address = curr->start_address;
      r.register_count = curr->register_count;
      r.register_type = curr->register_type;
      r.sensors.insert(curr);  // TODO: This is redundant with insert below
      r.skip_updates = curr->skip_updates;
      buffer_offset = curr->get_register_size();

      ESP_LOGV(TAG, "Started new range");
    } else {
      // this is not the first register in range so it might be possible
      // to reuse the last register or extend the current range
      if (!curr->force_new_range && r.register_type == curr->register_type) {
        if (curr->start_address == (r.start_address + r.register_count - prev->register_count) &&
            curr->register_count == prev->register_count && curr->get_register_size() == prev->get_register_size()) {
          // this register can re-use the data from the previous register

          // remove this sensore because start_address is changed (sort-order)
          ix = this->sensorset_.erase(ix);

          curr->start_address = r.start_address;
          curr->offset += prev->offset;

          this->sensorset_.insert(curr);
          // move iterator backwards because it will be incremented later
          ix--;

          ESP_LOGV(TAG, "Re-use previous register - change to register: 0x%X %d offset=%u", curr->start_address,
                   curr->register_count, curr->offset);
        } else if (curr->start_address == (r.start_address + r.register_count)) {
          // this register can extend the current range

          // remove this sensore because start_address is changed (sort-order)
          ix = this->sensorset_.erase(ix);

          curr->start_address = r.start_address;
          curr->offset += buffer_offset;
          buffer_offset += curr->get_register_size();
          r.register_count += curr->register_count;

          this->sensorset_.insert(curr);
          // move iterator backwards because it will be incremented later
          ix--;

          ESP_LOGV(TAG, "Extend range - change to register: 0x%X %d offset=%u", curr->start_address,
                   curr->register_count, curr->offset);
        }
      }
    }

    if (curr->start_address == r.start_address && curr->register_type == r.register_type) {
      // use the lowest non zero value for the whole range
      // Because zero is the default value for skip_updates it is excluded from getting the min value.
      if (curr->skip_updates != 0) {
        if (r.skip_updates != 0) {
          r.skip_updates = std::min(r.skip_updates, curr->skip_updates);
        } else {
          r.skip_updates = curr->skip_updates;
        }
      }

      // add sensor to this range
      r.sensors.insert(curr);

      ix++;
    } else {
      ESP_LOGV(TAG, "Add range 0x%X %d skip:%d", r.start_address, r.register_count, r.skip_updates);
      register_ranges.push_back(std::move(r));
      r = {};
      buffer_offset = 0;
      // do not increment the iterator here because the current sensor has to be re-evaluated
    }

    prev = curr;
  }

  if (r.register_count > 0) {
    // Add the last range
    ESP_LOGV(TAG, "Add last range 0x%X %d skip:%d", r.start_address, r.register_count, r.skip_updates);
    register_ranges.push_back(std::move(r));
  }

  for (auto &r : register_ranges) {
    ModbusCommandItem cmd =
        ModbusCommandItem::create_read_command(this, r.register_type, r.start_address, r.register_count);
    // TODO: Create a factory method for this
    cmd.sensors = std::move(r.sensors);
    cmd.skip_updates = r.skip_updates;
    this->polling_command_items_.push_back(cmd);
  }

  for (auto &sensor : this->sensorset_) {
    if (sensor->register_type == ModbusRegisterType::CUSTOM) {
      ModbusCommandItem cmd = ModbusCommandItem::create_custom_command(this, sensor->custom_data);
      cmd.register_address = sensor->start_address;  // TODO: Is this needed?
      cmd.register_count = sensor->register_count;   // TODO: Is this needed?
      this->polling_command_items_.push_back(std::move(cmd));
    }
  }

  return this->polling_command_items_.size();
}

void ModbusController::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ModbusController:\n"
                "  Address: 0x%02X\n"
                "  Max Command Retries: %d\n"
                "  Offline Skip Updates: %d\n",
                this->address_, this->max_cmd_retries_, this->offline_skip_updates_);

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGCONFIG(TAG, "sensormap");
  for (auto &it : this->sensorset_) {
    ESP_LOGCONFIG(TAG, " Sensor type=%zu start=0x%X offset=0x%X count=%d size=%d",
                  static_cast<uint8_t>(it->register_type), it->start_address, it->offset, it->register_count,
                  it->get_register_size());
  }
  ESP_LOGCONFIG(TAG, "ranges");
  for (auto &it : this->polling_command_items_) {
    ESP_LOGCONFIG(TAG, "  Range type=%zu start=0x%X count=%d skip_updates=%d", static_cast<uint8_t>(it.register_type),
                  it.register_address, it.register_count, it.skip_updates);
  }

#endif
}

void ModbusController::dump_sensors_() {
  ESP_LOGV(TAG, "sensors");
  for (auto &it : this->sensorset_) {
    ESP_LOGV(TAG, "  Sensor start=0x%X count=%d size=%d offset=%d", it->start_address, it->register_count,
             it->get_register_size(), it->offset);
  }
}

ModbusCommandItem::ModbusCommandItem(ModbusController *controller) : controller(controller) {
  if (controller != nullptr) {
    this->controller = controller;
    this->set_parent(controller->parent_);
    this->set_address(controller->address_);
  }
}

ModbusCommandItem ModbusCommandItem::create_read_command(
    ModbusController *controller, ModbusRegisterType register_type, uint16_t start_address, uint16_t register_count,
    std::function<void(ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
        &&handler) {
  ModbusCommandItem cmd(controller);
  cmd.register_type = register_type;
  cmd.function_code = modbus_register_read_function(register_type);
  cmd.register_address = start_address;
  cmd.register_count = register_count;
  if (handler)
    cmd.on_data_func = std::move(handler);
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_multiple_command(ModbusController *controller, uint16_t start_address,
                                                                   uint16_t register_count,
                                                                   const std::vector<uint16_t> &values) {
  ModbusCommandItem cmd(controller);
  cmd.register_type = ModbusRegisterType::HOLDING;
  cmd.function_code = ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS;
  cmd.register_address = start_address;
  cmd.register_count = register_count;
  for (auto v : values) {
    auto decoded_value = decode_value(v);
    cmd.payload.push_back(decoded_value[0]);
    cmd.payload.push_back(decoded_value[1]);
  }
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_single_coil(ModbusController *controller, uint16_t address,
                                                              bool value) {
  ModbusCommandItem cmd(controller);
  cmd.register_type = ModbusRegisterType::COIL;
  cmd.function_code = ModbusFunctionCode::WRITE_SINGLE_COIL;
  cmd.register_address = address;
  cmd.register_count = 1;
  cmd.payload.push_back(value ? 0xFF : 0);
  cmd.payload.push_back(0);
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_multiple_coils(ModbusController *controller, uint16_t start_address,
                                                                 const std::vector<bool> &values) {
  ModbusCommandItem cmd(controller);
  cmd.register_type = ModbusRegisterType::COIL;
  cmd.function_code = ModbusFunctionCode::WRITE_MULTIPLE_COILS;
  cmd.register_address = start_address;
  cmd.register_count = values.size();

  uint8_t bitmask = 0;
  int bitcounter = 0;
  for (auto coil : values) {
    if (coil) {
      bitmask |= (1 << bitcounter);
    }
    bitcounter++;
    if (bitcounter % 8 == 0) {
      cmd.payload.push_back(bitmask);
      bitmask = 0;
    }
  }
  // add remaining bits
  if (bitcounter % 8) {
    cmd.payload.push_back(bitmask);
  }
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_single_command(ModbusController *controller, uint16_t start_address,
                                                                 uint16_t value) {
  ModbusCommandItem cmd(controller);
  cmd.register_type = ModbusRegisterType::HOLDING;
  cmd.function_code = ModbusFunctionCode::WRITE_SINGLE_REGISTER;
  cmd.register_address = start_address;
  cmd.register_count = 1;  // not used here anyways

  auto decoded_value = decode_value(value);
  cmd.payload.push_back(decoded_value[0]);
  cmd.payload.push_back(decoded_value[1]);
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_custom_command(
    ModbusController *controller, const std::vector<uint8_t> &values,
    std::function<void(ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
        &&handler) {
  ModbusCommandItem cmd(controller);
  cmd.function_code = ModbusFunctionCode::CUSTOM;
  cmd.on_data_func = handler;

  cmd.payload = values;

  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_custom_command(
    ModbusController *controller, const std::vector<uint16_t> &values,
    std::function<void(ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
        &&handler) {
  ModbusCommandItem cmd(controller);
  cmd.function_code = ModbusFunctionCode::CUSTOM;
  cmd.on_data_func = handler;

  for (auto v : values) {
    cmd.payload.push_back((v >> 8) & 0xFF);
    cmd.payload.push_back(v & 0xFF);
  }

  return cmd;
}

bool ModbusCommandItem::send() {
  if (this->function_code != ModbusFunctionCode::CUSTOM) {
    this->ModbusClientDevice::send(uint8_t(this->function_code), this->register_address, this->register_count,
                                   this->payload.size(), this->payload.empty() ? nullptr : &this->payload[0]);
  } else {
    this->send_raw(this->payload);
  }
  ESP_LOGV(TAG, "Command sent %d 0x%X %d", uint8_t(this->function_code), this->register_address, this->register_count);
  return true;
}

bool ModbusCommandItem::is_equal(const ModbusCommandItem &other) {
  // for custom commands we have to check for identical payloads, since
  // address/count/type fields will be set to zero
  return this->function_code == ModbusFunctionCode::CUSTOM
             ? this->payload == other.payload
             : other.register_address == this->register_address && other.register_count == this->register_count &&
                   other.register_type == this->register_type && other.function_code == this->function_code;
}

void ModbusController::add_on_command_sent_callback(std::function<void(int, int)> &&callback) {
  this->command_sent_callback_.add(std::move(callback));
}

void ModbusController::add_on_online_callback(std::function<void(int, int)> &&callback) {
  this->online_callback_.add(std::move(callback));
}

void ModbusController::add_on_offline_callback(std::function<void(int, int)> &&callback) {
  this->offline_callback_.add(std::move(callback));
}

}  // namespace modbus_controller
}  // namespace esphome
