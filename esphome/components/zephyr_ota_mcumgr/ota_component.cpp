#include "ota_component.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#include <zephyr/sys/math_extras.h>
#include <zephyr/dfu/mcuboot.h>

struct img_mgmt_upload_action {
  /** The total size of the image. */
  unsigned long long size;
};

struct img_mgmt_upload_req {
  uint32_t image; /* 0 by default */
  size_t off;     /* SIZE_MAX if unspecified */
};

namespace esphome {
namespace zephyr_ota_mcumgr {

static const char *const TAG = "zephyr_ota_mcumgr";

#define IMAGE_HASH_LEN 32 /* Size of SHA256 TLV hash */

static enum mgmt_cb_return mcumgr_img_mgmt_cb(uint32_t event, enum mgmt_cb_return prev_status, int32_t *rc,
                                              uint16_t *group, bool *abort_more, void *data, size_t data_size) {
  if (MGMT_EVT_OP_IMG_MGMT_DFU_CHUNK == event) {
    const img_mgmt_upload_check &upload = *static_cast<img_mgmt_upload_check *>(data);
    static_cast<OTAComponent *>(ota::global_ota_component)->update_chunk(upload);
  } else if (MGMT_EVT_OP_IMG_MGMT_DFU_STARTED == event) {
    static_cast<OTAComponent *>(ota::global_ota_component)->update_started();
  } else if (MGMT_EVT_OP_IMG_MGMT_DFU_CHUNK_WRITE_COMPLETE == event) {
    static_cast<OTAComponent *>(ota::global_ota_component)->update_chunk_wrote();
  } else if (MGMT_EVT_OP_IMG_MGMT_DFU_PENDING == event) {
    static_cast<OTAComponent *>(ota::global_ota_component)->update_pending();
  } else {
    ESP_LOGD(TAG, "MCUmgr Image Management Event with the %d ID", u32_count_trailing_zeros(MGMT_EVT_GET_ID(event)));
  }
  return MGMT_CB_OK;
}

static struct mgmt_callback IMG_MGMT_CALLBACK = {
    .callback = mcumgr_img_mgmt_cb,
    .event_id = MGMT_EVT_OP_IMG_MGMT_ALL,
};

OTAComponent::OTAComponent() { ota::global_ota_component = this; }

void OTAComponent::setup() { mgmt_callback_register(&IMG_MGMT_CALLBACK); }

void OTAComponent::loop() {
  if (!is_confirmed_) {
    is_confirmed_ = boot_is_img_confirmed();
    if (!is_confirmed_) {
      if (boot_write_img_confirmed()) {
        ESP_LOGD(TAG, "Unable to confirm image");
        // TODO reboot
      }
    }
  }
}

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

void OTAComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Over-The-Air Updates:");
  ESP_LOGCONFIG(TAG, "  swap type after reboot: %s", swap_type_str(mcuboot_swap_type()));
  ESP_LOGCONFIG(TAG, "  image confirmed: %s", YESNO(boot_is_img_confirmed()));
}

void OTAComponent::update_chunk(const img_mgmt_upload_check &upload) {
  percentage_ = (upload.req->off * 100.0f) / upload.action->size;
}

void OTAComponent::update_started() {
  ESP_LOGD(TAG, "Starting OTA Update from %s...", "ble");
#ifdef USE_OTA_STATE_CALLBACK
  this->state_callback_.call(ota::OTA_STARTED, 0.0f, 0);
#endif
}

void OTAComponent::update_chunk_wrote() {
  uint32_t now = millis();
  if (now - last_progress_ > 1000) {
    last_progress_ = now;
    ESP_LOGD(TAG, "OTA in progress: %0.1f%%", percentage_);
#ifdef USE_OTA_STATE_CALLBACK
    this->state_callback_.call(ota::OTA_IN_PROGRESS, percentage_, 0);
#endif
  }
}

void OTAComponent::update_pending() {
  ESP_LOGD(TAG, "OTA pending");
#ifdef USE_OTA_STATE_CALLBACK
  this->state_callback_.call(ota::OTA_COMPLETED, 100.0f, 0);
#endif
}

}  // namespace zephyr_ota_mcumgr
}  // namespace esphome
