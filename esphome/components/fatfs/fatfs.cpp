#include "fatfs.h"
#include "esphome/core/log.h"

extern "C" {
#include "ff.h"
#include "diskio.h"
#if ESP_IDF_VERSION_MAJOR > 3
#include "diskio_impl.h"
#endif
#include "esp_vfs_fat.h"
}

namespace esphome {
namespace fatfs {

static const char *TAG = "fatfs";

void MediaDetectInterrupt::inserted(MediaDetectInterrupt *data) { data->present = true; }
void MediaDetectInterrupt::ejected(MediaDetectInterrupt *data) { data->present = false; }

const char *fs_errstr(uint8_t errnum) {
  const char *fs_err2str[] = {"(0) Succeeded",
                              "(1) A hard error occurred in the low level disk I/O layer",
                              "(2) Assertion failed",
                              "(3) The physical drive cannot work",
                              "(4) Could not find the file",
                              "(5) Could not find the path",
                              "(6) The path name format is invalid",
                              "(7) Access denied due to prohibited access or directory full",
                              "(8) Access denied due to prohibited access",
                              "(9) The file/directory object is invalid",
                              "(10) The physical drive is write protected",
                              "(11) The logical drive number is invalid",
                              "(12) The volume has no work area",
                              "(13) There is no valid FAT volume",
                              "(14) The f_mkfs() aborted due to any problem",
                              "(15) Could not get a grant to access the volume within defined period",
                              "(16) The operation is rejected according to the file sharing policy",
                              "(17) LFN working buffer could not be allocated",
                              "(18) Number of open files > FF_FS_LOCK",
                              "(19) Given parameter is invalid"};
  return fs_err2str[errnum];
}

/** --------------------------------------------------------------------------------
 *
 * @brief
 *
 */
void FatFs::set_drive_id(uint8_t drive_id) {
  drive_id_ = drive_id;
  logical_drv_ += std::to_string(drive_id_) + std::string(":");
}

/** --------------------------------------------------------------------------------
 *
 * @brief
 *
 */
bool FatFs::init_storage_state_interrupt() {
  if (this->cd_pin_ != NULL) {
    this->cd_pin_->setup();
    this->cd_pin_->pin_mode(gpio::FLAG_PULLUP);
    this->cd_pin_->attach_interrupt(MediaDetectInterrupt::inserted, &this->media_present_st_,
                                    gpio::INTERRUPT_LOW_LEVEL);
    this->cd_pin_->attach_interrupt(MediaDetectInterrupt::ejected, &this->media_present_st_,
                                    gpio::INTERRUPT_HIGH_LEVEL);

    // Set current pin state
    this->media_present_st_.present = this->cd_pin_->digital_read();
    this->media_present_st_.init = true;
    ESP_LOGD(TAG, "Arm CardDetect interrupt");
    return true;
  }
  return false;
}

/** --------------------------------------------------------------------------------
 *
 * @brief
 *
 */
StorageState FatFs::is_storage_state_interrupt() {
  if (this->media_present_st_.init) {
    if (this->media_present_st_.present != media_present_) {
      media_present_ = this->media_present_st_.present;
    }
    if (media_present_)
      return StorageState::MEDIA_PRESENT;
    else
      return StorageState::MEDIA_ABSENT;
  }
  return StorageState::MEDIA_UNUSED;
}

}  // namespace fatfs
}  // namespace esphome
