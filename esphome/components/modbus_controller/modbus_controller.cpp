#include "modbus_controller.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome::modbus_controller {

static const char *const TAG = "modbus_controller";

void ModbusController::setup() { this->create_polling_commands_(); }

ModbusCommandItem::ModbusCommandItem(ModbusController &controller, modbus::ModbusClientHub *parent, uint8_t address,
                                     RegisterRange &&range)
    : modbus::ModbusClientDevice(parent, address),
      sensors(std::move(range.sensors)),
      skip_updates(range.skip_updates),
      register_type_(range.register_type),
      start_address_(range.start_address),
      register_count_(range.register_count),
      function_code_(modbus::helpers::modbus_register_read_function(range.register_type)),
      controller_(&controller) {}

ModbusCommandItem::ModbusCommandItem(ModbusController &controller, modbus::ModbusClientHub *parent, uint8_t address,
                                     SensorItem *sensor)
    : modbus::ModbusClientDevice(parent, address),
      skip_updates(sensor->skip_updates),
      start_address_(sensor->start_address),
      register_count_(sensor->register_count),
      function_code_(FunctionCode::CUSTOM),
      custom_data_(&sensor->custom_data),
      controller_(&controller) {
  this->sensors.insert(sensor);
}

// The base deletes copy/move; command items re-provide construction. The moved-from device must not
// unregister the hub slot we just took over, so its parent_ is cleared. The copy constructor exists
// only for callers that pass an lvalue to queue_command() (in-tree callers move); remove it when
// queue_command() is removed.
ModbusCommandItem::ModbusCommandItem(const ModbusCommandItem &other)
    : modbus::ModbusClientDevice(other.parent_, other.address_),
      sensors(other.sensors),
      skip_updates(other.skip_updates),
      on_data_func(other.on_data_func),
      register_type_(other.register_type_),
      start_address_(other.start_address_),
      register_count_(other.register_count_),
      function_code_(other.function_code_),
      custom_data_(other.custom_data_),
      controller_(other.controller_) {
  // SmallInlineBuffer is move-only, so deep-copy the bytes explicitly.
  this->payload.set(other.payload.data(), other.payload.size());
}

ModbusCommandItem::ModbusCommandItem(ModbusCommandItem &&other) noexcept
    : modbus::ModbusClientDevice(other.parent_, other.address_),
      sensors(std::move(other.sensors)),
      skip_updates(other.skip_updates),
      on_data_func(std::move(other.on_data_func)),
      payload(std::move(other.payload)),
      register_type_(other.register_type_),
      start_address_(other.start_address_),
      register_count_(other.register_count_),
      function_code_(other.function_code_),
      custom_data_(other.custom_data_),
      controller_(other.controller_) {
  other.parent_ = nullptr;
}

// A valid response: the device is online. Dispatch the payload to the handler or the range's sensors.
void ModbusCommandItem::on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) {
  if (this->controller_ != nullptr)
    this->controller_->set_online(true, static_cast<int>(this->function_code_), this->start_address_);
  auto data = modbus::helpers::server_pdu_payload(response_pdu);
  if (this->on_data_func) {
    this->on_data_func(this->register_type_, this->start_address_, data);
  } else if (modbus::helpers::is_function_code_write(static_cast<uint8_t>(this->function_code_))) {
    // write acknowledgement - nothing to publish
  } else {
    for (auto *sensor : this->sensors)
      sensor->parse_and_publish(data);
  }
  if (this->controller_ != nullptr)
    this->controller_->unqueue_command(this);
}

// An exception response is still a legitimate reply, so the device is considered online.
void ModbusCommandItem::on_error(std::span<const uint8_t> request_pdu, modbus::ExceptionCode exception_code) {
  const uint8_t function_code = request_pdu.empty() ? 0 : request_pdu[0];
  ESP_LOGW(TAG, "Modbus error function code: 0x%X register 0x%X exception: %d", function_code, this->start_address_,
           static_cast<uint8_t>(exception_code));
  if (this->controller_ != nullptr) {
    this->controller_->set_online(true, function_code, this->start_address_);
    this->controller_->unqueue_command(this);
  }
}

// Not being sent says nothing about online/offline status; just drop it from the pending list.
void ModbusCommandItem::on_not_sent(std::span<const uint8_t> request_pdu) {
  // A dropped write is lost while the entity has already published optimistically, so surface it.
  if (modbus::helpers::is_function_code_write(static_cast<uint8_t>(this->function_code_))) {
    ESP_LOGW(TAG, "Write not sent: function 0x%X register 0x%X", static_cast<uint8_t>(this->function_code_),
             this->start_address_);
  }
  if (this->controller_ != nullptr)
    this->controller_->unqueue_command(this);
}

// Fired once per wire transmission (including hub re-queues from a retry), so the on_command_sent
// trigger reflects when the frame actually went out, not when it was queued.
void ModbusCommandItem::on_sent(std::span<const uint8_t> request_pdu) {
  if (this->controller_ == nullptr)
    return;
  this->controller_->command_sent(static_cast<int>(this->function_code_), this->start_address_);
  // A broadcast (address 0) is never answered (Modbus 4.1), so the hub delivers no terminal callback.
  // on_sent is this command's only callback, so drop the one-shot from the queue here, or it would leak.
  // Test the address the frame went to, not address_: a custom command's frame carries its own address
  // (frame[0]), which may differ from this controller's. (unqueue_command() is a no-op for a poll.)
  uint8_t wire_address = this->address_;
  if (this->function_code_ == FunctionCode::CUSTOM) {
    std::span<const uint8_t> frame =
        this->custom_data_ != nullptr ? std::span<const uint8_t>(*this->custom_data_) : this->payload;
    if (!frame.empty())
      wire_address = frame[0];
  }
  if (wire_address == modbus::BROADCAST_ADDRESS)
    this->controller_->unqueue_command(this);
}

bool ModbusCommandItem::on_no_response(std::span<const uint8_t> request_pdu) {
  if (this->controller_ == nullptr)
    return false;
  this->controller_->increment_non_response_count();
  if (this->controller_->can_send()) {
    // Have the hub re-queue the frame it is holding; on_sent fires again when it goes back out.
    return true;
  }
  this->controller_->set_online(false, static_cast<int>(this->function_code_), this->start_address_);
  this->controller_->unqueue_command(this);
  return false;
}

void ModbusController::set_online(bool online, int function_code, int register_address) {
  if (online) {
    this->cmd_non_responses_ = 0;
    if (this->module_offline_) {
      ESP_LOGW(TAG, "Modbus device=%d back online", this->address_);
      this->module_offline_ = false;
      this->online_callback_.call(function_code, register_address);
    }
  } else {
    // Offline is a property of the physical device, so drop every sender's queued frames for its
    // address; retired frames get on_not_sent(), which reclaims one-shots through the normal path.
    this->hub_->clear_tx_queue_for_address(this->address_);
    if (!this->module_offline_) {
      ESP_LOGW(TAG, "Modbus device=%d set offline", this->address_);
      this->module_offline_ = true;
      this->module_offline_at_ = this->update_counter_;
      this->offline_callback_.call(function_code, register_address);
    }
  }
}

void ModbusController::queue_command(ModbusCommandItem command) {
  this->sweep_completed_one_shots_();  // reclaim finished one-shots before adding a new one
  // Duplicates are the caller's to manage; the controller only holds the item until its terminal callback.
  this->one_shot_command_items_.push_back(make_unique<ModbusCommandItem>(std::move(command)));
  // A refused frame gets no terminal callback (see the hub contract), so reclaim the item here.
  auto &item = this->one_shot_command_items_.back();
  if (!item->send()) {
    // The caller (e.g. a write entity) has usually already published optimistically - surface the loss.
    ESP_LOGW(TAG, "Command refused by hub: type=0x%X address=0x%X", static_cast<uint8_t>(item->register_type()),
             item->register_address());
    item->pending_removal = true;
  }
}

void ModbusController::unqueue_command(const ModbusCommandItem *command) {
  // Called as the last action of the command's own callback (on_response/on_error/on_not_sent/
  // on_no_response), which the hub runs from inside its sweep while this entry is still live.
  // Destroying `command` here would leave the hub touching a freed object, so we only FLAG it;
  // sweep_completed_one_shots_() erases it later at a safe point. No-op for polling commands
  // (they persist and are not in the one-shot list).
  for (auto &item : this->one_shot_command_items_) {
    if (item.get() == command) {
      item->pending_removal = true;
      return;
    }
  }
}

void ModbusController::sweep_completed_one_shots_() {
  this->one_shot_command_items_.remove_if(
      [](const std::unique_ptr<ModbusCommandItem> &item) { return item->pending_removal; });
}

void ModbusController::update_range_(ModbusCommandItem &cmd) {
  if (this->update_counter_ % (cmd.skip_updates + 1) != 0) {
    ESP_LOGVV(TAG, "Skipping update for range 0x%X", cmd.register_address());
    return;
  }
  // A refusal is already logged by the hub; note the affected range for controller-level diagnostics.
  if (!cmd.send())
    ESP_LOGD(TAG, "Poll refused by hub for range 0x%X", cmd.register_address());
}

void ModbusController::update() {
  this->sweep_completed_one_shots_();  // reclaim one-shots deferred out of their own callbacks
  if (this->module_offline_) {
    // Offline probing follows the offline cadence alone; per-range skip_updates resumes once the
    // device is back online. Requiring both cadences to coincide would leave phase combinations
    // where a probe never goes out.
    if (offline_retry_due(this->update_counter_, this->module_offline_at_, this->offline_skip_updates_)) {
      ESP_LOGV(TAG, "Module offline - retrying");
      this->cmd_non_responses_ = 0;  // allow the probe through can_send()
      for (auto &cmd : this->polling_command_items_) {
        if (!cmd.send())
          ESP_LOGD(TAG, "Probe refused by hub for range 0x%X", cmd.register_address());
      }
    } else {
      ESP_LOGV(TAG, "Module offline - skipping update");
    }
    this->update_counter_++;
    return;
  }

  if (this->can_send()) {
    for (auto &cmd : this->polling_command_items_) {
      ESP_LOGVV(TAG, "Updating range 0x%X", cmd.register_address());
      this->update_range_(cmd);
    }
  }
  this->update_counter_++;
}

// walk through the sensors and determine the register ranges to read
void ModbusController::create_polling_commands_() {
  if (this->sensorset_.empty()) {
    ESP_LOGW(TAG, "No sensors registered");
    return;
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
        this->create_polling_command_(std::move(r));
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
    this->create_polling_command_(std::move(r));
  }
  // Reclaim growth slack; safe here because nothing has registered with the hub yet (see the
  // lifetime note on polling_command_items_).
  this->polling_command_items_.shrink_to_fit();
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
  for (auto &it : this->polling_command_items_) {
    ESP_LOGCONFIG(TAG, "  Range type=%u start=0x%X count=%d skip_updates=%d", static_cast<uint8_t>(it.register_type()),
                  it.register_address(), it.register_count(), it.skip_updates);
  }
#endif
}

void ModbusController::on_write_register_response(EntityType register_type, uint16_t start_address,
                                                  std::span<const uint8_t> data) {
  // A well-formed write ACK echoes address and value, but a truncated PDU yields a short/empty span.
  if (data.size() >= 3) {
    ESP_LOGV(TAG, "Command ACK 0x%X %d ", modbus::helpers::get_data<uint16_t>(data.data(), 0),
             modbus::helpers::get_data<int16_t>(data.data(), 1));
  } else {
    ESP_LOGV(TAG, "Command ACK (short payload, %zu bytes)", data.size());
  }
}

ModbusCommandItem ModbusCommandItem::create_read_command(
    ModbusController *modbusdevice, EntityType register_type, uint16_t start_address, uint16_t register_count,
    std::function<void(EntityType register_type, uint16_t start_address, std::span<const uint8_t> data)> &&handler) {
  ModbusCommandItem cmd(*modbusdevice, modbusdevice->hub(), modbusdevice->device_address());
  cmd.set_command_(modbus::helpers::modbus_register_read_function(register_type), register_type, start_address,
                   register_count);
  cmd.on_data_func = std::move(handler);
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_multiple_command(ModbusController *modbusdevice,
                                                                   uint16_t start_address, uint16_t register_count,
                                                                   const std::vector<uint16_t> &values) {
  ModbusCommandItem cmd(*modbusdevice, modbusdevice->hub(), modbusdevice->device_address());
  cmd.set_command_(FunctionCode::WRITE_MULTIPLE_REGISTERS, EntityType::HOLDING, start_address, register_count);
  cmd.on_data_func = [modbusdevice](EntityType register_type, uint16_t start_address, std::span<const uint8_t> data) {
    modbusdevice->on_write_register_response(register_type, start_address, data);
  };
  uint8_t *p = cmd.payload.init(values.size() * 2);
  for (auto v : values) {
    auto decoded_value = decode_value(v);
    *p++ = decoded_value[0];
    *p++ = decoded_value[1];
  }
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_single_coil(ModbusController *modbusdevice, uint16_t address,
                                                              bool value) {
  ModbusCommandItem cmd(*modbusdevice, modbusdevice->hub(), modbusdevice->device_address());
  cmd.set_command_(FunctionCode::WRITE_SINGLE_COIL, EntityType::COIL, address, 1);
  cmd.on_data_func = [modbusdevice](EntityType register_type, uint16_t start_address, std::span<const uint8_t> data) {
    modbusdevice->on_write_register_response(register_type, start_address, data);
  };
  uint8_t *p = cmd.payload.init(2);
  p[0] = value ? 0xFF : 0;
  p[1] = 0;
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_multiple_coils(ModbusController *modbusdevice, uint16_t start_address,
                                                                 const std::vector<bool> &values) {
  ModbusCommandItem cmd(*modbusdevice, modbusdevice->hub(), modbusdevice->device_address());
  cmd.set_command_(FunctionCode::WRITE_MULTIPLE_COILS, EntityType::COIL, start_address, values.size());
  cmd.on_data_func = [modbusdevice](EntityType register_type, uint16_t start_address, std::span<const uint8_t> data) {
    modbusdevice->on_write_register_response(register_type, start_address, data);
  };

  // Pack through the shared bit view (MutablePackedBits) so the coil wire layout lives in one place
  // instead of an open-coded loop.
  const size_t byte_count = modbus::packed_bit_bytes(values.size());
  uint8_t *p = cmd.payload.init(byte_count);
  memset(p, 0, byte_count);
  modbus::MutablePackedBits bits(std::span<uint8_t>(p, byte_count), static_cast<uint16_t>(values.size()));
  for (size_t i = 0; i != values.size(); i++) {
    if (values[i])
      bits.set(i, true);
  }
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_single_command(ModbusController *modbusdevice, uint16_t start_address,
                                                                 uint16_t value) {
  ModbusCommandItem cmd(*modbusdevice, modbusdevice->hub(), modbusdevice->device_address());
  cmd.set_command_(FunctionCode::WRITE_SINGLE_REGISTER, EntityType::HOLDING, start_address, 1);
  cmd.on_data_func = [modbusdevice](EntityType register_type, uint16_t start_address, std::span<const uint8_t> data) {
    modbusdevice->on_write_register_response(register_type, start_address, data);
  };

  auto decoded_value = decode_value(value);
  uint8_t *p = cmd.payload.init(2);
  p[0] = decoded_value[0];
  p[1] = decoded_value[1];
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_custom_command(
    ModbusController *modbusdevice, const std::vector<uint8_t> &values,
    std::function<void(EntityType register_type, uint16_t start_address, std::span<const uint8_t> data)> &&handler) {
  ModbusCommandItem cmd(*modbusdevice, modbusdevice->hub(), modbusdevice->device_address());
  cmd.function_code_ = FunctionCode::CUSTOM;
  if (handler == nullptr) {
    cmd.on_data_func = [](EntityType register_type, uint16_t start_address, std::span<const uint8_t> data) {
      ESP_LOGI(TAG, "Custom Command sent");
    };
  } else {
    cmd.on_data_func = handler;
  }
  cmd.payload.set(values.data(), values.size());

  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_custom_command(
    ModbusController *modbusdevice, const std::vector<uint16_t> &values,
    std::function<void(EntityType register_type, uint16_t start_address, std::span<const uint8_t> data)> &&handler) {
  ModbusCommandItem cmd(*modbusdevice, modbusdevice->hub(), modbusdevice->device_address());
  cmd.function_code_ = FunctionCode::CUSTOM;
  if (handler == nullptr) {
    cmd.on_data_func = [](EntityType register_type, uint16_t start_address, std::span<const uint8_t> data) {
      ESP_LOGI(TAG, "Custom Command sent");
    };
  } else {
    cmd.on_data_func = handler;
  }
  uint8_t *p = cmd.payload.init(values.size() * 2);
  for (auto v : values) {
    *p++ = (v >> 8) & 0xFF;
    *p++ = v & 0xFF;
  }

  return cmd;
}

bool ModbusCommandItem::send() {
  bool accepted;
  if (this->function_code_ != FunctionCode::CUSTOM) {
    accepted = this->queue_pdu(modbus::helpers::create_client_pdu(
        this->function_code_, this->start_address_, this->register_count_,
        this->payload.empty() ? nullptr : this->payload.data(), this->payload.size()));
  } else {
    // Custom command: the bytes are a complete raw frame (address + PDU). Send the PDU to the frame's own
    // address (which may differ from this controller's); the hub appends the CRC and routes the response
    // back to this item by pointer. (send_raw() is deprecated, so queue_pdu() is called with the extracted
    // address. Raw-frame semantics are kept here; the custom_pdu migration is a later step.)
    std::span<const uint8_t> frame =
        this->custom_data_ != nullptr ? std::span<const uint8_t>(*this->custom_data_) : this->payload;
    if (frame.empty()) {
      ESP_LOGW(TAG, "Empty custom command frame, not sent");
      accepted = false;
    } else {
      accepted = this->parent_->queue_pdu(frame[0], frame.subspan(1), this);
    }
  }
  // The on_command_sent trigger fires from on_sent() when the frame actually reaches the wire.
  if (accepted) {
    ESP_LOGV(TAG, "Command queued %d 0x%X %d", uint8_t(this->function_code_), this->start_address_,
             this->register_count_);
  }
  return accepted;
}

}  // namespace esphome::modbus_controller
