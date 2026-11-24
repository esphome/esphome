#include "announcement_volume.h"

#include "esphome/core/log.h"

namespace esphome::speaker {

ESPHOME_LOG_TAG(TAG, "announcement_volume");

void AnnouncementVolume::log_begin_() const { ESP_LOGD(TAG, "Using announcement volume %.2f", this->volume_); }

void AnnouncementVolume::log_end_(float media_volume) const { ESP_LOGD(TAG, "Restoring volume %.2f", media_volume); }

}  // namespace esphome::speaker
