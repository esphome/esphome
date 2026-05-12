#ifdef USE_ZEPHYR
#include "ota_zephyr_mcumgr.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <zephyr/sys/math_extras.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/dfu/mcuboot.h>

// It should be from below header but there is problem with internal includes.
// #include <zephyr/mgmt/mcumgr/grp/img_mgmt/img_mgmt.h>
// NOLINTBEGIN(readability-identifier-naming,google-runtime-int)
struct img_mgmt_upload_action {
  /** The total size of the image. */
  unsigned long long size;
};

struct img_mgmt_upload_req {
  uint32_t image; /* 0 by default */
  size_t off;     /* SIZE_MAX if unspecified */
};
// NOLINTEND(readability-identifier-naming,google-runtime-int)

namespace esphome::zephyr_mcumgr {

static_assert(sizeof(struct img_mgmt_upload_action) == 8, "ABI mismatch");
static_assert(sizeof(struct img_mgmt_upload_req) == 8, "ABI mismatch");
static_assert(offsetof(struct img_mgmt_upload_req, image) == 0, "ABI mismatch");
static_assert(offsetof(struct img_mgmt_upload_req, off) == 4, "ABI mismatch");

static const char *const TAG = "zephyr_mcumgr";
static OTAComponent *global_ota_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static enum mgmt_cb_return mcumgr_img_mgmt_cb(uint32_t event, enum mgmt_cb_return prev_status, int32_t *rc,
                                              uint16_t *group, bool *abort_more, void *data, size_t data_size) {
  if (MGMT_EVT_OP_IMG_MGMT_DFU_CHUNK == event) {
    const img_mgmt_upload_check &upload = *static_cast<img_mgmt_upload_check *>(data);
    global_ota_component->update_chunk(upload);
  } else if (MGMT_EVT_OP_IMG_MGMT_DFU_STARTED == event) {
    global_ota_component->update_started();
  } else if (MGMT_EVT_OP_IMG_MGMT_DFU_CHUNK_WRITE_COMPLETE == event) {
    global_ota_component->update_chunk_wrote();
  } else if (MGMT_EVT_OP_IMG_MGMT_DFU_PENDING == event) {
    global_ota_component->update_pending();
  } else if (MGMT_EVT_OP_IMG_MGMT_DFU_STOPPED == event) {
    global_ota_component->update_stopped();
  } else {
    ESP_LOGD(TAG, "MCUmgr Image Management Event with the %d ID", u32_count_trailing_zeros(MGMT_EVT_GET_ID(event)));
  }
  return MGMT_CB_OK;
}

OTAComponent::OTAComponent() { global_ota_component = this; }

void OTAComponent::setup() {
  this->img_mgmt_callback_.callback = mcumgr_img_mgmt_cb;
  this->img_mgmt_callback_.event_id = MGMT_EVT_OP_IMG_MGMT_ALL;
  mgmt_callback_register(&this->img_mgmt_callback_);
#ifdef CONFIG_USB_DEVICE_STACK
  usb_enable(nullptr);
#endif
// Handle OTA rollback: mark partition valid immediately unless USE_OTA_ROLLBACK is enabled,
// in which case safe_mode will mark it valid after confirming successful boot.
#ifndef USE_OTA_ROLLBACK
  if (!boot_is_img_confirmed()) {
    boot_write_img_confirmed();
  }
#endif
}

#ifdef ESPHOME_LOG_HAS_CONFIG
static const char *swap_type_str(uint8_t type) {
  switch (type) {
    case BOOT_SWAP_TYPE_NONE:
      return "none";
    case BOOT_SWAP_TYPE_TEST:
      return "test";
    case BOOT_SWAP_TYPE_PERM:
      return "perm";
    case BOOT_SWAP_TYPE_REVERT:
      return "revert";
    case BOOT_SWAP_TYPE_FAIL:
      return "fail";
  }

  return "unknown";
}
#endif

void OTAComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Over-The-Air Updates:\n"
                "  swap type after reboot: %s\n"
                "  image confirmed: %s",
                swap_type_str(mcuboot_swap_type()), YESNO(boot_is_img_confirmed()));
}

void OTAComponent::update_chunk(const img_mgmt_upload_check &upload) {
  float percentage = (upload.req->off * 100.0f) / upload.action->size;
  this->defer([this, percentage]() { this->percentage_ = percentage; });
}

void OTAComponent::update_started() {
  this->defer([this]() {
    ESP_LOGD(TAG, "Starting update");
#ifdef USE_OTA_STATE_LISTENER
    this->notify_state_(ota::OTA_STARTED, 0.0f, 0);
#endif
  });
}

void OTAComponent::update_chunk_wrote() {
  uint32_t now = millis();
  if (now - this->last_progress_ > 1000) {
    this->last_progress_ = now;
    this->defer([this]() {
      ESP_LOGD(TAG, "OTA in progress: %0.1f%%", this->percentage_);
#ifdef USE_OTA_STATE_LISTENER
      this->notify_state_(ota::OTA_IN_PROGRESS, this->percentage_, 0);
#endif
    });
  }
}

void OTAComponent::update_pending() {
  this->defer([this]() {
    ESP_LOGD(TAG, "OTA pending");
#ifdef USE_OTA_STATE_LISTENER
    this->notify_state_(ota::OTA_COMPLETED, 100.0f, 0);
#endif
  });
}

void OTAComponent::update_stopped() {
  this->defer([this]() {
    ESP_LOGD(TAG, "OTA stopped");
#ifdef USE_OTA_STATE_LISTENER
    this->notify_state_(ota::OTA_ERROR, 0.0f, static_cast<uint8_t>(ota::OTA_RESPONSE_ERROR_UNKNOWN));
#endif
  });
}

}  // namespace esphome::zephyr_mcumgr
#endif
