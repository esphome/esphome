#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include <cstdint>
#include <string>

namespace esphome {
namespace storage {

/*  File open modes */
static const uint8_t OPEN_READ = 0x01;
static const uint8_t OPEN_WRITE = 0x02;
static const uint8_t OPEN_APPEND = 0x04;
static const uint8_t OPEN_CREATE = 0x08;

/*  File or directory attributes */
static const uint8_t ATTR_HIDDEN = 1;    /* 0: File hidden */
static const uint8_t ATTR_SYSTEM = 2;    /* 0: file is system file */
static const uint8_t ATTR_PROTECTED = 3; /* 0: File protected (readonly) */

/**
 * @brief Represent Directory object. Object is opened in class constructor.
 * For close directory object pointer it is need to call class destructor.
 *
 */
class DirObj {
 public:
  /**
   * @brief Each subsequent  call 'next', return next name in directory contents list.
   *
   * @return std::string  next name in directory content list.  For end of list return Empty string;
   */
  virtual std::string next() = 0;
  /**
   * @brief Return error (if any) for open operaton(construct) or read name.
   *
   * @return uint8_t
   */
  virtual uint8_t error() = 0;
  /**
   * @brief Return last error name.
   *
   * @return const char*
   */
  virtual const char *error_str() = 0;
};

/**
 * @brief Represent file object.    Opened in constructor. Close in destructor.
 *
 */
class FileObj {
 public:
  /**
   * @brief Move read and read pointer for specifyed position.
   *
   * @param pos Poition
   * @return bool Has error
   */
  virtual bool seek(int32_t pos) = 0;
  /**
   * @brief Read  data block from current read position.
   *
   * @param data Memory areaa where to place read data
   * @param length max read data length
   * @return int32_t Exatly read bytes. -1 for any error; 0 if end of file reached
   */
  virtual int32_t read(uint8_t *data, size_t length) = 0;
  /**
   * @brief Write buffer to file from ceurrent write position.
   *
   * @param data  Tha data buffer write to file.
   * @param length  Length of data buffer
   * @return int32_t  really wretten bytes.  -1 for any error;
   */
  virtual int32_t write(uint8_t *data, size_t length) = 0;
  /**
   * @brief Error num of last io operation
   *
   * @return uint8_t
   */
  virtual uint8_t error() = 0;
  /**
   * @brief Return last error name.
   *
   * @return const char*
   */
  virtual const char *error_str() = 0;
};

/**
 * @brief Provide filesystems interconnection.
 * Get files and directory access, attributes access and moving files.
 */
class FileProvider {
 public:
  /**
   * @brief  Check is filesystem is mounted and ready for access
   *
   * @return true  ready
   * @return false  not ready
   */
  virtual bool is_ready() = 0;
  /**
   * @brief Check if path is directory.
   *
   * @param path
   * @return true
   * @return false
   */
  virtual bool is_dir(std::string path) = 0;

  /**
   * @brief Return file size.
   *
   * @param path
   * @return size_t
   */
  virtual size_t get_size(std::string path) = 0;

  /**
   * @brief Get file or directory attribute
   *
   * @param path  Path to object
   * @param attr_name Attribute nume
   * @return true   Is attribute set
   * @return false  Is does not set
   */
  virtual bool get_attr(std::string path, uint8_t attr_name) = 0;

  /**
   * @brief Set file or directory attribute
   *
   * @param path
   * @param attr_name
   * @param attr
   */
  virtual void set_attr(std::string path, uint8_t attr_name, bool attr) = 0;
  /**
   * @brief Open file and return FileObj for working with files
   *
   * @param path FIle path
   * @param mode Open mode
   * @return FileObj* Pointer to open file
   */
  virtual FileObj *open_file(std::string path, uint8_t mode) = 0;
  /**
   * @brief Open directory for get directory content listing
   *
   * @param path Path to directory
   * @return DirObj*  Pointer to opened directory
   */
  virtual DirObj *open_dir(std::string path) = 0;
  /**
   * @brief Move file from one path to anather.
   * Note: the file is not transferred if the paths are from different storages.
   *
   * @param from_path
   * @param to_path
   * @return true  Done
   * @return false  Any error. Check error()
   */
  virtual bool rename(std::string from_path, std::string to_path) = 0;

  /**
   * @brief Delete file or directory
   *
   * @param path
   * @return true Done
   * @return false Any error. Check error()
   */
  virtual bool del(std::string path) = 0;

  /**
   * @brief Create directory
   *
   * @param path
   * @return true Done
   * @return false Any error. Check error()
   */
  virtual bool mk_dir(std::string path) = 0;

  /**
   * @brief Return err number of last operation
   *
   * @return uint8_t
   */
  virtual uint8_t error() = 0;

  /**
   * @brief Return descriotion string of last error
   *
   * @return const char*
   */
  virtual const char *error_str() = 0;
};

}  // namespace storage
}  // namespace esphome
