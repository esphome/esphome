#include "modbus_controller.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::modbus_controller {

static const char *const TAG = "modbus_controller";

void ModbusController::setup() { this->create_polling_commands_(); }

void SensorItem::warn_write_buffer_deprecated_(const char *entity_name) {
  if (this->write_buffer_deprecated_warned_)
    return;
  this->write_buffer_deprecated_warned_ = true;
  ESP_LOGW(TAG,
           "%s: filling the write_lambda buffer parameter is deprecated; call a write helper / send_pdu() on the "
           "entity (item) instead. The buffer parameter is removed in 2027.2.0",
           entity_name);
}

// ---------------------------------------------------------------------------------------------------
// ModbusControllerDevice: the controller-facing behaviour shared by range commands and writer entities.
// The default callbacks below are what a writer entity uses (online tracking, retry, the on_command_sent
// trigger); ModbusCommandItem overrides on_response/error/not_sent/no_response to add dispatch/unqueue.

void ModbusControllerDevice::send_raw_frame_deprecated(std::span<const uint8_t> frame) {
  if (frame.empty())
    return;
  this->dispatched_ = true;
  this->parent_->send_pdu(frame[0], frame.subspan(1), this);
}

void ModbusControllerDevice::set_controller_(ModbusController *controller) {
  this->controller_ = controller;
  this->set_parent(controller->hub());
  this->set_address(controller->device_address());
}

void ModbusControllerDevice::notify_online_(std::span<const uint8_t> request_pdu) {
  if (this->controller_ != nullptr)
    this->controller_->set_online(true, fc_of_(request_pdu), addr_of_(request_pdu));
}

bool ModbusControllerDevice::note_no_response_(std::span<const uint8_t> request_pdu) {
  if (this->controller_ == nullptr)
    return false;
  this->controller_->increment_non_response_count();
  if (this->controller_->can_send())
    return true;  // the hub re-queues the frame it is holding; on_sent fires again on the retry
  this->controller_->set_online(false, fc_of_(request_pdu), addr_of_(request_pdu));
  return false;
}

void ModbusControllerDevice::on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) {
  this->notify_online_(request_pdu);
}

void ModbusControllerDevice::on_error(std::span<const uint8_t> request_pdu, modbus::ExceptionCode exception_code) {
  ESP_LOGE(TAG, "Modbus error function code: 0x%X register 0x%X exception: %d", fc_of_(request_pdu),
           addr_of_(request_pdu), static_cast<uint8_t>(exception_code));
  this->notify_online_(request_pdu);  // an exception is still a legitimate reply -> device is online
}

// Fired once per wire transmission (including hub re-queues from a retry), so the on_command_sent trigger
// reflects when the frame actually went out, not when it was queued. Decoded from the request PDU.
void ModbusControllerDevice::on_sent(std::span<const uint8_t> request_pdu) {
  if (this->controller_ != nullptr)
    this->controller_->command_sent(fc_of_(request_pdu), addr_of_(request_pdu));
}

bool ModbusControllerDevice::on_no_response(std::span<const uint8_t> request_pdu) {
  return this->note_no_response_(request_pdu);
}

// ---------------------------------------------------------------------------------------------------
// ModbusCommandItem: a range/custom-PDU polling command (or a deprecated factory command).

ModbusCommandItem::ModbusCommandItem(ModbusController &controller, modbus::ModbusClientHub *parent, uint8_t address,
                                     RegisterRange &&range)
    : ModbusControllerDevice(controller, parent, address),
      sensors(std::move(range.sensors)),
      skip_updates(range.skip_updates),
      register_type_(range.register_type),
      start_address_(range.start_address),
      register_count_(range.register_count),
      function_code_(modbus::helpers::modbus_register_read_function(range.register_type)) {}

ModbusCommandItem::ModbusCommandItem(ModbusController &controller, modbus::ModbusClientHub *parent, uint8_t address,
                                     SensorItem *sensor)
    : ModbusControllerDevice(controller, parent, address),
      skip_updates(sensor->skip_updates),
      start_address_(sensor->start_address),
      register_count_(sensor->register_count),
      custom_pdu_(&sensor->custom_pdu) {
  this->sensors.insert(sensor);
}

// The base deletes copy/move; command items re-provide construction for value storage. The moved-from
// device must not unregister the hub slot we just took over, so its parent_ is cleared. The copy
// constructor exists only for the queue_command() compatibility path (it copies its const-ref
// argument); remove it when queue_command() is removed.
ModbusCommandItem::ModbusCommandItem(const ModbusCommandItem &other)
    : ModbusControllerDevice(other.parent_, other.address_, other.controller_),
      sensors(other.sensors),
      skip_updates(other.skip_updates),
      register_type_(other.register_type_),
      start_address_(other.start_address_),
      register_count_(other.register_count_),
      function_code_(other.function_code_),
      custom_pdu_(other.custom_pdu_),
      payload_is_raw_frame_(other.payload_is_raw_frame_) {
  // SmallInlineBuffer is move-only, so deep-copy the bytes explicitly.
  memcpy(this->payload.init(other.payload.size()), other.payload.data(), other.payload.size());
}

ModbusCommandItem::ModbusCommandItem(ModbusCommandItem &&other) noexcept
    : ModbusControllerDevice(other.parent_, other.address_, other.controller_),
      sensors(std::move(other.sensors)),
      skip_updates(other.skip_updates),
      payload(std::move(other.payload)),
      register_type_(other.register_type_),
      start_address_(other.start_address_),
      register_count_(other.register_count_),
      function_code_(other.function_code_),
      custom_pdu_(other.custom_pdu_),
      payload_is_raw_frame_(other.payload_is_raw_frame_) {
  other.parent_ = nullptr;
}

// A valid read response: dispatch the payload to the range's sensors. (notify_online_ via the base.)
void ModbusCommandItem::on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) {
  this->notify_online_(request_pdu);
  auto data = modbus::helpers::server_pdu_payload(response_pdu);
  if (!modbus::helpers::is_function_code_write(static_cast<uint8_t>(this->function_code_))) {
    for (auto *sensor : this->sensors)
      sensor->parse_and_publish(data);
  }  // else: write acknowledgement - nothing to publish
  if (this->controller_ != nullptr)
    this->controller_->unqueue_command(this);
}

void ModbusCommandItem::on_error(std::span<const uint8_t> request_pdu, modbus::ExceptionCode exception_code) {
  ModbusControllerDevice::on_error(request_pdu, exception_code);  // log + notify_online_
  if (this->controller_ != nullptr)
    this->controller_->unqueue_command(this);
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

bool ModbusCommandItem::on_no_response(std::span<const uint8_t> request_pdu) {
  if (this->note_no_response_(request_pdu))
    return true;
  if (this->controller_ != nullptr)
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
  if (!this->one_shot_command_items_.back()->send())
    this->one_shot_command_items_.back()->pending_removal = true;
}

void ModbusController::unqueue_command(const ModbusCommandItem *command) {
  // Called as the last action of the command's own callback, and from send() after send_pdu (which may
  // synchronously call on_not_sent). Destroying `command` here would leave send() and the hub touching a
  // freed object, so we only FLAG it; sweep_completed_one_shots_() erases it later at a safe point. No-op
  // for polling commands (they persist and are not in the one-shot list).
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

void ModbusController::update() {
  this->sweep_completed_one_shots_();  // reclaim one-shots deferred out of their own callbacks
  if (this->module_offline_) {
    if (static_cast<uint16_t>(this->update_counter_ + 1 - this->module_offline_at_) %
            (this->offline_skip_updates_ + 1) !=
        0) {
      ESP_LOGV(TAG, "Module offline - skipping update");
    } else {  // time to try the device again
      ESP_LOGV(TAG, "Module offline - retrying");
      this->cmd_non_responses_ = 0;  // allow a retry attempt through can_send()
    }
  }

  if (this->can_send()) {
    for (auto &cmd : this->polling_command_items_) {
      if (this->update_counter_ % (cmd.skip_updates + 1) != 0) {
        ESP_LOGVV(TAG, "Skipping update for range 0x%X", cmd.register_address());
        continue;
      }
      ESP_LOGVV(TAG, "Updating range 0x%X", cmd.register_address());
      cmd.send({.continuous = this->continuous_});
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

ModbusCommandItem ModbusCommandItem::create_write_multiple_command(ModbusController *controller, uint16_t start_address,
                                                                   uint16_t register_count,
                                                                   const std::vector<uint16_t> &values) {
  ModbusCommandItem cmd(*controller, controller->hub(), controller->device_address());
  cmd.set_command_(FunctionCode::WRITE_MULTIPLE_REGISTERS, EntityType::HOLDING, start_address, values.size());
  auto pdu = modbus::helpers::create_write_registers_pdu(start_address, values);
  memcpy(cmd.payload.init(pdu.size()), pdu.data(), pdu.size());
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_single_coil(ModbusController *controller, uint16_t address,
                                                              bool value) {
  ModbusCommandItem cmd(*controller, controller->hub(), controller->device_address());
  cmd.set_command_(FunctionCode::WRITE_SINGLE_COIL, EntityType::COIL, address, 1);
  auto pdu = modbus::helpers::create_write_single_coil_pdu(address, value);
  memcpy(cmd.payload.init(pdu.size()), pdu.data(), pdu.size());
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_multiple_coils(ModbusController *controller, uint16_t start_address,
                                                                 const std::vector<bool> &values) {
  ModbusCommandItem cmd(*controller, controller->hub(), controller->device_address());
  cmd.set_command_(FunctionCode::WRITE_MULTIPLE_COILS, EntityType::COIL, start_address, values.size());
  // std::vector<bool> is bit-packed and cannot bind to std::span<const bool>; pack it and use the packed builder.
  StaticVector<uint8_t, (modbus::MAX_NUM_OF_COILS_TO_WRITE + 7) / 8> packed;
  const size_t packable = std::min<size_t>(values.size(), modbus::MAX_NUM_OF_COILS_TO_WRITE);
  for (size_t i = 0; i != packable; i++) {
    if (i % 8 == 0)
      packed.push_back(0);
    if (values[i])
      packed[i / 8] |= (1 << (i % 8));
  }
  auto pdu = modbus::helpers::create_write_coils_pdu(
      start_address, modbus::PackedBits(std::span<const uint8_t>(packed.data(), packed.size()), values.size()));
  memcpy(cmd.payload.init(pdu.size()), pdu.data(), pdu.size());
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_single_command(ModbusController *controller, uint16_t start_address,
                                                                 uint16_t value) {
  ModbusCommandItem cmd(*controller, controller->hub(), controller->device_address());
  cmd.set_command_(FunctionCode::WRITE_SINGLE_REGISTER, EntityType::HOLDING, start_address, 1);
  auto pdu = modbus::helpers::create_write_single_register_pdu(start_address, value);
  memcpy(cmd.payload.init(pdu.size()), pdu.data(), pdu.size());
  return cmd;
}

// Shared non-warning impl for the deprecated overloads; `values` is a legacy raw frame (address + function
// code + data) sent to its own address byte, CRC added by the hub.
ModbusCommandItem ModbusCommandItem::custom_command_impl(ModbusController *controller,
                                                         std::span<const uint8_t> values) {
  ModbusCommandItem cmd(*controller, controller->hub(), controller->device_address());
  memcpy(cmd.payload.init(values.size()), values.data(), values.size());
  cmd.payload_is_raw_frame_ = true;
  // Best-effort decode of the raw frame's header (address + PDU) so logs get the same metadata as a
  // standard command.
  if (values.size() >= 4) {
    cmd.set_command_(static_cast<FunctionCode>(values[1]), modbus::helpers::entity_type_from_function_code(values[1]),
                     modbus::helpers::get_data<uint16_t>(values.data(), 2), 0);
  } else if (values.size() >= 2) {
    cmd.function_code_ = static_cast<FunctionCode>(values[1]);
    cmd.register_type_ = modbus::helpers::entity_type_from_function_code(values[1]);
  }
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_custom_command(ModbusController *controller,
                                                           const std::vector<uint8_t> &values) {
  return custom_command_impl(controller, values);
}

ModbusCommandItem ModbusCommandItem::create_custom_command(ModbusController *controller,
                                                           const std::vector<uint16_t> &values) {
  StaticVector<uint8_t, modbus::MAX_RAW_SIZE> bytes;
  for (auto v : values) {
    bytes.push_back((v >> 8) & 0xFF);
    bytes.push_back(v & 0xFF);
  }
  return custom_command_impl(controller, std::span<const uint8_t>(bytes.data(), bytes.size()));
}

bool ModbusCommandItem::send(modbus::CommandOptions options) {
  // Options pass straight through to the hub (which ignores continuous for writes). The polling loop in
  // update() sets continuous from the controller's continuous mode; one-shot commands keep the default.
  bool accepted;
  if (this->custom_pdu_ != nullptr) {
    // Custom polling command: the ready-made PDU bytes live in the sensor.
    accepted = modbus::ModbusClientDevice::send_pdu(std::span<const uint8_t>(*this->custom_pdu_), options);
  } else if (!this->payload.empty()) {
    if (this->payload_is_raw_frame_) {
      // Legacy raw frame staged by create_custom_command(): route the PDU to the frame's own address byte.
      std::span<const uint8_t> frame = this->payload;
      accepted = this->parent_->send_pdu(frame[0], frame.subspan(1), this, options);
    } else {
      // Full PDU staged by one of the deprecated create_* factories.
      accepted = modbus::ModbusClientDevice::send_pdu(this->payload, options);
    }
  } else {
    // Read command: dispatch by entity type through the base's typed read helper.
    accepted = this->read_entities(this->register_type_, this->start_address_, this->register_count_, options);
  }
  // The on_command_sent trigger fires from on_sent() when the frame actually reaches the wire.
  if (accepted) {
    ESP_LOGV(TAG, "Command queued %d 0x%X %d", uint8_t(this->function_code_), this->start_address_,
             this->register_count_);
  }
  return accepted;
}

}  // namespace esphome::modbus_controller
