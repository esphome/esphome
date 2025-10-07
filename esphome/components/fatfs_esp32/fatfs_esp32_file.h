#pragma once
#include "esphome/core/gpio.h"
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include <map>
#include "esphome/core/log.h"
#include "esphome/components/fatfs/fatfs.h"
#include "esphome/core/time.h"

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

using fatfs::fs_errstr;

/** --------------------------------------------------------------------------------
 *
 * @brief Provde implementation for @ref fatfs::FatInfo API
 *
 */
class FatESP32Info : public fatfs::FatInfo {
  // class FatESP32Info : virtual public fatfs::FatInfo {
 public:
  FatESP32Info() = default;
  FatESP32Info(fatfs::FatInfo &source);
  FatESP32Info(std::string path);
  FatESP32Info &operator=(const FatESP32Info &source);
  // void load(std::string path, FILINFO *finfo);
  bool is_exist() override { return exist_; };
  bool is_dir() override { return is_dir_; };
  bool is_readonly() override { return is_ro_; };
  bool is_sys() override { return is_system_; };
  bool is_hidden() override { return is_hidden_; };
  size_t size() override { return size_; };
  ESPTime *get_cr_date() override { return &create_date_; };
  std::string get_name() override { return name_; };
  std::string get_path() override { return path_; };
  std::string get_drive() override { return drive_; };
  std::string get_full_path() override;

 private:
  std::string name_{""};
  std::string path_{""};
  std::string drive_{""};
  size_t size_;
  bool exist_ = false;
  bool is_dir_ = false;
  bool is_hidden_ = false;
  bool is_system_ = false;
  bool is_ro_ = false;
  ESPTime create_date_;
};

/** --------------------------------------------------------------------------------
 *
 * @brief Provde implementation for @ref fatfs::FileObject API
 *
 */
class FatESP32File : public fatfs::FileObject, public FatESP32Info {
 public:
  FatESP32File(std::string path, uint16_t mode);
  FatESP32File(fatfs::FatInfo &finfo, uint16_t mode);
  ~FatESP32File();
  bool open(uint16_t mode) override;
  void close() override;
  int32_t read(void *buf, size_t size) override;
  int32_t write(void *buf, size_t size) override;
  bool lseek(size_t pos) override;
  bool truncate() override;
  void flush() override;
  bool is_eof() override;
  uint32_t get_pos() override;
  fatfs::FatError file_error() override { return static_cast<fatfs::FatError>(error_); };

 private:
  uint8_t error_ = FR_OK;
  FIL fptr_;
  bool is_open_ = false;
};

/** --------------------------------------------------------------------------------
 *
 * @brief Provde implementation for @ref fatfs::DirObject API
 *
 */
class FatESP32Dir : public fatfs::DirObject, public FatESP32Info {
 public:
  FatESP32Dir(std::string path);
  FatESP32Dir(fatfs::FatInfo &finfo);
  ~FatESP32Dir();
  bool open();
  fatfs::FatInfo *get_next() override;
  bool reset() override;
  fatfs::FatError file_error() override { return static_cast<fatfs::FatError>(error_); };

 private:
  uint8_t error_ = {FR_OK};
  FF_DIR dptr_;
  bool is_open_ = false;
};

}  // namespace fatfs_esp32
}  // namespace esphome
