#include "fatfs_esp32.h"
#ifdef USE_ESP32
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
namespace fatfs_esp32 {

using namespace esphome::fatfs;
static const char *const TAG = "fatfs_esp32";

std::map<std::uint8_t, StorageIO *> db;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
using enum fatfs::StorageState;

//--------------------------------------------------------------------------------
DSTATUS ff_sd_initialize(uint8_t pdrv) {
  StorageIO *io_class = nullptr;
  if (db.find(pdrv) != db.end()) {
    io_class = db.at(pdrv);
  } else {
    ESP_LOGE(TAG, "IO driver not initilized, drv=%d", pdrv);
    return RES_ERROR;
  }
  if (io_class == NULL) {
    ESP_LOGE(TAG, "Cannot obtain driver drv=%d", pdrv);
    return RES_ERROR;
  }
  io_class->storage_init();
  ESP_LOGVV(TAG, "ff_sd_initialize, drv=%d, rc=%d", pdrv, io_class->storage_error());
  return io_class->storage_status();
}

//--------------------------------------------------------------------------------
DSTATUS ff_sd_status(uint8_t pdrv) {
  StorageIO *io_class;
  if (db.find(pdrv) != db.end()) {
    io_class = db.at(pdrv);
  } else {
    ESP_LOGE(TAG, "IO driver not initilized, drv=%d", pdrv);
    return RES_ERROR;
  }
  ESP_LOGVV(TAG, "ff_sd_status, drv=%d, rc=%d", pdrv, io_class->storage_error());

  return io_class->storage_status();
}

//--------------------------------------------------------------------------------
// DRESULT ff_sd_read(uint8_t pdrv, uint8_t *buffer, DWORD sector, UINT count)
DRESULT ff_sd_read(uint8_t pdrv, uint8_t *buffer, DWORD sector, UINT count) {
  StorageIO *io_class;
  if (db.find(pdrv) != db.end()) {
    io_class = db.at(pdrv);
  } else {
    ESP_LOGE(TAG, "ff_sd_read. IO driver not initilized, drv=%d", pdrv);
    return RES_ERROR;
  }
  DRESULT res = static_cast<DRESULT>(io_class->storage_read_sectors(buffer, sector, count));
  if (res != 0) {
    ESP_LOGE(TAG, "ff_sd_read. Read error, drv=%d, res=%d, rc=%d", pdrv, res, io_class->storage_error());
  } else {
    ESP_LOGVV(TAG, "ff_sd_read. Read OK, drv=%d, sector=%d, count=%d", pdrv, sector, count);
  }
  return res;
}

//--------------------------------------------------------------------------------
// DRESULT ff_sd_write(uint8_t pdrv, const uint8_t *buffer, DWORD sector, UINT count)
DRESULT ff_sd_write(uint8_t pdrv, const uint8_t *buffer, DWORD sector, UINT count) {
  StorageIO *io_class;
  if (db.find(pdrv) != db.end()) {
    io_class = db.at(pdrv);
  } else {
    ESP_LOGE(TAG, "ff_sd_write. IO driver not initilized, drv=%d", pdrv);
    return RES_ERROR;
  }

  DRESULT res = static_cast<DRESULT>(io_class->storage_write_sectors(buffer, sector, count));
  if (res != 0) {
    ESP_LOGE(TAG, "ff_sd_write. Read error, drv=%d, rc=%d", pdrv, io_class->storage_error());
  } else {
    ESP_LOGVV(TAG, "ff_sd_write. Read OK, drv=%d, sector=%d, count=%d", pdrv, sector, count);
  }

  return res;
}

//--------------------------------------------------------------------------------
DRESULT ff_sd_ioctl(uint8_t pdrv, uint8_t cmd, void *buff) {
  StorageIO *io_class;
  if (db.find(pdrv) != db.end()) {
    io_class = db.at(pdrv);
  } else {
    ESP_LOGE(TAG, "ff_sd_ioctl. IO driver not initilized, drv=%d", pdrv);
    return RES_ERROR;
  }
  DRESULT res = static_cast<DRESULT>(io_class->storage_ioctl(cmd, buff));
  ESP_LOGVV(TAG, "ff_sd_ioctl, drv=%d, cmd=%d, res=%d, rc=%d", pdrv, cmd, res, io_class->storage_error());
  return res;
}

/** --------------------------------------------------------------------------------
 *
 * @brief
 *
 */

bool FatESP32::set_io_driver(StorageIO *io_class) {
  io_ = io_class;

  if (this->get_grive_id() == 0xFF) {
    for (uint8_t i = 0; i < 11; i++) {
      if (db.find(i) == db.end()) {
        this->set_drive_id(i);
        break;
      }
    }
  } else {
    //  Check is driver_id unique
    if (db.find(this->get_grive_id()) != db.end()) {
      ESP_LOGE(TAG, "Init driver id fail. Duplicate id x%02x", this->get_grive_id());
      return false;
    }
  }

  //  Check is IO driver registered
  if (io_ == NULL) {
    ESP_LOGE(TAG, "Init IO driver failed. Driver not set.");
    return false;
  }
  //  Register IO Driver
  db.insert_or_assign(this->get_grive_id(), io_);
  return true;
}

/** --------------------------------------------------------------------------------
 *
 * @brief
 *
 */
void FatESP32::init_driver() {
  sd_impl_.init = &ff_sd_initialize;
  sd_impl_.status = &ff_sd_status;
  sd_impl_.read = &ff_sd_read;
  sd_impl_.write = &ff_sd_write;
  sd_impl_.ioctl = &ff_sd_ioctl;
  ff_diskio_register(this->get_grive_id(), &sd_impl_);
  ESP_LOGD(TAG, "init drv=%s", this->get_drive_name().c_str());
}

/** --------------------------------------------------------------------------------
 *
 * @brief
 *
 */
void FatESP32::relese_driver() {
  ff_diskio_register(this->get_grive_id(), NULL);
  ESP_LOGV(TAG, "Release driver. drv=%s", this->get_drive_name().c_str());
}

/** --------------------------------------------------------------------------------
 * @brief
 *
 * @return true
 * @return false
 */
bool FatESP32::mount() {
  FATFS *local_fs;
  // char *ldrv = strdup(this->get_drive_name().c_str());

  esp_err_t err = esp_vfs_fat_register(path_.c_str(), this->get_drive_name().c_str(), FAT_MAX_FILES, &local_fs);
  if (err == ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "esp_vfs_fat_register failed (0x%x)%s: SD is registered.", err, esp_err_to_name(err));
    return false;
  } else if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_vfs_fat_register failed (0x%x)%s", err, esp_err_to_name(err));
    return false;
  }

  FRESULT res = f_mount(local_fs, this->get_drive_name().c_str(), 1);
  if (res != FR_OK) {
    ESP_LOGE(TAG, "f_mount failed: (0x%x)%s", res, fs_errstr(res));
    esp_vfs_fat_unregister_path(path_.c_str());
    return false;
  }
  ESP_LOGD(TAG, "mount drv=%s, mount point=%s", this->get_drive_name().c_str(), path_.c_str());
  fs_ = local_fs;
  return true;
}

/** --------------------------------------------------------------------------------
 * @brief
 *
 * @return true
 * @return false
 */
void FatESP32::unmount() {
  if (fs_ != NULL) {
    FRESULT res = f_mount(NULL, this->get_drive_name().c_str(), 0);
    if (res != FR_OK)
      ESP_LOGE(TAG, "unmount problem: (0x%x)%s", res, fs_errstr(res));
    esp_err_t err = esp_vfs_fat_unregister_path(path_.c_str());
    if (err != ESP_OK)
      ESP_LOGE(TAG, "esp_vfs_fat_unregister_path problem (0x%x)%s", err, esp_err_to_name(err));
    fs_ = NULL;
    ESP_LOGV(TAG, "unmount drv=%s", this->get_drive_name().c_str());
  }
}

//--------------------------------------------------------------------------------

fatfs::FatInfo *FatESP32::get_info(std::string &path) { return new FatESP32Info(path); }

//--------------------------------------------------------------------------------

bool FatESP32::is_exist(std::string &path) {
  auto fs = FatESP32Info(path);
  return fs.is_exist();
}

//--------------------------------------------------------------------------------

fatfs::FileObject *FatESP32::open_file(std::string &path, uint8_t mode) {
  FatESP32File *fl = new FatESP32File(path, mode);
  error_ = fl->file_error();
  // if (fl->error() != fatfs::FatError::FR_OK) {
  //   return NULL;
  // }
  return fl;
}

//--------------------------------------------------------------------------------

fatfs::FileObject *FatESP32::open_file(fatfs::FatInfo *obj, uint8_t mode) {
  FatESP32File *fl = new FatESP32File(*obj, mode);
  error_ = fl->file_error();
  // if (fl->error() != fatfs::FatError::FR_OK) {
  //   return NULL;
  // }
  return fl;
}

//--------------------------------------------------------------------------------

fatfs::DirObject *FatESP32::open_dir(std::string &path) {
  ESP_LOGV(TAG, "Open dir from path %s", path.c_str());
  FatESP32Dir *d_obj = new FatESP32Dir(path);
  error_ = d_obj->file_error();
  return d_obj;
}

//--------------------------------------------------------------------------------

fatfs::DirObject *FatESP32::open_dir(fatfs::FatInfo *obj) {
  ESP_LOGV(TAG, "Open dir from FatInfo %s", obj->get_full_path().c_str());
  FatESP32Dir *d_obj = new FatESP32Dir(*obj);
  error_ = d_obj->file_error();
  return d_obj;
}

//--------------------------------------------------------------------------------

fatfs::DirObject *FatESP32::mk_dir(std::string &path) {
  error_ = static_cast<fatfs::FatError>(f_mkdir(path.c_str()));
  if (error_ != fatfs::FatError::FR_OK) {
    ESP_LOGE(TAG, "f_mkdir  %s failled (0x%x) %s", path.c_str(), static_cast<int>(error_),
             fs_errstr(static_cast<int>(error_)));
    return NULL;
  }
  FatESP32Dir *dptr = new FatESP32Dir(path);
  error_ = dptr->file_error();
  return dptr;
}

//--------------------------------------------------------------------------------

fatfs::DirObject *FatESP32::mk_dir(fatfs::FatInfo *obj, std::string &name) {
  std::string dir = obj->get_full_path() + "/" + name;
  return this->mk_dir(dir);
}

//--------------------------------------------------------------------------------

bool FatESP32::del(std::string &path) {
  error_ = static_cast<fatfs::FatError>(f_unlink(path.c_str()));
  if (error_ != fatfs::FatError::FR_OK) {
    ESP_LOGE(TAG, "f_unlink  %s failled (0x%x) %s", path.c_str(), static_cast<int>(error_),
             fs_errstr(static_cast<int>(error_)));
    return false;
  }
  return true;
}

//--------------------------------------------------------------------------------

bool FatESP32::rename(std::string &path_from, std::string &path_to) {
  error_ = static_cast<fatfs::FatError>(f_rename(path_from.c_str(), path_to.c_str()));
  if (error_ != fatfs::FatError::FR_OK) {
    ESP_LOGE(TAG, "f_rename %s - %s failled (0x%x) %s", path_from.c_str(), path_to.c_str(), static_cast<int>(error_),
             fs_errstr(static_cast<int>(error_)));
    return false;
  }
  return true;
}

}  // namespace fatfs_esp32
}  // namespace esphome
#endif
