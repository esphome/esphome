#pragma once
#include "esphome/core/defines.h"
#ifdef USE_ESP32
#include "esphome/core/gpio.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include <map>
#include "esphome/core/log.h"
#include "esphome/components/fatfs/fatfs.h"
#include "esphome/core/time.h"
#include "fatfs_esp32_file.h"

namespace esphome {
namespace fatfs_esp32 {
#ifndef FAT_MAX_FILES
#define FAT_MAX_FILES 5  // NOLINT
#endif

using DSTATUS = BYTE;

DSTATUS ff_sd_initialize(uint8_t pdrv);
DSTATUS ff_sd_status(uint8_t pdrv);

/** --------------------------------------------------------------------------------
 * @brief The disk_read function is called to read data from the storage device.
 *
 * @param pdrv [IN] Physical drive number to identify the target device.
 * @param buffer [OUT] Pointer to the first item of the byte array
 *      to store read data. Size of read data will be the sector size * count bytes.
 * @param sector [IN] Start sector number in LBA. The data type LBA_t is an alias of
 *      DWORD or QWORD depends on the configuration option.
 * @param count [IN] Number of sectors to read.
 * @return DRESULT
 * RES_OK (0) The function succeeded.
 * RES_ERROR An unrecoverable hard error occured during the read operation.
 * RES_PARERR Invalid parameter.
 * RES_NOTRDY The device has not been initialized.
 */
DRESULT ff_sd_read(uint8_t pdrv, uint8_t *buffer, DWORD sector, UINT count);

/** --------------------------------------------------------------------------------
 * @brief
 *
 * @param pdrv Physical drive number to identify the target device.
 * @param buffer Pointer to the first item of the byte array to be written.
 *      The size of data to be written is sector size * count bytes.
 * @param sector
 *      Start sector number in LBA. The data type LBA_t is an
 *      alias of DWORD or QWORD depends on the configuration option.
 * @param count
 *      Number of sectors to write.
 * @return DRESULT
 * RES_OK (0) The function succeeded.
 * RES_ERROR An unrecoverable hard error occured during the write operation.
 * RES_WRPRT The medium is write protected.
 * RES_PARERR Invalid parameter.
 * RES_NOTRDY The device has not been initialized.
 */
DRESULT ff_sd_write(uint8_t pdrv, const uint8_t *buffer, DWORD sector, UINT count);

DRESULT ff_sd_ioctl(uint8_t pdrv, uint8_t cmd, void *buff);

/** ****************************************************************************************
 * @brief  Diskio operation wrapper api, required  as low level for FatFS lib and esp_idf vfat
 *         need to be implemented for IO driver implementation
 *
 *   Method return statueses:
 * RES_OK  0 - Successful
 * RES_ERROR  1 - R//W Error
 * RES_WRPRT 2 - Write Protected
 * RES_NOTRDY 3 - Not Ready
 * RES_PARERR 4 - Invalid Parameter
 */
class StorageIO {
 public:
  /**
   * @brief init driver
   *
   * @return true
   * @return false
   */
  virtual bool storage_init() = 0;

  /**
   * @brief reset driver initialization
   */
  virtual void storage_uninit() = 0;

  /**
   * @brief provide writing one or more sectors
   *
   * @param buffer buffer with data need wreite
   * @param sector start sector
   * @param count total sectors need to write
   * @return uint8_t
   */
  virtual uint8_t storage_write_sectors(const uint8_t *buffer, DWORD sector, UINT count) = 0;

  /**
   * @brief provide reading one or more sectors
   *
   * @param buffer buffer for save read data
   * @param sector start sector for read
   * @param count nom of sectors to read
   * @return uint8_t
   */
  virtual uint8_t storage_read_sectors(uint8_t *buffer, DWORD sector, UINT count) = 0;

  /**
   * @brief Used for special data requests for FAT lib
   *
   * @param cmd
   *    CTRL_SYNC
   *    GET_SECTOR_COUNT
   *    GET_SECTOR_SIZE
   *    GET_BLOCK_SIZE
   *
   * @param buff buffer for store result
   * @return uint8_t  operation status
   */
  virtual uint8_t storage_ioctl(uint8_t cmd, void *buff) = 0;

  /**
   * @brief The current drive status is returned in combination of status flags described below.
   *        FatFs refers only STA_NOINIT and STA_PROTECT.
   * @return uint8_t  - status flag.  Can bee one of following:
   *
   * STA_READY  (0x00) - Ready for operation
   * STA_NOINIT (0x01) - Indicates that the device has not been initialized and not ready to work.
   *    This flag is set on system reset, media removal or failure of disk_initialize function.
   *    It is cleared on disk_initialize function succeeded. Any media change that occurs
   *    asynchronously must be captured and reflect it to the status flags, or auto-mount
   *    function will not work correctly. If the system does not support media change detection,
   *    application program needs to explicitly re-mount the volume with f_mount function
   *    after each media change.
   * STA_NODISK (0x02)-  Indicates that no medium in the drive. This is always cleared when the drive
   *    is non-removable class. Note that FatFs does not refer this flag.
   * STA_PROTECT (0x04) - Indicates that the medium is write protected.
   *    This is always cleared when the drive has no write protect function.
   *    Not valid if STA_NODISK is set.
   */
  virtual uint8_t storage_status() = 0;

  /**
   * @brief  Compare storage status returned by @ref storage_status "storage_status"
   * with exact status
   *
   * @param st_const  status tocompare with
   * @return true   if pointed status set
   * @return false  if pointed status not set
   */
  virtual bool is_storage_status(uint8_t st_const) { return (this->storage_status() & st_const) > 0; };

  /**
   * @brief  Return last IO operation error (if any)
   *
   * @return uint8_t
   */
  virtual uint8_t storage_error() = 0;
};

/** ****************************************************************************************
 * @brief  Provide implementation of fatfs  API. See @ref fatfs::FatFs component.
 * Cover FATFS lib from  esp_idf sdk.  FATFS lib also accessible under Arduino framework.
 * Implementation controll fat mount/unmount, check if media exist
 * and of course  provide access to fs objects (files and dirs)
 *
 * It does not provide storage media access, so this implementation
 * need to be extended with storage media driver  StorageIO @ref fatfs_esp32::StorageIO.
 * Injects driver class with "set_io_driver" method
 */
class FatESP32 : public fatfs::FatFs {
 public:
  bool set_io_driver(StorageIO *io_class);
  void set_mount_point(std::string &path) { path_ = std::move(path); };
  std::string get_mount_point() { return path_; };

  /**
   * @brief   Save pointers for read/write call and send it to ff_diskio_register (from fatfs)
   */
  virtual void init_driver();

  /**
   * @brief  Reset driver state
   */
  virtual void relese_driver();

  /**
   * @brief   Mount Fat filesystems if media available
   *
   * @return true  mout success
   * @return false  mount failed
   */
  virtual bool mount();

  /**
   * @brief  Remove mount registrations
   */
  virtual void unmount();

  /**
   * @brief  Check is FS is mounted
   *
   * @return true   FS available
   * @return false  FS not mounted
   */
  bool is_mount() override { return fs_ != NULL; };

  fatfs::FatInfo *get_info(std::string &path) override;
  bool is_exist(std::string &path) override;
  fatfs::FileObject *open_file(std::string &path, uint8_t mode) override;
  fatfs::FileObject *open_file(fatfs::FatInfo *obj, uint8_t mode) override;
  fatfs::DirObject *open_dir(std::string &path) override;
  fatfs::DirObject *open_dir(fatfs::FatInfo *obj) override;
  fatfs::DirObject *mk_dir(std::string &path) override;
  fatfs::DirObject *mk_dir(fatfs::FatInfo *obj, std::string &name) override;
  bool del(std::string &path) override;
  bool rename(std::string &path_from, std::string &path_to) override;
  fatfs::FatError file_error() override { return error_; };
  const char *print_file_error() override { return fs_errstr(static_cast<int>(error_)); };

 private:
  StorageIO *io_{NULL};
  ff_diskio_impl_t sd_impl_;
  std::string path_ = "/sdcard";
  FATFS *fs_{NULL};
  fatfs::FatError error_ = fatfs::FatError::FR_OK;
};

extern std::map<std::uint8_t, StorageIO *> db;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace fatfs_esp32
}  // namespace esphome
#endif
