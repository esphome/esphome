#include "modbus_controller.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cstring>
#include <limits>

namespace esphome::modbus_controller {

static const char *const TAG = "modbus_controller";

void ModbusController::setup() { this->create_polling_commands_(); }

void WriterDevice::warn_write_buffer_deprecated(const LogString *platform, uint16_t address) {
  if (this->write_buffer_deprecated_warned_)
    return;
  this->write_buffer_deprecated_warned_ = true;
  ESP_LOGW(TAG,
           "Modbus %s (address 0x%X): filling the write_lambda buffer parameter is deprecated; call a write helper / "
           "queue_pdu() on the entity (item) instead. The buffer parameter is removed in 2027.3.0",
           LOG_STR_ARG(platform), address);
}

bool WriterDevice::send_raw_frame_deprecated(std::span<const uint8_t> frame) {
  if (frame.empty())
    return false;
  return this->parent_->queue_pdu(frame[0], frame.subspan(1), this);
}

void ControllerDevice::set_controller(ModbusController *controller) {
  this->controller_ = controller;
  this->set_parent(controller->hub());
  this->set_address(controller->device_address());
}

// A request whose layout carries no start address (a custom PDU) reports -1; 0 stays a real address.
static int trigger_address(std::span<const uint8_t> request_pdu) {
  const auto addr = modbus::helpers::client_pdu_start_address(request_pdu);
  return addr.has_value() ? *addr : -1;
}

void ControllerDevice::notify_online_(std::span<const uint8_t> request_pdu) {
  if (this->controller_ != nullptr) {
    this->controller_->set_online(true, modbus::helpers::pdu_function_code(request_pdu), trigger_address(request_pdu));
  }
}

void ControllerDevice::on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) {
  this->notify_online_(request_pdu);
}

void ControllerDevice::on_error(std::span<const uint8_t> request_pdu, modbus::ExceptionCode exception_code) {
  ESP_LOGW(TAG, "Modbus error function code: 0x%X register %d exception: %d",
           modbus::helpers::pdu_function_code(request_pdu), trigger_address(request_pdu),
           static_cast<uint8_t>(exception_code));
  this->notify_online_(request_pdu);  // an exception is still a legitimate reply -> device is online
}

void WriterDevice::on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) {
  ControllerDevice::on_response(request_pdu, response_pdu);
  this->dispatch_response_(request_pdu, response_pdu, std::nullopt);
}

void WriterDevice::on_error(std::span<const uint8_t> request_pdu, modbus::ExceptionCode exception_code) {
  ControllerDevice::on_error(request_pdu, exception_code);
  this->dispatch_response_(request_pdu, {}, exception_code);
}

// Fired once per wire transmission (including hub re-queues from a retry), so the on_command_sent trigger
// reflects when the frame actually went out, not when it was queued.
void ControllerDevice::on_sent(std::span<const uint8_t> request_pdu) {
  if (this->controller_ != nullptr) {
    this->controller_->command_sent(modbus::helpers::pdu_function_code(request_pdu), trigger_address(request_pdu));
  }
}

void ControllerDevice::on_not_sent(std::span<const uint8_t> request_pdu) {
  const uint8_t fc = modbus::helpers::pdu_function_code(request_pdu);
  const int addr = trigger_address(request_pdu);
  // Only the offline teardown reaches this (a supersede retires silently), so the frame is genuinely
  // lost; a dropped write was already published optimistically, so surface it.
  if (modbus::helpers::is_function_code_write(fc)) {
    ESP_LOGW(TAG, "Write not sent: function 0x%X register %d", fc, addr);
  } else {
    ESP_LOGD(TAG, "Request not sent: function 0x%X register %d", fc, addr);
  }
}

bool ControllerDevice::on_no_response(std::span<const uint8_t> request_pdu) {
  if (this->controller_ == nullptr)
    return false;
  this->controller_->increment_non_response_count();
  if (this->controller_->can_send())
    return true;  // the hub re-queues the frame it is holding; on_sent fires again on the retry
  this->controller_->set_online(false, modbus::helpers::pdu_function_code(request_pdu), trigger_address(request_pdu));
  return false;
}

PollingDevice::PollingDevice(ModbusController &controller, RegisterRange &&range)
    : ControllerDevice(&controller), range_(std::move(range)) {}

bool PollingDevice::queue(modbus::CommandOptions options) {
  bool accepted;
  if (this->range_.custom_pdu != nullptr) {
    accepted = this->queue_pdu(std::span<const uint8_t>(*this->range_.custom_pdu), options);
  } else {
    accepted = this->read_entities(this->range_.register_type, this->range_.start_address, this->range_.register_count,
                                   options);
  }
  if (accepted) {
    ESP_LOGV(TAG, "Poll queued type=%u 0x%X %d", static_cast<uint8_t>(this->range_.register_type),
             this->range_.start_address, this->range_.register_count);
  }
  return accepted;
}

void PollingDevice::on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) {
  this->notify_online_(request_pdu);
  auto data = modbus::helpers::server_pdu_payload(response_pdu);
  for (auto *sensor : this->range_.sensors)
    sensor->parse_and_publish(data);
}

// ModbusCommandItem's machinery stays as-is until its removal in 2027.3.0; silence its self-references.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
ModbusCommandItem::ModbusCommandItem(ModbusController &controller, modbus::ModbusClientHub *parent, uint8_t address,
                                     RegisterRange &&range)
    : modbus::ModbusClientDevice(parent, address),
      sensors(std::move(range.sensors)),
      register_type_(range.register_type),
      start_address_(range.start_address),
      register_count_(range.register_count),
      function_code_(modbus::helpers::modbus_register_read_function(range.register_type)),
      controller_(&controller) {}

ModbusCommandItem::ModbusCommandItem(ModbusController &controller, modbus::ModbusClientHub *parent, uint8_t address,
                                     SensorItem *sensor)
    : modbus::ModbusClientDevice(parent, address),
      start_address_(sensor->start_address),
      register_count_(sensor->entity_count()),
      custom_pdu_(&sensor->custom_pdu),
      controller_(&controller) {
  // The PDU's first byte is its real function code; carry it so dump_config, the on_command_sent
  // trigger and the response callbacks report the actual code instead of CUSTOM.
  if (!sensor->custom_pdu.empty())
    this->function_code_ = static_cast<FunctionCode>(sensor->custom_pdu.data()[0]);
  this->sensors.insert(sensor);
}

// The base deletes copy/move; command items re-provide construction. The moved-from device must not
// unregister the hub slot we just took over, so its parent_ is cleared. The copy constructor exists
// only for callers that pass an lvalue to queue_command() (in-tree callers move); remove it when
// queue_command() is removed.
ModbusCommandItem::ModbusCommandItem(const ModbusCommandItem &other)
    : modbus::ModbusClientDevice(other.parent_, other.address_),
      sensors(other.sensors),
      on_data_func(other.on_data_func),
      register_type_(other.register_type_),
      start_address_(other.start_address_),
      register_count_(other.register_count_),
      function_code_(other.function_code_),
      custom_pdu_(other.custom_pdu_),
      controller_(other.controller_) {
  // SmallInlineBuffer is move-only, so deep-copy the bytes explicitly.
  this->payload.set(other.payload.data(), other.payload.size());
}

ModbusCommandItem::ModbusCommandItem(ModbusCommandItem &&other) noexcept
    : modbus::ModbusClientDevice(other.parent_, other.address_),
      sensors(std::move(other.sensors)),
      on_data_func(std::move(other.on_data_func)),
      payload(std::move(other.payload)),
      register_type_(other.register_type_),
      start_address_(other.start_address_),
      register_count_(other.register_count_),
      function_code_(other.function_code_),
      custom_pdu_(other.custom_pdu_),
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
  } else if (!this->sensors.empty()) {
    // A polling command always has sensors; a factory/write command never does. Test this before the
    // write-code branch so a custom_pdu whose function code is a write (e.g. 0x17, whose response
    // carries read data) still reaches its sensor instead of being treated as a bare write ack.
    for (auto *sensor : this->sensors)
      sensor->parse_and_publish(data);
  } else if (modbus::helpers::is_function_code_write(static_cast<uint8_t>(this->function_code_))) {
    // write acknowledgement - nothing to publish
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
  // A custom polling command sends its PDU to this controller's own address, so only a factory custom
  // command (a raw frame staged in payload) can carry a different address byte.
  uint8_t wire_address = this->address_;
  if (this->function_code_ == FunctionCode::CUSTOM && !this->payload.empty())
    wire_address = this->payload.data()[0];
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

#pragma GCC diagnostic pop

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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
void ModbusController::queue_command(ModbusCommandItem command) {
  this->sweep_completed_one_shots_();  // reclaim finished one-shots before adding a new one
  // Duplicates are the caller's to manage; the controller only holds the item until its terminal callback.
  this->one_shot_command_items_.push_back(make_unique<ModbusCommandItem>(std::move(command)));
  // A refused frame gets no terminal callback (see the hub contract), so reclaim the item here.
  auto &item = this->one_shot_command_items_.back();
  // We intentionally do not pass read_options_ here, because one-shot commands are usually writes, and are non-polling.
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

#pragma GCC diagnostic pop

void ModbusController::update() {
  this->sweep_completed_one_shots_();  // reclaim one-shots deferred out of their own callbacks
  if (this->module_offline_) {
    // Offline probing follows the offline cadence alone; regular every-update polling resumes once
    // the device is back online.
    if (offline_retry_due(this->update_counter_, this->module_offline_at_, this->offline_skip_updates_)) {
      ESP_LOGV(TAG, "Module offline - retrying");
      this->cmd_non_responses_ = 0;  // allow the probe through can_send()
      for (auto &poll : this->polling_devices_) {
        // Probes carry the read-side options too, so a recovering device resumes streaming on the
        // probe itself rather than waiting for the next update_interval.
        if (!poll.queue(this->read_options_)) {
          ESP_LOGD(TAG, "Probe refused by hub for range 0x%X", poll.register_address());
        }
      }
    } else {
      ESP_LOGV(TAG, "Module offline - skipping update");
    }
    this->update_counter_++;
    return;
  }

  if (this->can_send()) {
    for (auto &poll : this->polling_devices_) {
      ESP_LOGVV(TAG, "Updating range 0x%X", poll.register_address());
      // read_options_ carries the controller's continuous flag (the offline probe above sends it too).
      // A refusal is already logged by the hub; note the affected range for controller-level diagnostics.
      if (!poll.queue(this->read_options_)) {
        ESP_LOGD(TAG, "Poll refused by hub for range 0x%X", poll.register_address());
      }
    }
  }
  this->update_counter_++;
}

// walk through the sensors and determine the register ranges to read
namespace {

class RangeBuilder {
 public:
  explicit RangeBuilder(FixedVector<RegisterRange> &ranges) : ranges_(ranges) {}

  bool can_join(const SensorItem *curr) const {
    return this->have_range_ && curr->reuse_previous_range != RangeReuse::NEVER &&
           this->r_.register_type == curr->register_type && curr->register_type != modbus::EntityType::CUSTOM;
  }

  // A sensor that joined mid-range must never anchor this - hence both address tests.
  bool try_reuse_register(SensorItem *curr) {
    const uint32_t range_end = this->range_end_();
    if (curr->start_address != range_end - this->prev_->entity_count() ||
        this->prev_->start_address + this->prev_->entity_count() != range_end ||
        curr->entity_count() != this->prev_->entity_count() ||
        curr->get_register_size() != this->prev_->get_register_size()) {
      return false;
    }
    if (!place_offset(curr, static_cast<uint32_t>(this->prev_->offset) + curr->offset_from_start_address))
      return false;
    ESP_LOGV(TAG, "Re-use previous register 0x%X", curr->start_address);
    return true;
  }

  bool try_extend(SensorItem *curr) {
    const uint32_t range_end = this->range_end_();
    const bool reachable =
        curr->reuse_previous_range == RangeReuse::ALWAYS
            ? curr->start_address >= range_end
            : curr->start_address == range_end && (curr->addresses_bits() || !this->range_custom_size_);
    if (!reachable)
      return false;
    const uint16_t gap = static_cast<uint16_t>(curr->start_address - range_end);
    const uint32_t new_count = this->r_.register_count + gap + curr->entity_count();
    const uint16_t max_quantity =
        curr->addresses_bits() ? modbus::MAX_NUM_OF_COILS_TO_READ : modbus::MAX_NUM_OF_REGISTERS_TO_READ;
    const uint32_t prospective_offset =
        (curr->addresses_bits() ? static_cast<uint32_t>(curr->start_address - this->r_.start_address)
                                : static_cast<uint32_t>(this->range_bytes_) + gap * 2) +
        curr->offset_from_start_address;
    if (new_count > max_quantity || !place_offset(curr, prospective_offset)) {
      return false;
    }
    if (!curr->addresses_bits())
      this->range_bytes_ += static_cast<size_t>(gap) * 2;
    this->range_bytes_ += curr->get_register_size();
    this->range_custom_size_ = this->range_custom_size_ || has_custom_size(curr);
    this->r_.register_count = static_cast<uint16_t>(new_count);
    ESP_LOGV(TAG, "Extend range to include 0x%X", curr->start_address);
    return true;
  }

  bool try_cover(SensorItem *curr) {
    if (!this->range_shared_ || this->range_forced_ || curr->start_address < this->r_.start_address ||
        curr->start_address + curr->entity_count() > this->range_end_() || this->range_custom_size_ ||
        has_custom_size(curr)) {
      return false;
    }
    const uint32_t addr_delta = curr->start_address - this->r_.start_address;
    if (!place_offset(curr, (curr->addresses_bits() ? addr_delta : addr_delta * 2) + curr->offset_from_start_address))
      return false;
    ESP_LOGV(TAG, "Register 0x%X already covered by range 0x%X", curr->start_address, this->r_.start_address);
    return true;
  }

  // A response dispatches to a single range per (start address, register type), so same-address items
  // must share - even reuse_previous_range: false and custom entities.
  bool try_share(SensorItem *curr) {
    if (!this->have_range_ || this->r_.register_type != curr->register_type ||
        this->r_.start_address != curr->start_address) {
      return false;
    }
    curr->offset = curr->offset_from_start_address;
    this->r_.register_count = std::max(this->r_.register_count, curr->entity_count());
    this->range_bytes_ = std::max(this->range_bytes_, curr->get_register_size());
    this->range_custom_size_ = this->range_custom_size_ || has_custom_size(curr);
    this->range_shared_ = true;
    this->range_forced_ = this->range_forced_ || curr->reuse_previous_range == RangeReuse::NEVER;
    ESP_LOGV(TAG, "Share range start 0x%X", curr->start_address);
    return true;
  }

  bool always_declined(const SensorItem *curr) const {
    return this->have_range_ && curr->reuse_previous_range == RangeReuse::ALWAYS &&
           this->r_.register_type == curr->register_type && curr->start_address != this->r_.start_address;
  }

  void open(SensorItem *curr) {
    this->close();
    this->r_ = {};
    this->range_bytes_ = curr->get_register_size();
    this->range_custom_size_ = has_custom_size(curr);
    this->range_forced_ = curr->reuse_previous_range == RangeReuse::NEVER;
    this->range_shared_ = false;
    curr->offset = curr->offset_from_start_address;
    this->r_.start_address = curr->start_address;
    this->r_.register_count = curr->entity_count();
    this->r_.register_type = curr->register_type;
    if (curr->register_type == modbus::EntityType::CUSTOM)
      this->r_.custom_pdu = &curr->custom_pdu;
    this->have_range_ = true;
  }

  void record(SensorItem *curr) {
    curr->range_start_address = this->r_.start_address;
    this->r_.sensors.insert(curr);
    this->prev_ = curr;
  }

  void close() {
    if (!this->have_range_)
      return;
    ESP_LOGV(TAG, "Add range 0x%X %d", this->r_.start_address, this->r_.register_count);
    this->ranges_.push_back(std::move(this->r_));
    this->have_range_ = false;
  }

 private:
  uint32_t range_end_() const { return this->r_.start_address + this->r_.register_count; }
  // The resolved offset must fit its uint8_t field or the sensor would parse the wrong slice.
  static bool place_offset(SensorItem *curr, uint32_t offset) {
    if (offset > std::numeric_limits<uint8_t>::max())
      return false;
    curr->offset = static_cast<uint8_t>(offset);
    return true;
  }
  static bool has_custom_size(const SensorItem *item) {
    return item->get_register_size() != static_cast<size_t>(item->entity_count()) * 2;
  }
  FixedVector<RegisterRange> &ranges_;
  RegisterRange r_ = {};
  bool have_range_ = false;
  bool range_forced_ = false;  // a reuse: false member blocks the coverage join
  bool range_shared_ = false;  // only a share-widened range absorbs by coverage
  size_t range_bytes_ = 0;
  bool range_custom_size_ = false;
  SensorItem *prev_ = nullptr;
};

}  // namespace

void ModbusController::create_polling_commands_() {
  if (this->sensorset_.empty()) {
    ESP_LOGW(TAG, "No sensors registered");
    return;
  }

  // At most one range closes per sensor plus one final close, so sensorset_.size() bounds the pushes
  // (FixedVector silently drops past capacity).
  FixedVector<RegisterRange> ranges;
  ranges.init(this->sensorset_.size());
  RangeBuilder builder(ranges);
  for (SensorItem *curr : this->sensorset_) {
    ESP_LOGV(TAG, "Register: 0x%X width=%u size=%zu offset=%u addr=%p", curr->start_address, curr->entity_count(),
             curr->get_register_size(), curr->offset, curr);
    bool join = builder.can_join(curr) &&
                (builder.try_reuse_register(curr) || builder.try_extend(curr) || builder.try_cover(curr));
    if (!join && builder.always_declined(curr)) {
      ESP_LOGW(TAG, "reuse_previous_range on 0x%X cannot join the previous range; starting a new range",
               curr->start_address);
    }
    join = join || builder.try_share(curr);
    if (!join)
      builder.open(curr);
    builder.record(curr);
  }
  builder.close();

  this->polling_devices_.init(ranges.size());
  for (auto &range : ranges) {
    this->polling_devices_.emplace_back(*this, std::move(range));
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
    ESP_LOGCONFIG(TAG, " Sensor type=%u start=0x%X offset=0x%X width=%u size=%zu",
                  static_cast<uint8_t>(it->register_type), it->start_address, it->offset, it->entity_count(),
                  it->get_register_size());
  }
  ESP_LOGCONFIG(TAG, "ranges");
  for (auto &it : this->polling_devices_) {
    ESP_LOGCONFIG(TAG, "  Range type=%u start=0x%X count=%d", static_cast<uint8_t>(it.register_type()),
                  it.register_address(), it.register_count());
  }
#endif
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
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

bool ModbusCommandItem::send(modbus::CommandOptions options) {
  // Options pass straight through to the hub
  bool accepted;
  if (this->custom_pdu_ != nullptr) {
    // Custom polling command: send the sensor's ready-made PDU (function code + data, no address byte)
    // to this controller's own device address; the hub prepends the address and appends the CRC.
    accepted = modbus::ModbusClientDevice::queue_pdu(std::span<const uint8_t>(*this->custom_pdu_), options);
  } else if (this->function_code_ != FunctionCode::CUSTOM) {
    accepted = this->queue_pdu(modbus::helpers::create_client_pdu(
                                   this->function_code_, this->start_address_, this->register_count_,
                                   this->payload.empty() ? nullptr : this->payload.data(), this->payload.size()),
                               options);
  } else {
    // Factory custom command: payload holds a complete raw frame (address + PDU). Send the PDU to the
    // frame's own address (which may differ from this controller's); the hub appends the CRC and routes
    // the response back to this item by pointer.
    std::span<const uint8_t> frame = this->payload;
    if (frame.empty()) {
      ESP_LOGW(TAG, "Empty custom command frame, not sent");
      accepted = false;
    } else {
      accepted = this->parent_->queue_pdu(frame[0], frame.subspan(1), this, options);
    }
  }
  // The on_command_sent trigger fires from on_sent() when the frame actually reaches the wire.
  if (accepted) {
    ESP_LOGV(TAG, "Command queued %d 0x%X %d", uint8_t(this->function_code_), this->start_address_,
             this->register_count_);
  }
  return accepted;
}
#pragma GCC diagnostic pop

}  // namespace esphome::modbus_controller
