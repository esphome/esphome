#include "modbus_controller.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::modbus_controller {

static const char *const TAG = "modbus_controller";

void ModbusController::setup() { this->create_register_ranges_(); }

/*
 To work with the existing modbus class and avoid polling for responses a command queue is used.
 send_next_command will submit the command at the top of the queue and set the corresponding callback
 to handle the response from the device.
 Once the response has been processed it is removed from the queue and the next command is sent
*/
bool ModbusController::send_next_command_() {
  uint32_t last_send = millis() - this->last_command_timestamp_;

  if ((last_send > this->command_throttle_) && this->ready_for_immediate_send() && !this->command_queue_.empty()) {
    auto &command = this->command_queue_.front();

    // remove from queue if command was sent too often
    if (!command->should_retry(this->max_cmd_retries_)) {
      if (!this->module_offline_) {
        ESP_LOGW(TAG, "Modbus device=%d set offline", this->address_);

        if (this->offline_skip_updates_ > 0) {
          // Update skip_updates_counter to stop flooding channel with timeouts
          for (auto &r : this->register_ranges_) {
            r.skip_updates_counter = this->offline_skip_updates_;
          }
        }

        this->module_offline_ = true;
        this->offline_callback_.call((int) command->function_code, command->register_address);
      }
      ESP_LOGD(TAG, "Modbus command to device=%d register=0x%02X no response received - removed from send queue",
               this->address_, command->register_address);
      this->command_queue_.pop_front();
    } else {
      ESP_LOGV(TAG, "Sending next modbus command to device %d register 0x%02X count %d", this->address_,
               command->register_address, command->register_count);
      command->send();

      this->last_command_timestamp_ = millis();

      this->command_sent_callback_.call((int) command->function_code, command->register_address);

      // remove from queue if no handler is defined
      if (!command->on_data_func) {
        this->command_queue_.pop_front();
      }
    }
  }
  return (!this->command_queue_.empty());
}

// Queue incoming response
void ModbusController::on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) {
  if (this->command_queue_.empty()) {
    ESP_LOGW(TAG, "Received modbus data but command queue is empty");
    return;
  }
  auto &current_command = this->command_queue_.front();
  if (current_command != nullptr) {
    if (this->module_offline_) {
      ESP_LOGW(TAG, "Modbus device=%d back online", this->address_);

      if (this->offline_skip_updates_ > 0) {
        // Restore skip_updates_counter to restore commands updates
        for (auto &r : this->register_ranges_) {
          r.skip_updates_counter = 0;
        }
      }
      // Restore module online state
      this->module_offline_ = false;
      this->online_callback_.call((int) current_command->function_code, current_command->register_address);
    }

    // Move the commandItem to the response queue. The span points into the hub's receive buffer, so
    // copy the payload into the command for deferred processing in loop().
    auto data = modbus::helpers::server_pdu_payload(response_pdu);
    current_command->payload.assign(data.begin(), data.end());
    this->incoming_queue_.push(std::move(current_command));
    ESP_LOGV(TAG, "Modbus response queued");
    this->command_queue_.pop_front();
  }
}

// Dispatch the response to the registered handler
void ModbusController::process_modbus_data_(const ModbusCommandItem *response) {
  ESP_LOGV(TAG, "Process modbus response for address 0x%X size: %zu", response->register_address,
           response->payload.size());
  response->on_data_func(response->register_type, response->register_address, response->payload);
}

void ModbusController::on_error(std::span<const uint8_t> request_pdu, modbus::ExceptionCode exception_code) {
  // The request function code (request_pdu[0]) already carries what the log needs; the exception bit only
  // ever appears on the response, so no masking is needed here.
  const uint8_t function_code = request_pdu.empty() ? 0 : request_pdu[0];
  ESP_LOGE(TAG, "Modbus error function code: 0x%X exception: %d ", function_code, static_cast<uint8_t>(exception_code));
  if (this->command_queue_.empty()) {
    return;
  }
  // Remove pending command waiting for a response
  auto &current_command = this->command_queue_.front();
  if (current_command != nullptr) {
    ESP_LOGE(TAG,
             "Modbus error - last command: function code=0x%X  register address = 0x%X  "
             "registers count=%d "
             "payload size=%zu",
             function_code, current_command->register_address, current_command->register_count,
             current_command->payload.size());
    this->command_queue_.pop_front();
  }
}

SensorSet ModbusController::find_sensors_(modbus::EntityType register_type, uint16_t start_address) const {
  auto reg_it = std::find_if(
      std::begin(this->register_ranges_), std::end(this->register_ranges_),
      [=](RegisterRange const &r) { return (r.start_address == start_address && r.register_type == register_type); });

  if (reg_it == this->register_ranges_.end()) {
    ESP_LOGE(TAG, "No matching range for sensor found - start_address : 0x%X", start_address);
  } else {
    return reg_it->sensors;
  }

  // not found
  return {};
}
void ModbusController::on_register_data(modbus::EntityType register_type, uint16_t start_address,
                                        const std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "data for register address : 0x%X : ", start_address);

  // loop through all sensors in this range; each reads its own bytes from the position resolved for it.
  auto sensors = find_sensors_(register_type, start_address);
  for (auto *sensor : sensors) {
    sensor->parse_and_publish(data);
  }
}

void ModbusController::queue_command(const ModbusCommandItem &command) {
  if (!this->allow_duplicate_commands_) {
    // check if this command is already qeued.
    // not very effective but the queue is never really large
    for (auto &item : this->command_queue_) {
      if (item->is_equal(command)) {
        ESP_LOGW(TAG, "Duplicate modbus command found: type=0x%x address=%u count=%u",
                 static_cast<uint8_t>(command.register_type), command.register_address, command.register_count);
        // update the payload of the queued command
        // replaces a previous command
        item->payload = command.payload;
        return;
      }
    }
  }
  this->command_queue_.push_back(make_unique<ModbusCommandItem>(command));
}

void ModbusController::update_range_(RegisterRange &r) {
  ESP_LOGV(TAG, "Range : %X Size: %x (%d) skip: %d", r.start_address, r.register_count, (int) r.register_type,
           r.skip_updates_counter);
  if (r.skip_updates_counter == 0) {
    // if a custom command is used the user supplied custom_data is only available in the SensorItem.
    if (r.register_type == modbus::EntityType::CUSTOM) {
      auto sensors = this->find_sensors_(r.register_type, r.start_address);
      if (!sensors.empty()) {
        auto sensor = sensors.cbegin();
        auto command_item = ModbusCommandItem::create_custom_command(
            this, (*sensor)->custom_data,
            [this](modbus::EntityType register_type, uint16_t start_address, const std::vector<uint8_t> &data) {
              this->on_register_data(modbus::EntityType::CUSTOM, start_address, data);
            });
        command_item.register_address = (*sensor)->start_address;
        command_item.register_count = (*sensor)->register_count;
        command_item.function_code = FunctionCode::CUSTOM;
        queue_command(command_item);
      }
    } else {
      queue_command(ModbusCommandItem::create_read_command(this, r.register_type, r.start_address, r.register_count));
    }
    r.skip_updates_counter = r.skip_updates;  // reset counter to config value
  } else {
    r.skip_updates_counter--;
  }
}
//
// Queue the modbus requests to be send.
// Once we get a response to the command it is removed from the queue and the next command is send
//
void ModbusController::update() {
  if (!this->command_queue_.empty()) {
    ESP_LOGV(TAG, "%zu modbus commands already in queue", this->command_queue_.size());
  } else {
    ESP_LOGV(TAG, "Updating modbus component");
  }

  for (auto &r : this->register_ranges_) {
    ESP_LOGVV(TAG, "Updating range 0x%X", r.start_address);
    update_range_(r);
  }
}

// walk through the sensors and determine the register ranges to read
size_t ModbusController::create_register_ranges_() {
  this->register_ranges_.clear();
  if (this->sensorset_.empty()) {
    ESP_LOGW(TAG, "No sensors registered");
    return 0;
  }

  // Sensors are walked in the sensor set's order (see SensorItemsComparator): register type, then
  // force_new_range ahead of the rest, then address - so the walk is not purely address-ordered.
  // Each keeps the address it was configured with; what is resolved here is its `offset`, the position
  // of its data within the response of whichever range it ends up in.
  RegisterRange r = {};
  bool have_range = false;
  // Set while the open range belongs to a force_new_range sensor: a range the user asked to keep
  // separate must not quietly absorb other sensors.
  bool range_forced = false;
  // Set once a sensor has joined by sharing the range's start address, which widens the read. Only a
  // widened range can absorb a later sensor by coverage: ranges that were kept apart before stay apart,
  // so their frames and polling rates are untouched.
  bool range_shared = false;
  // Bytes the range's registers have consumed so far. An extending sensor starts after them, so a
  // register that returns more bytes than its count implies pushes the sensors after it along.
  // range_custom_size records whether any of them returns something other than two bytes per register,
  // which is what makes a position inside the range impossible to work out from addresses alone. Coils
  // count as such: they carry one bit per address, so bit ranges never take the coverage join.
  size_t range_bytes = 0;
  bool range_custom_size = false;
  SensorItem *prev = nullptr;
  for (SensorItem *curr : this->sensorset_) {
    ESP_LOGV(TAG, "Register: 0x%X count=%d size=%zu offset=%u skip=%u addr=%p", curr->start_address,
             curr->register_count, curr->get_register_size(), curr->offset, curr->skip_updates, curr);

    const bool custom_size = curr->get_register_size() != static_cast<size_t>(curr->register_count) * 2;

    bool join = false;
    if (have_range && !curr->force_new_range && r.register_type == curr->register_type &&
        curr->register_type != modbus::EntityType::CUSTOM) {
      if (curr->start_address == (r.start_address + r.register_count - prev->register_count) &&
          prev->start_address + prev->register_count == r.start_address + r.register_count &&
          curr->register_count == prev->register_count && curr->get_register_size() == prev->get_register_size()) {
        // A second sensor on the register(s) the previous one covers: it reads those same bytes,
        // starting where that sensor's offset pointed, so a chain configured 0/2/4 resolves to 0/2/6.
        // Both address tests matter. The first identifies the previous sensor's register by working back
        // from the range's end, which only describes it while it actually sits there - hence the second.
        // A sensor that joined mid-range must never anchor this, or the next one inherits its offset.
        curr->offset = static_cast<uint8_t>(prev->offset + curr->offset_from_start_address);
        join = true;
        ESP_LOGV(TAG, "Re-use previous register 0x%X", curr->start_address);
      } else if (curr->start_address == (r.start_address + r.register_count)) {
        // The next contiguous register(s): the data begins after what the range has consumed so far -
        // the byte cursor for registers, the distance in bits for coils.
        curr->offset =
            static_cast<uint8_t>((curr->addresses_bits() ? curr->start_address - r.start_address : range_bytes) +
                                 curr->offset_from_start_address);
        range_bytes += curr->get_register_size();
        range_custom_size = range_custom_size || custom_size;
        r.register_count += curr->register_count;
        join = true;
        ESP_LOGV(TAG, "Extend range to include 0x%X", curr->start_address);
      } else if (range_shared && !range_forced && curr->start_address >= r.start_address &&
                 curr->start_address + curr->register_count <= r.start_address + r.register_count &&
                 !range_custom_size && !custom_size && curr->skip_updates == r.skip_updates) {
        // The registers already fall inside a range that a shared-address join widened, so this sensor
        // reads its slice of that response instead of adding an overlapping second poll. The guards keep
        // it narrow: only a widened range, never a force-isolated one; only where every register in the
        // range returns two bytes, so interior positions follow from the addresses; only sensors genuinely
        // inside it, which is why the lower bound is needed given the walk is not address-ordered; and
        // only where the polling rates already match, since joining runs this sensor through the rate
        // merge below and would otherwise change one of them.
        const uint16_t addr_delta = curr->start_address - r.start_address;
        curr->offset = static_cast<uint8_t>((curr->addresses_bits() ? addr_delta : addr_delta * 2) +
                                            curr->offset_from_start_address);
        join = true;
        ESP_LOGV(TAG, "Register 0x%X already covered by range 0x%X", curr->start_address, r.start_address);
      }
    }

    // Sensors on the same start address have to share one range: a response is dispatched to a single
    // range per (start_address, register_type), so a second range with that key would never receive
    // data. This holds for force_new_range and custom entities too. The read widens to cover whichever
    // sensor needs the most registers, which also fixes a short read for coils that use offset.
    if (!join && have_range && r.register_type == curr->register_type && r.start_address == curr->start_address) {
      curr->offset = curr->offset_from_start_address;  // shares the range start
      r.register_count = std::max(r.register_count, curr->register_count);
      range_bytes = std::max(range_bytes, curr->get_register_size());
      range_custom_size = range_custom_size || custom_size;
      range_shared = true;
      range_forced = range_forced || curr->force_new_range;
      join = true;
      ESP_LOGV(TAG, "Share range start 0x%X", curr->start_address);
    }

    if (!join) {
      if (have_range) {
        ESP_LOGV(TAG, "Add range 0x%X %d skip:%d", r.start_address, r.register_count, r.skip_updates);
        this->register_ranges_.push_back(std::move(r));
      }
      r = {};
      range_bytes = curr->get_register_size();
      range_custom_size = custom_size;
      range_forced = curr->force_new_range;
      range_shared = false;
      curr->offset = curr->offset_from_start_address;
      r.start_address = curr->start_address;
      r.register_count = curr->register_count;
      r.register_type = curr->register_type;
      r.skip_updates = curr->skip_updates;
      r.skip_updates_counter = 0;
      have_range = true;
    } else if (curr->skip_updates != 0) {
      // use the lowest non-zero skip_updates for the whole range (0 is the default and is excluded)
      r.skip_updates = (r.skip_updates != 0) ? std::min(r.skip_updates, curr->skip_updates) : curr->skip_updates;
    }

    // Every member records its range's first register. The resolved offset is relative to it, so the
    // two together give the sensor's real position, and the address a write entity targets.
    curr->range_start_address = r.start_address;
    r.sensors.insert(curr);
    prev = curr;
  }
  if (have_range) {
    ESP_LOGV(TAG, "Add last range 0x%X %d skip:%d", r.start_address, r.register_count, r.skip_updates);
    this->register_ranges_.push_back(std::move(r));
  }

  return this->register_ranges_.size();
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
    ESP_LOGCONFIG(TAG, " Sensor type=%u start=0x%X offset=0x%X count=%d size=%zu",
                  static_cast<uint8_t>(it->register_type), it->start_address, it->offset, it->register_count,
                  it->get_register_size());
  }
  ESP_LOGCONFIG(TAG, "ranges");
  for (auto &it : this->register_ranges_) {
    ESP_LOGCONFIG(TAG, "  Range type=%u start=0x%X count=%d skip_updates=%d", static_cast<uint8_t>(it.register_type),
                  it.start_address, it.register_count, it.skip_updates);
  }
#endif
}

void ModbusController::loop() {
  // Incoming data to process?
  if (!this->incoming_queue_.empty()) {
    auto &message = this->incoming_queue_.front();
    if (message != nullptr)
      this->process_modbus_data_(message.get());
    this->incoming_queue_.pop();

  } else {
    // all messages processed send pending commands
    this->send_next_command_();
  }
}

void ModbusController::on_write_register_response(modbus::EntityType register_type, uint16_t start_address,
                                                  const std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "Command ACK 0x%X %d ", modbus::helpers::get_data<uint16_t>(data, 0),
           modbus::helpers::get_data<int16_t>(data, 1));
}

void ModbusController::dump_sensors_() {
  ESP_LOGV(TAG, "sensors");
  for (auto &it : this->sensorset_) {
    ESP_LOGV(TAG, "  Sensor start=0x%X count=%d size=%zu offset=%d", it->start_address, it->register_count,
             it->get_register_size(), it->offset);
  }
}

ModbusCommandItem ModbusCommandItem::create_read_command(
    ModbusController *modbusdevice, modbus::EntityType register_type, uint16_t start_address, uint16_t register_count,
    std::function<void(modbus::EntityType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
        &&handler) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.register_type = register_type;
  cmd.function_code = modbus::helpers::modbus_register_read_function(register_type);
  cmd.register_address = start_address;
  cmd.register_count = register_count;
  cmd.on_data_func = std::move(handler);
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_read_command(ModbusController *modbusdevice,
                                                         modbus::EntityType register_type, uint16_t start_address,
                                                         uint16_t register_count) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.register_type = register_type;
  cmd.function_code = modbus::helpers::modbus_register_read_function(register_type);
  cmd.register_address = start_address;
  cmd.register_count = register_count;
  cmd.on_data_func = [modbusdevice](modbus::EntityType register_type, uint16_t start_address,
                                    const std::vector<uint8_t> &data) {
    modbusdevice->on_register_data(register_type, start_address, data);
  };
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_multiple_command(ModbusController *modbusdevice,
                                                                   uint16_t start_address, uint16_t register_count,
                                                                   const std::vector<uint16_t> &values) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.register_type = modbus::EntityType::HOLDING;
  cmd.function_code = FunctionCode::WRITE_MULTIPLE_REGISTERS;
  cmd.register_address = start_address;
  cmd.register_count = register_count;
  cmd.on_data_func = [modbusdevice, cmd](modbus::EntityType register_type, uint16_t start_address,
                                         const std::vector<uint8_t> &data) {
    modbusdevice->on_write_register_response(cmd.register_type, start_address, data);
  };
  for (auto v : values) {
    auto decoded_value = decode_value(v);
    cmd.payload.push_back(decoded_value[0]);
    cmd.payload.push_back(decoded_value[1]);
  }
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_single_coil(ModbusController *modbusdevice, uint16_t address,
                                                              bool value) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.register_type = modbus::EntityType::COIL;
  cmd.function_code = FunctionCode::WRITE_SINGLE_COIL;
  cmd.register_address = address;
  cmd.register_count = 1;
  cmd.on_data_func = [modbusdevice, cmd](modbus::EntityType register_type, uint16_t start_address,
                                         const std::vector<uint8_t> &data) {
    modbusdevice->on_write_register_response(cmd.register_type, start_address, data);
  };
  cmd.payload.push_back(value ? 0xFF : 0);
  cmd.payload.push_back(0);
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_multiple_coils(ModbusController *modbusdevice, uint16_t start_address,
                                                                 const std::vector<bool> &values) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.register_type = modbus::EntityType::COIL;
  cmd.function_code = FunctionCode::WRITE_MULTIPLE_COILS;
  cmd.register_address = start_address;
  cmd.register_count = values.size();
  cmd.on_data_func = [modbusdevice, cmd](modbus::EntityType register_type, uint16_t start_address,
                                         const std::vector<uint8_t> &data) {
    modbusdevice->on_write_register_response(cmd.register_type, start_address, data);
  };

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

ModbusCommandItem ModbusCommandItem::create_write_single_command(ModbusController *modbusdevice, uint16_t start_address,
                                                                 uint16_t value) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.register_type = modbus::EntityType::HOLDING;
  cmd.function_code = FunctionCode::WRITE_SINGLE_REGISTER;
  cmd.register_address = start_address;
  cmd.register_count = 1;  // not used here anyways
  cmd.on_data_func = [modbusdevice, cmd](modbus::EntityType register_type, uint16_t start_address,
                                         const std::vector<uint8_t> &data) {
    modbusdevice->on_write_register_response(cmd.register_type, start_address, data);
  };

  auto decoded_value = decode_value(value);
  cmd.payload.push_back(decoded_value[0]);
  cmd.payload.push_back(decoded_value[1]);
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_custom_command(
    ModbusController *modbusdevice, const std::vector<uint8_t> &values,
    std::function<void(modbus::EntityType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
        &&handler) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.function_code = FunctionCode::CUSTOM;
  if (handler == nullptr) {
    cmd.on_data_func = [](modbus::EntityType register_type, uint16_t start_address, const std::vector<uint8_t> &data) {
      ESP_LOGI(TAG, "Custom Command sent");
    };
  } else {
    cmd.on_data_func = handler;
  }
  cmd.payload = values;

  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_custom_command(
    ModbusController *modbusdevice, const std::vector<uint16_t> &values,
    std::function<void(modbus::EntityType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
        &&handler) {
  ModbusCommandItem cmd = {};
  cmd.modbusdevice = modbusdevice;
  cmd.function_code = FunctionCode::CUSTOM;
  if (handler == nullptr) {
    cmd.on_data_func = [](modbus::EntityType register_type, uint16_t start_address, const std::vector<uint8_t> &data) {
      ESP_LOGI(TAG, "Custom Command sent");
    };
  } else {
    cmd.on_data_func = handler;
  }
  for (auto v : values) {
    cmd.payload.push_back((v >> 8) & 0xFF);
    cmd.payload.push_back(v & 0xFF);
  }

  return cmd;
}

bool ModbusCommandItem::send() {
  if (this->function_code != FunctionCode::CUSTOM) {
    modbusdevice->send_pdu(
        modbus::helpers::create_client_pdu(this->function_code, this->register_address, this->register_count,
                                           this->payload.empty() ? nullptr : &this->payload[0], this->payload.size()));
  } else {
    modbusdevice->send_raw(this->payload);
  }
  this->send_count_++;
  ESP_LOGV(TAG, "Command sent %d 0x%X %d send_count: %d", uint8_t(this->function_code), this->register_address,
           this->register_count, this->send_count_);
  return true;
}

bool ModbusCommandItem::is_equal(const ModbusCommandItem &other) {
  // for custom commands we have to check for identical payloads, since
  // address/count/type fields will be set to zero
  return this->function_code == FunctionCode::CUSTOM
             ? this->payload == other.payload
             : other.register_address == this->register_address && other.register_count == this->register_count &&
                   other.register_type == this->register_type && other.function_code == this->function_code;
}

}  // namespace esphome::modbus_controller
