#pragma once

// Pull in ESPHome's USE_* macro table before the ifdef guards — none of the
// other includes below reach defines.h transitively, so without this the
// #ifdef closes and the compiled .o has zero symbols.
#include "esphome/core/defines.h"

// esp-matter 1.6.0 only supports these ESP32 variants. Strip the whole
// TU on any other target (P4, S2, C2, C5, C61, H4, H21, S31) so clang-tidy
// jobs for those variants — which grep this file in via USE_WIFI /
// USE_ETHERNET — don't try to compile against an esp_matter.h that upstream
// never ships for those chips. Runtime builds are already rejected by the
// only_on_variant config validator in matter/__init__.py; this guard is the
// static-analysis mirror of the same restriction.
#ifdef USE_ESP_IDF
#if defined(USE_ESP32_VARIANT_ESP32) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32C3) || \
    defined(USE_ESP32_VARIANT_ESP32C6) || defined(USE_ESP32_VARIANT_ESP32H2)
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
#endif  // matter supported variant
#endif  // USE_ESP_IDF
