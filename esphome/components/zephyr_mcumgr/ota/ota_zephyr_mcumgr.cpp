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

static const char *const TAG = "zephyr_mcumgr";
static OTAComponent *global_ota_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#define IMAGE_HASH_LEN 32 /* Size of SHA256 TLV hash */

static enum mgmt_cb_return mcumgr_img_mgmt_cb(uint32_t event, enum mgmt_cb_return prev_status, int32_t *rc,
                                              uint16_t *group, bool *abort_more, void *data, size_t data_size) {
  if (MGMT_EVT_OP_IMG_MGMT_DFU_CHUNK == event) {
    const img_mgmt_upload_check &upload = *static_cast<img_mgmt_upload_check *>(data);
    static_cast<OTAComponent *>(global_ota_component)->update_chunk(upload);
  } else if (MGMT_EVT_OP_IMG_MGMT_DFU_STARTED == event) {
    static_cast<OTAComponent *>(global_ota_component)->update_started();
  } else if (MGMT_EVT_OP_IMG_MGMT_DFU_CHUNK_WRITE_COMPLETE == event) {
    static_cast<OTAComponent *>(global_ota_component)->update_chunk_wrote();
  } else if (MGMT_EVT_OP_IMG_MGMT_DFU_PENDING == event) {
    static_cast<OTAComponent *>(global_ota_component)->update_pending();
  } else {
    ESP_LOGD(TAG, "MCUmgr Image Management Event with the %d ID", u32_count_trailing_zeros(MGMT_EVT_GET_ID(event)));
  }
  return MGMT_CB_OK;
}

OTAComponent::OTAComponent() { global_ota_component = this; }

void OTAComponent::setup() {
  img_mgmt_callback_.callback = mcumgr_img_mgmt_cb;
  img_mgmt_callback_.event_id = MGMT_EVT_OP_IMG_MGMT_ALL;
  mgmt_callback_register(&img_mgmt_callback_);
#ifdef CONFIG_USB_DEVICE_STACK
  if (cdc_uart_) {
    usb_enable(NULL);
  }
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
  ESP_LOGCONFIG(TAG, "Over-The-Air Updates:");
  ESP_LOGCONFIG(TAG, "  swap type after reboot: %s", swap_type_str(mcuboot_swap_type()));
  ESP_LOGCONFIG(TAG, "  image confirmed: %s", YESNO(boot_is_img_confirmed()));
}

void OTAComponent::update_chunk(const img_mgmt_upload_check &upload) {
  this->defer([this, upload]() { this->percentage_ = (upload.req->off * 100.0f) / upload.action->size; });
}

void OTAComponent::update_started() {
  this->defer([this]() {
    ESP_LOGD(TAG, "Starting OTA Update from %s...", "ble");
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

}  // namespace esphome::zephyr_mcumgr
#endif
