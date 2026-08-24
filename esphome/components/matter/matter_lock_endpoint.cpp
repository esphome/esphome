// Same rationale as the other wrappers — pull ESPHome's USE_* macros in
// before the ifdef guards. See matter_switch_endpoint.cpp comment.
#include "esphome/core/defines.h"

// USE_MATTER_VARIANT_SUPPORTED is set by matter's Python to_code() via
// cg.add_define() on the 5 esp-matter-supported ESP32 variants (ESP32,
// S3, C3, C6, H2). It is deliberately NOT declared in
// esphome/core/defines.h — that path is only exercised by clang-tidy and
// static-analysis tools, which do not have esp_matter.h available (the
// SDK is a third-party managed component fetched at build time). Keeping
// the symbol out of defines.h means matter files strip on lint (no
// missing-header errors) but compile normally on real builds where
// Python-side codegen has run. Runtime variant enforcement lives in the
// only_on_variant validator in matter/__init__.py.
#if defined(USE_ESP_IDF) && defined(USE_MATTER_VARIANT_SUPPORTED)
#ifdef USE_LOCK

#include "matter_lock_endpoint.h"
#include "matter_component.h"

#include "esphome/core/log.h"
#include "esphome/components/lock/lock.h"

#include <esp_matter.h>

#include <app-common/zap-generated/cluster-objects.h>
#include <app/clusters/door-lock-server/door-lock-server.h>

namespace esphome::matter {

static const char *const TAG = "matter.lock";

namespace {

// ESPHome LockState → Matter DlLockState raw uint8. is_null is only set true
// for LOCK_STATE_JAMMED, where we deliberately want the fabric to see the
// "unknown / error" state. Everything else — including LOCK_STATE_NONE — maps
// to a real value, because HA (and other controllers) treat null LockState as
// "device not ready" and refuse to render lock/unlock controls. Defaulting
// NONE to Unlocked (2) is a safe fallback: it's the more permissive state and
// the user's next LockDoor command will drive it to Locked.
uint8_t map_state_to_matter(::esphome::lock::LockState state, bool *is_null) {
  *is_null = false;
  switch (state) {
    case ::esphome::lock::LOCK_STATE_LOCKED:
      return 1;  // Locked
    case ::esphome::lock::LOCK_STATE_UNLOCKED:
    case ::esphome::lock::LOCK_STATE_OPEN:
    case ::esphome::lock::LOCK_STATE_NONE:  // fallback: unknown → Unlocked so HA renders controls
      return 2;                             // Unlocked (Unlatched=3 requires Unbolt feature, which we don't advertise)
    case ::esphome::lock::LOCK_STATE_LOCKING:
    case ::esphome::lock::LOCK_STATE_UNLOCKING:
    case ::esphome::lock::LOCK_STATE_OPENING:
      return 0;  // NotFullyLocked (transient)
    case ::esphome::lock::LOCK_STATE_JAMMED:
      *is_null = true;  // Genuinely broken state — controller should surface as unknown/error
      return 0;
    default:
      return 2;  // Unlocked fallback for any future LockState enum additions
  }
}

}  // namespace

MatterLockEndpoint::MatterLockEndpoint(::esphome::lock::Lock *lock) : lock_(lock) {}

bool MatterLockEndpoint::setup() {
  ::esp_matter::node_t *node = ::esp_matter::node::get();
  if (node == nullptr) {
    ESP_LOGE(TAG, "no Matter node available for lock '%s'", this->lock_->get_name().c_str());
    return false;
  }

  ::esp_matter::endpoint::door_lock::config_t config;
  // lock_type = 0 → DeadBolt (spec default; controllers render as a generic
  // deadbolt). Not exposing lock_type as a per-entity option — ESPHome lock
  // has no equivalent concept and every controller in scope treats DeadBolt
  // like any other lock. operating_mode = 0 → Normal. supported_operating_modes
  // stays at the config default 0xFFF6 (Normal only, other bits set to "not
  // supported" per spec Table 8.13.4).
  config.door_lock.lock_type = 0;
  config.door_lock.actuator_enabled = true;
  config.door_lock.operating_mode = 0;
  bool null_state = false;
  uint8_t initial = map_state_to_matter(this->lock_->state, &null_state);
  if (null_state) {
    config.door_lock.lock_state = nullptr;
  } else {
    config.door_lock.lock_state = initial;
  }

  ::esp_matter::endpoint_t *endpoint =
      ::esp_matter::endpoint::door_lock::create(node, &config, ::esp_matter::ENDPOINT_FLAG_NONE, this);
  if (endpoint == nullptr) {
    ESP_LOGE(TAG, "failed to create door_lock endpoint for '%s'", this->lock_->get_name().c_str());
    return false;
  }
  this->endpoint_id_ = ::esp_matter::endpoint::get_id(endpoint);

  MatterComponent::instance()->register_endpoint_label(endpoint, this->endpoint_id_, this->lock_->get_name());

  // Device → fabric: ESPHome lock state callback carries the new LockState.
  this->lock_->add_on_state_callback([this](::esphome::lock::LockState state) {
    if (this->applying_matter_write_) {
      ESP_LOGV(TAG, "device state callback suppressed (matter-driven change) endpoint=%u", this->endpoint_id_);
      return;
    }
    ESP_LOGD(TAG, "device state change → fabric: endpoint=%u state=%u lock='%s'", this->endpoint_id_,
             static_cast<unsigned>(state), this->lock_->get_name().c_str());
    this->report_state_to_fabric_(state);
  });

  ESP_LOGI(TAG, "registered lock '%s' as Matter door_lock endpoint %u", this->lock_->get_name().c_str(),
           this->endpoint_id_);
  return true;
}

bool MatterLockEndpoint::on_matter_command(bool is_lock) {
  ESP_LOGD(TAG, "matter %s command endpoint=%u lock='%s'", is_lock ? "LockDoor" : "UnlockDoor", this->endpoint_id_,
           this->lock_->get_name().c_str());
  // Called straight from CHIP's DoorLockServer callback on the PlatformManager
  // task — Lock::lock/unlock must not run there. Defer the actual command onto
  // the main loop; the return value here is the "did we accept the command"
  // ack, which is optimistic anyway (see comment below). applying_matter_write_
  // is scoped inside the lambda to keep guard access main-loop-only.
  MatterComponent::instance()->defer_on_main_loop([this, is_lock]() {
    this->applying_matter_write_ = true;
    if (is_lock) {
      this->lock_->lock();
    } else {
      this->lock_->unlock();
    }
    this->applying_matter_write_ = false;
  });
  // Optimistic ack — the ESPHome lock() call itself doesn't return status.
  // CHIP's DoorLockServer will call SetLockState(Locked/Unlocked) internally
  // when we return true. If the physical lock jams, the platform will publish
  // LOCK_STATE_JAMMED later and the state callback path corrects the fabric
  // view via report_state_to_fabric_().
  return true;
}

void MatterLockEndpoint::push_initial_state() { this->report_state_to_fabric_(this->lock_->state); }

void MatterLockEndpoint::report_state_to_fabric_(::esphome::lock::LockState state) {
  bool is_null = false;
  uint8_t matter_state = map_state_to_matter(state, &is_null);
  // LockState is created as nullable_ENUM8 (see esp_matter_attribute.cpp:2585
  // create_lock_state → esp_matter_nullable_enum8). Pushing nullable_uint8
  // triggers ESP_ERR_INVALID_ARG in esp_matter's type validator even though
  // the underlying storage is identical. nullable<T> is defined at global
  // scope; default-construct is null.
  ::nullable<uint8_t> nv = is_null ? ::nullable<uint8_t>() : ::nullable<uint8_t>(matter_state);
  ::esp_matter_attr_val_t val = ::esp_matter_nullable_enum8(nv);

  ApplyingReportGuard applying_report_guard(this->applying_report_);
  MatterComponent::instance()->defer_attribute_update(this->endpoint_id_, chip::app::Clusters::DoorLock::Id,
                                                      chip::app::Clusters::DoorLock::Attributes::LockState::Id, val);
}

}  // namespace esphome::matter

// =============================================================================
// Weak-symbol overrides for CHIP's DoorLock command hooks.
//
// CHIP's HandleRemoteLockOperation calls these to let application code drive
// the physical lock. Default (weak) impls in
// connectedhomeip/.../door-lock-server-callback.cpp return false. We override
// with strong symbols in the same signature so our matter component wins at
// link time when both objects go into the final ELF.
//
// Both hooks look up the wrapper by endpoint id via MatterComponent::instance()
// and delegate to MatterLockEndpoint::on_matter_command(). Returning true lets
// CHIP update LockState + emit the LockOperation event itself.
// =============================================================================

// esp-matter 1.5.1 references `emberAfDoorLockClusterInitCallback` in the
// door_lock cluster's function_list (esp_matter_cluster.cpp:2046) but
// neither esp-matter nor CHIP provide a definition — the ZAP output for
// DoorLock is missing the usual empty stub that other cluster init callbacks
// get (e.g. emberAfOnOffClusterServerInitCallback is defined in
// on-off-server.cpp:1028). Provide our own empty stub so the linker resolves
// the reference. CHIP's DoorLockServer::Instance() initializes itself via
// MatterDoorLockPluginServerInitCallback (properly defined in
// door-lock-server.cpp:4312), so an empty per-cluster init is functionally
// correct.
//
// Function names + camelCase parameter names below are dictated by CHIP's
// weak-symbol contract; suppressing clang-tidy's naming check across the
// block since renaming would break the override.
// NOLINTBEGIN(readability-identifier-naming)
void emberAfDoorLockClusterInitCallback(chip::EndpointId /*endpoint*/) {}

bool emberAfPluginDoorLockOnDoorLockCommand(chip::EndpointId endpointId,
                                            const chip::app::DataModel::Nullable<chip::FabricIndex> & /*fabricIdx*/,
                                            const chip::app::DataModel::Nullable<chip::NodeId> & /*nodeId*/,
                                            const chip::Optional<chip::ByteSpan> & /*pinCode*/,
                                            chip::app::Clusters::DoorLock::OperationErrorEnum &err) {
  auto *comp = ::esphome::matter::MatterComponent::instance();
  if (comp == nullptr) {
    err = chip::app::Clusters::DoorLock::OperationErrorEnum::kUnspecified;
    return false;
  }
  if (comp->handle_lock_command(static_cast<uint16_t>(endpointId), true)) {
    return true;
  }
  err = chip::app::Clusters::DoorLock::OperationErrorEnum::kUnspecified;
  return false;
}

bool emberAfPluginDoorLockOnDoorUnlockCommand(chip::EndpointId endpointId,
                                              const chip::app::DataModel::Nullable<chip::FabricIndex> & /*fabricIdx*/,
                                              const chip::app::DataModel::Nullable<chip::NodeId> & /*nodeId*/,
                                              const chip::Optional<chip::ByteSpan> & /*pinCode*/,
                                              chip::app::Clusters::DoorLock::OperationErrorEnum &err) {
  auto *comp = ::esphome::matter::MatterComponent::instance();
  if (comp == nullptr) {
    err = chip::app::Clusters::DoorLock::OperationErrorEnum::kUnspecified;
    return false;
  }
  if (comp->handle_lock_command(static_cast<uint16_t>(endpointId), false)) {
    return true;
  }
  err = chip::app::Clusters::DoorLock::OperationErrorEnum::kUnspecified;
  return false;
}
// NOLINTEND(readability-identifier-naming)

#endif  // USE_LOCK
#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED
