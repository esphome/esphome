#include "fatfs_esp32_sdspi.h"
#include "esphome/core/defines.h"
#ifdef USE_ESP32
#include "esphome/core/log.h"
#include "esphome/components/fatfs_esp32/fatfs_esp32.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace fatfs_esp32_sdspi {
// using namespace esphome::fatfs_esp32;
using namespace esphome::fatfs;

static const char *const TAG = "fatfs_esp32_sdspi";

bool SdspiIO::storage_init() {
  bool ret = this->init();  //  Call init from SDSPIDriver parent class
  ESP_LOGD(TAG, "sdspi init %s", TRUEFALSE(ret));

  if (!ret) {
    ESP_LOGE(TAG, "sdspi init error %d", this->error());
  }
  return ret;
}

//   bool storage_write_sectors(const uint8_t *buffer, DWORD sector, UINT count) override;
//   uint8_t storage_read_sectors(uint8_t *buffer, DWORD sector, UINT count) override;
//   uint8_t storage_ioctl(uint8_t cmd, void *buff) override;
//   uint8_t storage_status() override;

// FatESP32sdspi::FatESP32sdspi() { io_driver_ = new SdspiIO(); }

/** --------------------------------------------------------------------------------
 * @brief
 *
 * @return true
 * @return false
 */
bool FatESP32sdspi::init_io() {
  ESP_LOGV(TAG, "init_io...");
  bool ret = false;
  if (io_driver_ != NULL) {
    if (io_driver_->is_storage_status(STA_NOINIT)) {
      ret = io_driver_->storage_init();
      if (!ret) {
        ESP_LOGE(TAG, "init io. err");
      } else {
        ESP_LOGD(TAG, "init io.");
      }
    } else {
      ret = true;
      ESP_LOGV(TAG, "init io. already");
    }
  } else
    ESP_LOGE(TAG, "init io. storage not set");

  return ret;
}

void FatESP32sdspi::set_spi_parent(spi::SPIComponent *parent) { io_driver_->set_spi_parent(parent); }
void FatESP32sdspi::set_cs_pin(GPIOPin *cs) { io_driver_->set_cs_pin(cs); }
void FatESP32sdspi::set_data_rate(uint32_t data_rate) { io_driver_->set_data_rate(data_rate); }
void FatESP32sdspi::set_mode(spi::SPIMode mode) { io_driver_->set_mode(mode); }

//========================================================================================

void FatESP32sdspi::dump_config() {
  ESP_LOGCONFIG(TAG, "FATFS sdcard:");
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "   FS mountpoint:     %s", this->get_mount_point().c_str());
  ESP_LOGCONFIG(TAG, "   IO driver:         %s", TRUEFALSE(io_driver_ != NULL));
  ESP_LOGCONFIG(TAG, "   Drive name:        %s", this->get_drive_name().c_str());
  ESP_LOGCONFIG(TAG, "   Drive id:          %d", this->get_grive_id());
  LOG_PIN("   CD pin", this->get_cd_pin());
}

//========================================================================================

void FatESP32sdspi::setup() {
  ESP_LOGD(TAG, "Setup ...");

  this->set_io_driver(io_driver_);

  this->init_storage_state_interrupt();

  this->init_driver();  /// Release Driver
  this->init_io();
  if (io_driver_->is_storage_status(STA_NOINIT)) {
    this->relese_driver();
    this->init_driver();  /// Release Driver
    this->init_io();
  }

  //  Check for storage presented
  if (this->is_card()) {
    if (!this->mount()) {
      this->mark_failed();
      return;
    } else {
#ifdef FATFS_FS_TEST
      std::string test_path = "/";
      this->test_fs(test_path);
#endif
    }
  }
  { ESP_LOGD(TAG, "Storage media not present."); }
}

//========================================================================================

void FatESP32sdspi::update() {
  if (this->is_storage_state_interrupt() == StorageState::MEDIA_UNUSED) {
    // If media presend does not detected by cd_pin interrupt
    // Check it by hand
    if (this->is_card()) {
      this->reinit_driver(StorageState::MEDIA_PRESENT);
    } else
      this->reinit_driver(StorageState::MEDIA_ABSENT);
  }
}

//========================================================================================

void FatESP32sdspi::loop() { this->reinit_driver(this->is_storage_state_interrupt()); }

//--------------------------------------------------------------------------------

bool FatESP32sdspi::is_card() {
  ESP_LOGD(TAG, "is_card ...");
  StorageState storage_state_int = this->is_storage_state_interrupt();
  bool ret = false;

  // If Card detect pin activated, check pin.

  if (storage_state_int == StorageState::MEDIA_PRESENT) {
    ret = true;
  } else if (storage_state_int == StorageState::MEDIA_ABSENT) {
    ret = false;
  }

  //  Card detect pin/interrupt not used.  Use dummy reading from media
  else {
    if (this->init_io() && io_driver_->is_ready()) {
      ESP_LOGV(TAG, "start storage_ioctl ...");
      uint32_t ssize = 0;
      io_driver_->storage_ioctl(GET_SECTOR_COUNT, &ssize);
      if (ssize > 0) {
        ret = true;
      }
    } else {
      ret = false;  //  cannot init_io
    }
  }

  ESP_LOGD(TAG, "Check storage present = %s", TRUEFALSE(ret));
#ifdef FATFS_FS_TEST
  if ((ret) && (this->is_mount())) {
    std::string test_path = "0:/pic";
    this->test_fs(test_path);
  }
#endif
  return ret;
}

//--------------------------------------------------------------------------------

bool FatESP32sdspi::reinit_driver(StorageState state) {
  if (state == StorageState::MEDIA_UNUSED)
    return false;

  //  if media inserted but not mounted yet
  if ((state == StorageState::MEDIA_PRESENT) && (!this->is_mount())) {
    // TODO: Check Error
    // this->uninit_driver();

    this->init_driver();
    if (this->init_io()) {
      // Check is card present

      if (!this->mount()) {
        ESP_LOGE(TAG, "Reinit. Cannot mount. Error");
        this->uninit_io();  //  Say driver  need init in next time
        this->relese_driver();
        return false;
      }
      ESP_LOGD(TAG, "Reinit. state = MEDIA_PRESENT");
    } else {
      this->relese_driver();
      return false;
    }
  }
  //  if media ejected but still not unmounted
  else if ((state == StorageState::MEDIA_ABSENT) && (this->is_mount())) {
    // TODO: Check Error
    this->unmount();
    this->uninit_io();  //  Say driver  need init in next time
    this->relese_driver();
    ESP_LOGD(TAG, "Reinit. state = MEDIA_ABSENT");
  }

  return true;
}

/****************************************************************
 *
 * @brief   Testing connected driver. Open and list root dir.
 *
 * @return true
 * @return false
 */
bool FatESP32sdspi::test_fs(std::string &path) {
  uint8_t res;

  ESP_LOGD(TAG, "TEST FS for path %s", path.c_str());

  DirObject *dir_ptr = this->open_dir(path);
  res = (uint8_t) dir_ptr->file_error();
  if (res != FR_OK) {
    ESP_LOGE(TAG, "Dir open error: (%d) %s", res, fs_errstr(res));
    return false;
  }

  FatInfo *fat_obj = dir_ptr->get_next();
  while (fat_obj != NULL) {
    ESP_LOGD(TAG, "Found %s: %s", fat_obj->is_dir() ? "DIR" : "FILE", fat_obj->get_full_path().c_str());

    if (!fat_obj->is_dir()) {
      fatfs::FileObject *fl = this->open_file(fat_obj, FAT_F_READ);
      res = (uint8_t) this->file_error();
      if (res != FR_OK) {
        ESP_LOGE(TAG, "Open file error: %s", fs_errstr(res));
      }
      fl->close();
    }

    ESP_LOGV(TAG, "Read next object in directory");
    fat_obj = dir_ptr->get_next();
    if ((uint8_t) dir_ptr->file_error() != FR_OK) {
      ESP_LOGE(TAG, "Read dir entry error: %s", fs_errstr(res));
      break;
    }
  }
  return true;
}

}  // namespace fatfs_esp32_sdspi
}  // namespace esphome
#endif
