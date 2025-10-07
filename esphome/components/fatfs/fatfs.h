#pragma once
#include "esphome/core/gpio.h"
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"
#include "esphome/core/time.h"

namespace esphome {
namespace fatfs {
/**
 * @brief An abstract API  for implemmenting FAT file system access on any storage media.
 * Primary provide nothods description for covering Generic FAT Filesystem Module
 * (fatfslib - https://elm-chan.org/fsw/ff/)
 *
 */

// #define AM_RDO 0x01 /* Read only */
// #define AM_HID 0x02 /* Hidden */
// #define AM_SYS 0x04 /* System */
// #define AM_DIR 0x10 /* Directory */
// #define AM_ARC 0x20 /* Archive */

// /* Filesystem type (FATFS.fs_type) */
// #define FS_FAT12 1
// #define FS_FAT16 2
// #define FS_FAT32 3
// #define FS_EXFAT 4

//  File open flags
#define FAT_F_READ 0x01           // NOLINT
#define FAT_F_WRITE = 0x02        // NOLINT
#define FAT_F_OPEN_EXISTING 0x00  // NOLINT
#define FAT_F_CREATE_NEW 0x04     // NOLINT
#define FAT_F_CREATE_ALWAYS 0x08  // NOLINT
#define FAT_F_OPEN_ALWAYS 0x10    // NOLINT  // 00010000
#define FAT_F_OPEN_APPEND 0x30    // NOLINT // 00110000
#define FAT_F_OPEN_TRUNCATE 0x40  // NOLINT// 01000000

enum class StorageState : uint8_t {
  MEDIA_UNUSED = 0,
  MEDIA_ABSENT = 1,
  MEDIA_PRESENT = 2,
};

enum class FatError : uint8_t {
  FR_OK = 0,                   /* (0) Succeeded */
  FR_DISK_ERR = 1,             /* (1) A hard error occurred in the low level disk I/O layer */
  FR_INT_ERR = 2,              /* (2) Assertion failed */
  FR_NOT_READY = 3,            /* (3) The physical drive cannot work */
  FR_NO_FILE = 4,              /* (4) Could not find the file */
  FR_NO_PATH = 5,              /* (5) Could not find the path */
  FR_INVALID_NAME = 6,         /* (6) The path name format is invalid */
  FR_DENIED = 7,               /* (7) Access denied due to prohibited access or directory full */
  FR_EXIST = 8,                /* (8) Access denied due to prohibited access */
  FR_INVALID_OBJECT = 9,       /* (9) The file/directory object is invalid */
  FR_WRITE_PROTECTED = 10,     /* (10) The physical drive is write protected */
  FR_INVALID_DRIVE = 11,       /* (11) The logical drive number is invalid */
  FR_NOT_ENABLED = 12,         /* (12) The volume has no work area */
  FR_NO_FILESYSTEM = 13,       /* (13) There is no valid FAT volume */
  FR_MKFS_ABORTED = 14,        /* (14) The f_mkfs() aborted due to any problem */
  FR_TIMEOUT = 15,             /* (15) Could not get a grant to access the volume within defined period */
  FR_LOCKED = 16,              /* (16) The operation is rejected according to the file sharing policy */
  FR_NOT_ENOUGH_CORE = 17,     /* (17) Buffer could not be allocated */
  FR_TOO_MANY_OPEN_FILES = 18, /* (18) Number of open files > FF_FS_LOCK */
  FR_INVALID_PARAMETER = 19    /* (19) Given parameter is invalid */
};

const char *fs_errstr(uint8_t);

/**
 * @brief  When created it contains addition information about filesystem's object
 *
 *
 */
class FatInfo {
 public:
  /**
   * @brief Returns is object  existed
   *
   * @return true  if exist
   * @return false  if not exist
   */
  virtual bool is_exist() { return false; };
  /**
   * @brief  Returns flag is object a directory
   *
   * @return true
   * @return false
   */
  virtual bool is_dir() { return false; };
  /**
   * @brief Returns flag is object write protected
   *
   * @return true
   * @return false
   */
  virtual bool is_readonly() { return false; };
  /**
   * @brief Returns flag is object a system object
   *
   * @return true
   * @return false
   */
  virtual bool is_sys() { return false; };
  /**
   * @brief Returns flag is object hiddent object
   *
   * @return true
   * @return false
   */
  virtual bool is_hidden() { return false; };
  /**
   * @brief Returens file size.  For directory return 0
   *
   * @return size_t
   */
  virtual size_t size() = 0;
  virtual ESPTime *get_cr_date() { return NULL; };
  /**
   * @brief Returns objects name
   *
   * @return std::string
   */
  virtual std::string get_name() { return std::string(); };
  /**
   * @brief Returns full path to object without filename
   *
   * @return std::string
   */
  virtual std::string get_path() { return std::string(); };
  /**
   * @brief return drive name (first part of path)
   *
   * @return std::string
   */
  virtual std::string get_drive() { return std::string(); };
  /**
   * @brief Get the full path to object includinf object name
   *
   * @return std::string
   */
  virtual std::string get_full_path() { return std::string(); };
};

/**
 * @brief Represent file object. Provide access to object contents.
 * Cover FF_function from fatfs @ref https://elm-chan.org/fsw/ff/
 *
 */
class FileObject {
 public:
  /**
   * @brief   Open file for read and write, depends of mode flags.
   * The f_open function opens a file and creates a file object. It is the identifier for subsequent
   * read/write operations to the file. After the function succeeded, the file object is valid.
   * If the function failed, the file object is set invalid.
   * Open file should be closed with f_close function after the session of the file access.
   * If any change to the file has been made and not closed prior to power off,
   * media removal or re-mount, or the file can be collapsed.
   *
   * @param mode  if combination of next flags:
   *    FAT_F_READ - Specifies read access to the file. Data can be read from the file.
   *    FAT_F_WRITE - Specifies write access to the file. Data can be written to the file. Combine with FA_READ for
   * read-write access. FAT_F_OPEN_EXISTING - Opens the file. The function fails if the file is not existing. (Default)
   *    FAT_F_CREATE_NEW    - Creates a new file. The function fails if the file is existing.
   *    FAT_F_CREATE_ALWAYS  - Creates a new file. If the file is existing, the file is truncated and overwritten.
   *    FAT_F_OPEN_ALWAYS  - Opens the file. If it is not exist, a new file is created.
   *    FAT_F_OPEN_APPEND  - Same as FA_OPEN_ALWAYS except the read/write pointer is set end of the file.
   * @return true File opened
   * @return false  fileopen fail. see @ref file_error
   */
  virtual bool open(uint16_t mode) { return false; };

  /**
   * @brief  CLose file. Flush to media&
   *
   */
  virtual void close() { return; };
  /**
   * @brief Reads data from a file
   *
   * @param buf Pointer to the buffer to store the read data.
   * @param size  Number of bytes to read in range of UINT type. If the file needs to be read fast, it should be read in
   * large chunk as possible.
   * @return int32_t Number of bytes read
   */
  virtual int32_t read(void *buf, size_t size) = 0;
  /**
   * @brief Writes data to a file. The function starts to read data from the file at the file
   * offset pointed by read/write pointer of the file object. The read/write pointer advances
   * as number of bytes read. After the function succeeded, returned size should be checked to detect end of the file.
   * In case of returned size < requested size, it means the read/write pointer hit end of the file during read
   * operation.
   *
   * @param buf Pointer to the data to be written.
   * @param size Specifies number of bytes to write in range of UINT type. If the data needs to be written fast, it
   * should be written in large chunk as possible.
   * @return int32_t The number of bytes written. This value is always valid after the function call regardless of the
   * function return code. See @ref file_error
   */
  virtual int32_t write(void *buf, size_t size) = 0;
  /**
   * @brief Moves the file read/write pointer of an open file object.
   * It can also be used to expand the file size (cluster pre-allocation).
   * The function starts to write data to the file at the file offset pointed by read/write pointer
   * of the file object. The read/write pointer advances as number of bytes written.
   * After the function succeeded, returned size should be checked to detect the disk full.
   * In case of returned size < requested size, it means the volume got full during the write operation.
   * The function can take a time when the volume is full or close to full.
   *
   * @param pos Byte offset from top of the file to set read/write pointer.
   * @return true  if success
   * @return false  any error See @ref file_error
   */
  virtual bool lseek(size_t pos) { return false; };
  /**
   * @brief Truncates the file size. The f_truncate function truncates the file size to the current file read/write
   * pointer. This function has no effect if the file read/write pointer is already pointing end of the file.
   *
   * @return true  if success
   * @return false  any error See @ref file_error
   */
  virtual bool truncate() { return false; };
  /**
   * @brief flushes the cached information of a writing file. See @ref https://elm-chan.org/fsw/ff/doc/sync.html
   *
   */
  virtual void flush() { return; };
  /**
   * @brief Test for end-of-file
   *
   * @return true
   * @return false
   */
  virtual bool is_eof() { return false; };
  /**
   * @brief Gets the current read/write pointer of a file.
   *
   * @return uint32_t
   */
  virtual uint32_t get_pos() = 0;
  /**
   * @brief Get error num of last operation.
   *
   * @return FatError
   */
  virtual FatError file_error() = 0;
};

/**
 * @brief Open directory object. and set content read pointer to first object
 *
 */
class DirObject {
 public:
  /**
   * @brief Subsequently reads an item of the directory. Directory item return as FatInfo object
   * If end of dir content list reached return NULL pointer.
   *
   * @return FatInfo* Describe read item
   */
  virtual FatInfo *get_next() { return NULL; };
  /**
   * @brief Rewind readint to first item in list
   *
   * @return true
   * @return false
   */
  virtual bool reset() { return false; };
  /**
   * @brief Get error num of last operation.
   *
   * @return FatError
   */
  virtual FatError file_error() = 0;
};

/** --------------------------------------------------------------------------------
 *
 * @brief   Card eject/insert interrupt data
 *
 */
struct MediaDetectInterrupt {
  volatile bool present{true};
  bool init{false};
  static void inserted(MediaDetectInterrupt *data);
  static void ejected(MediaDetectInterrupt *data);
};

/** --------------------------------------------------------------------------------
 *
 * @brief  Main class for accessing FAT file structure on the starage media
 *
 */
class FatFs {
 public:
  virtual void set_drive_id(uint8_t drive_id);
  uint8_t get_grive_id() { return drive_id_; };
  std::string get_drive_name() { return logical_drv_; };

  void set_cd_pin(InternalGPIOPin *pin) { cd_pin_ = pin; };
  InternalGPIOPin *get_cd_pin() { return cd_pin_; };
  StorageState is_storage_state_interrupt();
  bool init_storage_state_interrupt();
  /**
   * @brief Return true if file system mounted and ready to access
   *
   * @return true
   * @return false
   */
  virtual bool is_mount() { return false; };
  /**
   * @brief Get the filesystems object information class
   *
   * @param path  Path to object (file or directory)
   * @return FatInfo*
   */
  virtual FatInfo *get_info(std::string path) { return NULL; };
  /**
   * @brief Check if object exist in fs
   *
   * @param path path to object
   * @return true
   * @return false
   */
  virtual bool is_exist(std::string path) { return false; };
  /**
   * @brief Open object as file otherwise return NULL pointer
   *
   * @param path path to file
   * @param mode open mode (see @ref FileObject::open)
   * @return FileObject*
   */
  virtual FileObject *open_file(std::string path, uint8_t mode) { return NULL; };
  /**
   * @brief Open object as file otherwise return NULL pointer
   *
   * @param obj FatInfo object for file
   * @param mode open mode (see @ref FileObject::open)
   * @return FileObject*
   */
  virtual FileObject *open_file(FatInfo *obj, uint8_t mode) { return NULL; };
  /**
   * @brief Open fs object as directory otherwise return NULL pointer
   *
   * @param path path to directory
   * @return DirObject*
   */
  virtual DirObject *open_dir(std::string path) { return NULL; };
  /**
   * @brief
   *
   * @param obj FatInfo object for directory
   * @return DirObject*
   */
  virtual DirObject *open_dir(FatInfo *obj) { return NULL; };
  /**
   * @brief  Create directory.  Directory name is last part of path
   *
   * @param path path to new directory
   * @return DirObject*
   */
  virtual DirObject *mk_dir(const std::string path) { return NULL; };
  /**
   * @brief Create directory as children or specifyed obj with name
   *
   * @param obj Path to directory where to creat new one
   * @param name name of new directory
   * @return DirObject*
   */
  virtual DirObject *mk_dir(FatInfo *obj, const std::string name) { return NULL; };
  /**
   * @brief Removes a file or sub-directory from the volume.
   *
   * @param path path to directory
   * @return true
   * @return false
   */
  virtual bool del(const std::string path) { return false; };
  /**
   * @brief Renames and/or moves a file or sub-directory.
   *
   * @param path_from
   * @param path_to
   * @return true
   * @return false
   */
  virtual bool rename(const std::string path_from, const std::string path_to) { return false; };
  /**
   * @brief error of last FS operation
   *
   * @return FatError
   */
  virtual FatError file_error() = 0;
  /**
   * @brief Retutrn Next representation of last FS operaton error
   *
   * @return const char*
   */
  virtual const char *print_file_error() { return ""; };

 private:
  MediaDetectInterrupt media_present_st_;
  InternalGPIOPin *cd_pin_{NULL};  // DOWN -> No Card ;  UP -> Card present
  std::string logical_drv_;
  uint8_t drive_id_{0xFF};
  bool media_present_{false};
};

}  // namespace fatfs
}  // namespace esphome
