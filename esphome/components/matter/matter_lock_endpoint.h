#pragma once

// Pull in ESPHome's USE_* macro table before the ifdef guards — none of the
// other includes below reach defines.h transitively, so without this the
// #ifdef closes and the compiled .o has zero symbols.
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

#include <cstdint>
#include <atomic>

namespace esphome::lock {
class Lock;
enum LockState : uint8_t;
}  // namespace esphome::lock

namespace esphome::matter {

// Wraps one ESPHome lock as a Matter door_lock endpoint.
//
// LockState (0x0000, nullable enum8) — device→fabric only, via attribute::update.
// esp-matter creates it as a normal (not MANAGED_INTERNALLY) nullable attribute
// so update() works directly. Mapping ESPHome → Matter DlLockState:
//   LOCK_STATE_NONE → null
//   LOCK_STATE_LOCKED → 1 (Locked)
//   LOCK_STATE_UNLOCKED → 2 (Unlocked)
//   LOCK_STATE_JAMMED → 0 (NotFullyLocked)
//   LOCK_STATE_LOCKING / UNLOCKING / OPENING → 0 (NotFullyLocked, transient)
//   LOCK_STATE_OPEN → 2 (Unlocked)   // Unlatched (3) requires the Unbolt feature
//
// Fabric → device (commands, NOT attribute writes):
//   Matter LockDoor (cmd 0x00) and UnlockDoor (cmd 0x01) come in via CHIP's
//   DoorLockServer::HandleRemoteLockOperation, which invokes the weak hooks
//   emberAfPluginDoorLockOnDoorLockCommand / OnDoorUnlockCommand. The .cpp
//   provides strong overrides that route to MatterComponent::instance() which
//   dispatches to the wrapper by endpoint_id. Returning true lets CHIP set
//   LockState itself; we only drive ESPHome's lock()/unlock() from the hook.
class MatterLockEndpoint {
 public:
  explicit MatterLockEndpoint(lock::Lock *lock);

  bool setup();

  // Called by the weak-override hooks (via the MatterComponent dispatcher).
  // is_lock=true → LockDoor; false → UnlockDoor.
  // Returns true if the ESPHome call was issued (used by the hook to return
  // success to the fabric).
  bool on_matter_command(bool is_lock);

  void push_initial_state();

  uint16_t endpoint_id() const { return endpoint_id_; }
  lock::Lock *esphome_lock() const { return lock_; }
  bool applying_report() const { return this->applying_report_.load(std::memory_order_acquire); }

 protected:
  void report_state_to_fabric_(lock::LockState state);

  lock::Lock *lock_;
  uint16_t endpoint_id_{0};
  // Set while lock_->lock()/unlock() is executing from the command hook — the
  // ESPHome state callback fires synchronously via publish_state and would
  // otherwise re-push LockState right before CHIP's own SetLockState.
  bool applying_matter_write_{false};
  // Set while attribute::update() runs — PRE_UPDATE fires synchronously and
  // lands in the global dispatcher. LockState has no dispatcher branch today,
  // but keeping the flag consistent with the other bidirectional wrappers.
  std::atomic<bool> applying_report_{false};
};

}  // namespace esphome::matter

#endif  // USE_LOCK
#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED
