#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/gpio.h"

namespace esphome {
namespace storage {

//
//  Storage read/write return errors
//
#define RC_OK 0     /* 0: Successful */
#define RC_ERROR 1  /* 1: R/W Error */
#define RC_WRPRT 2  /* 2: Write Protected */
#define RC_NOTRDY 3 /* 3: Not Ready */
#define RC_PARERR 4 /* 4: Invalid Parameter */

//
//  Command code for disk_ioctrl fucntion
//
// Generic command
#define CMD_CTRL_SYNC 0         // Complete pending write process (needed at FF_FS_READONLY == 0)
#define CMD_GET_SECTOR_COUNT 1  // Get media size (needed at FF_USE_MKFS == 1)
#define CMD_GET_SECTOR_SIZE 2   // Get sector size (needed at FF_MAX_SS != FF_MIN_SS)
#define CMD_GET_BLOCK_SIZE 3    //  Get erase block size (needed at FF_USE_MKFS == 1)
#define CMD_CTRL_TRIM \
  4  //  Inform device that the data on the block of sectors is no longer used (needed at FF_USE_TRIM == 1)

// Generic command
#define CMD_CTRL_POWER 5   // Get/Set power status
#define CMD_CTRL_LOCK 6    // Lock/Unlock media removal
#define CMD_CTRL_EJECT 7   // Eject media
#define CMD_CTRL_FORMAT 8  // Create physical format on the media

// MMC/SDC specific ioctl command
#define CMD_MMC_GET_TYPE 10    // Get card type
#define CMD_MMC_GET_CSD 11     // Get CSD
#define CMD_MMC_GET_CID 12     // Get CID
#define CMD_MMC_GET_OCR 13     // Get OCR
#define CMD_MMC_GET_SDSTAT 14  // Get SD status
#define CMD_ISDIO_READ 55      // Read data form SD iSDIO register
#define CMD_ISDIO_WRITE 56     // Write data to SD iSDIO register
#define CMD_ISDIO_MRITE 57     // Masked write data to SD iSDIO register

//  ATA/CF specific ioctl command
#define CMD_ATA_GET_REV 20    // Get F/W revision
#define CMD_ATA_GET_MODEL 21  // Get model name
#define CMD_ATA_GET_SN 22     // Get serial number

enum class StorageIntState : uint8_t {
  MEDIA_UNUSED = 0,
  MEDIA_ABSENT = 1,
  MEDIA_PRESENT = 2,
};
/** --------------------------------------------------------------------------------
 *
 * @brief   Media presend detect pin interupt handler
 *
 */
struct MediaDetectInterrupt {
  volatile bool present{true};
  bool init{false};
  static void inserted(MediaDetectInterrupt *data);
  static void ejected(MediaDetectInterrupt *data);
};

/** ****************************************************************************************
 *
 * @brief  Storage device driver interface.  Contains IO operation.
 *         Most information about device are returned by ioctl call.
 *
 * Methods return statueses:
 * BD_RC_OK  0 - Successful
 * BD_RC_ERROR  1 - R//W Error
 * BD_RC_WRPRT 2 - Write Protected
 * BD_RC_NOTRDY 3 - Not Ready
 * BD_RC_PARERR 4 - Invalid Parameter
 */
class Storage {
 public:
  /**
   * @brief Init driver
   *
   * @return true initilized
   * @return false if any error
   */
  virtual bool initialize() = 0;

  /**
   * @brief Reset driver initialization.
   * After succesfuul reset, it need to be initialized again.
   *
   * @param hard = true for hard reset (including power on off)
   *               false - perform release initialization
   *
   * @return false if any error
   */
  virtual void reset(bool hard) = 0;

  /**
   * @brief Provide writing one or more sectors
   *
   * @param buffer data buffer need to be written
   * @param sector start sector
   * @param count total sectors need to write
   * @return uint8_t
   */
  virtual uint8_t write_sectors(const uint8_t *buffer, uint32_t sector, unsigned int count) = 0;

  /**
   * @brief provide reading one or more sectors
   *
   * @param buffer buffer for save read data
   * @param sector start sector for read
   * @param count nom of sectors to read
   * @return uint8_t
   */
  virtual uint8_t read_sectors(uint8_t *buffer, uint32_t sector, unsigned int count) = 0;

  /**
   * @brief Used for special data requests for FAT lib
   *
   * @param cmd ( See #define IO_...)
   *
   * @param buff buffer for store result
   * @return uint8_t  operation status
   */
  virtual uint8_t ioctl(uint8_t cmd, void *buff) = 0;

  /**
   * @brief  Return is media initialized
   *
   * @return true initialized
   * @return false need initialize
   */
  virtual bool state_init() = 0;

  /**
   * @brief Indicate is media(disk) is present
   *
   * @return true Disk present
   * @return false Disk absent
   */
  virtual bool state_media() = 0;
  /**
   * @brief Indicate is media is write protected
   *
   * @return true  Protected
   * @return false
   */
  virtual bool state_protect() = 0;

  /**
   * @brief This flag is set after initialization, and need to be cleared after mount
   *
   * @return true
   * @return false
   */
  virtual bool state_remount() = 0;

  /**
   * @brief  Clear state_remount to flase.
   *
   */
  virtual void clear_remount() = 0;
  /**
   * @brief  Return last IO operations error (if any)
   *
   * @return uint8_t
   */
  virtual uint8_t error() = 0;

  bool init_media_state_interrupt();
  StorageIntState media_interrupt_state();

  void set_cd_pin(InternalGPIOPin *pin) { cd_pin_ = pin; };
  InternalGPIOPin *get_cd_pin() { return cd_pin_; };

 private:
  MediaDetectInterrupt media_present_st_;
  InternalGPIOPin *cd_pin_{NULL};  // DOWN -> No Card ;  UP -> Card present
  bool cur_mstate_{true};          // used for filtering out interruts with same state
};

}  // namespace storage
}  // namespace esphome
