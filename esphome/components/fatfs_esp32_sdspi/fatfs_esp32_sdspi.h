#pragma once
#include "esphome/core/defines.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/log.h"
#include "esphome/components/fatfs_esp32/fatfs_esp32.h"
#include "sdspi_driver.h"

#ifdef USE_ESP32
#include "diskio.h"
#endif

namespace esphome {
namespace fatfs_esp32_sdspi {
using namespace esphome::fatfs_esp32;

//  FatFS IO errors on media access interface

/** *******************************************************************************
 *
 * @brief   SdspiIO Class aggregate  sd card  low level driver for esphome spi
 * components  (SDSPIDriver)
 * and fatfs callbacks for use with esp_idf vfat implamentation (StorageIO)
 *
 */
class SdspiIO : public StorageIO,   //  vfat callbacks
                public SDSPIDriver  //  Driver/wraper for esphome's spi implementation
{
 public:
  /**
   * @brief Init drived and send spi init sequence
   *
   * @return true
   * @return false
   */
  bool storage_init() override;

  /**
   * @brief reset initialization
   *
   */
  void storage_uninit() override { this->uninit(); };

  uint8_t storage_write_sectors(const uint8_t *buffer, DWORD sector, UINT count) override {
    if (this->write_sectors(sector, buffer, count))
      return RES_OK;
    else
      return RES_ERROR;
  };

  /**
   * @brief Read one or more sectors
   *
   * @param buffer mem space where to read
   * @param sector start sector
   * @param count  num os sectors ti read
   * @return uint8_t
   */
  uint8_t storage_read_sectors(uint8_t *buffer, DWORD sector, UINT count) override {
    if (this->read_sectors(sector, buffer, count))
      return RES_OK;
    else
      return RES_ERROR;
  };

  /**
   * @brief   Retrun warious media data depenf of cmd  Cover for ioctl from SDSPIDriver.
   * Known CMD
   * CTRL_SYNC
   * GET_SECTOR_COUNT
   * GET_SECTOR_SIZE
   * GET_BLOCK_SIZE
   * see @ref https://elm-chan.org/fsw/ff/doc/dioctl.html "fatfs lib"
   *
   * @param cmd
   * @param buff
   * @return uint8_t
   */
  uint8_t storage_ioctl(uint8_t cmd, void *buff) override { return this->ioctl(cmd, buff); };

  /**
   * @brief  Return card status see @ref SDSPIDriver::drv_state() "drv_state description"
   *
   * @return uint8_t
   */
  uint8_t storage_status() override { return this->drv_state(); };

  /**
   * @brief  Return last io operation error from SDSPIDriver
   *
   * @return uint8_t  last error
   */
  uint8_t storage_error() override { return this->error(); }
};

/** *******************************************************************************
 *
 * @brief Initialize fatfs_esp32 component and inject sd card spi low level driver
 *
 */
class FatESP32sdspi : private FatESP32, public PollingComponent {
 public:
  FatESP32sdspi() { io_driver_ = new SdspiIO(); };

  void setup() override;
  void loop() override;
  void dump_config() override;
  void update() override;

  // void setup() override;
  void set_spi_parent(spi::SPIComponent *parent);
  void set_cs_pin(GPIOPin *cs);
  void set_data_rate(uint32_t data_rate);
  void set_mode(spi::SPIMode mode);

  bool init_io();
  void uninit_io() { this->io_driver_->storage_uninit(); };

  /**
   * @brief   Reset and init againt io driver
   *
   * @param state
   * @return true
   * @return false
   */
  virtual bool reinit_driver(fatfs::StorageState state);

  /**
   * @brief   Used for check if storage media are inserted
   *
   * @return true
   * @return false
   */
  virtual bool is_card();

  /**
   * @brief Print out directory list through debug channel
   * @param path  path to directory
   *
   * @return true
   * @return false
   */
  bool test_fs(std::string path);

 private:
  SdspiIO *io_driver_{NULL};
};

}  // namespace fatfs_esp32_sdspi
}  // namespace esphome
