
#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/gpio.h"
#include "esphome/core/log.h"
#include "esphome/components/spi/spi.h"
#include "sdspi_defines.h"

namespace esphome {
namespace fatfs_esp32_sdspi {

/** SD is in idle state */
static const uint8_t IDLE_STATE = 0;
/** SD is in multi-sector read state. */
static const uint8_t READ_STATE = 1;
/** SD is in multi-sector write state. */
static const uint8_t WRITE_STATE = 2;

class SDSPIDriver : public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                          spi::DATA_RATE_20MHZ> {
 public:
  /***********************************************************************************
   * @brief Init SPI, tune spi interconnection
   *
   * @return true
   * @return false
   */
  bool init();

  void uninit();

  /***********************************************************************************
   * @brief Start spi transaction Down CS pin
   *
   * @return true
   * @return false
   */
  bool spi_start();

  /***********************************************************************************
   * @brief  Stop spi interaction. Up CS pin
   *
   */
  void spi_stop();

  /***********************************************************************************
   * @brief  Return if sdcard redy for io
   *
   * @return true
   * @return false
   */
  bool is_busy();

  /***********************************************************************************
   * @brief Check if card ready
   *
   * @return true
   * @return false
   */
  bool is_ready();

  /***********************************************************************************
   * @brief Send sdcard command (single token)
   *
   * @param cmd
   * @param arg
   * @return uint8_t
   */
  uint8_t spi_command(uint8_t cmd, uint32_t arg);

  /***********************************************************************************
   * @brief  Send sdcard command after app token
   *
   * @param cmd
   * @param arg
   * @return uint8_t
   */
  uint8_t spi_app_command(uint8_t cmd, uint32_t arg);

  /***********************************************************************************
   * @brief Stop reading and writing
   *
   * @return true
   * @return false
   */
  bool reset_io();

  /***********************************************************************************
   * @brief Wait time in ms
   *
   * @param ms
   * @return true
   * @return false
   */
  bool wait(uint16_t ms);

  /***********************************************************************************
   * @brief Stop writing
   *
   * @return true
   * @return false
   */
  bool write_stop();

  /***********************************************************************************
   *
   * @brief  Return the card type: SD V1, SD V2 or SDHC/SDXC
   * @return 0 - SD V1, 1 - SD V2, or 3 - SDHC/SDXC.
   */
  uint8_t get_type() const { return card_type_; }

  /***********************************************************************************
   * @brief Prepare for write  sectors
   *
   * @param sector
   * @return true
   * @return false
   */
  bool write_start(uint32_t sector);

  /***********************************************************************************
   * @brief Write 512 data block with token
   *
   * @param token
   * @param src
   * @return true
   * @return false
   */
  bool write_data(uint8_t token, const uint8_t *src);

  /***********************************************************************************
   * @brief Write 512 data block
   *
   * @param src
   * @return true
   * @return false
   */
  bool write_data(const uint8_t *src);

  /***********************************************************************************
   * @brief Write multiple 512 byte sectors to an SD card.
   *
   * @param sector Logical sector to be written.
   * @param src Pointer to the location of the data to be written.
   * @param ns Number of sectors to be written.
   * @return true
   * @return false
   */

  bool write_sectors(uint32_t sector, const uint8_t *src, size_t ns);

  /***********************************************************************************
   * @brief Write single sector
   *
   * @param sector
   * @param src
   * @return true
   * @return false
   */
  bool write_sector(uint32_t sector, const uint8_t *src);

  /***********************************************************************************
   * @brief Prepare to read sectors
   *
   * @param sector
   * @return true
   * @return false
   */
  bool read_start(uint32_t sector);

  /***********************************************************************************
   * @brief Stop reading multy sectors
   *
   * @return true
   * @return false
   */
  bool read_stop();

  /***********************************************************************************
   * @brief Read byte
   *
   * @param dst
   * @return uint8_t
   */
  uint8_t read_data(uint8_t *dst);

  /***********************************************************************************
   * @brief Read data block
   *
   * @param dst
   * @param count
   * @return uint8_t
   */
  uint8_t read_data(uint8_t *dst, size_t count);

  /***********************************************************************************
   * @brief Read single sector
   *
   * @param sector
   * @param dst
   * @return true
   * @return false
   */
  bool read_sector(uint32_t sector, uint8_t *dst);

  /***********************************************************************************
   * @brief Read multiple 512 byte sectors from an SD card.
   *
   * @param sector Logical sector to be read.
   * @param dst Pointer to the location that will receive the data.
   * @param ns Number of sectors to be read.
   * @return true
   * @return false
   */
  bool read_sectors(uint32_t sector, uint8_t *dst, size_t ns);

  /***********************************************************************************
   * @brief Read 64 bytes status block
   *
   * @param status
   * @return true
   * @return false
   */
  bool read_status(uint8_t *status);

  /***********************************************************************************
   * @brief Read 64bit register
   *
   * @param cmd
   * @param buf
   * @return true
   * @return false
   */
  bool read_register(uint8_t cmd, void *buf);

  /***********************************************************************************
   *
   * @brief Return sectors num
   *
   * @return uint32_t
   */
  uint32_t sector_count();

  /***********************************************************************************
   * @brief
   *
   * @return uint32_t
   */
  uint32_t sector_size();

  /***********************************************************************************
   * @brief Read a card's CID register. The CID contains card identification
   *  information such as Manufacturer ID, Product name, Product serial
   *  number and Manufacturing date.
   * @param cid cid pointer to area for returned data
   * @return true
   * @return false
   */
  bool read_cid(cid_t *cid) { return this->read_register(CMD10, cid); };

  /***********************************************************************************
   * @brief Read a card's CSD register. The CSD contains Card-Specific Data that
   * provides information regarding access to the card's contents.
   * @param csd
   * @return true
   * @return false
   */
  bool read_csd(csd_t *csd) { return this->read_register(CMD9, csd); };

  /***********************************************************************************
   *
   * @brief Erase a subsequent sectors.
   *
   * @param first_sector
   * @param last_sector
   * @return true
   * @return false
   */
  bool erase(uint32_t first_sector, uint32_t last_sector);

  /***********************************************************************************
   * @brief  is erase sectors enabled
   *
   * @return true
   * @return false
   */
  bool erase_sector_enable();

  /***********************************************************************************
   * @brief
   *
   * @param cmd
   * @param buff
   * @return uint8_t
   */
  uint8_t ioctl(uint8_t cmd, void *buff);

  // uint8_t connection_state();
  /**
   * @brief
   *
   * @return uint8_t
   */
  uint8_t drv_state() { return card_con_state_; };

  /**
   * @brief   Return error of last i/o operation
   *
   * @return uint8_t
   */
  uint8_t error() { return last_err_; };

 private:
  void set_type_(uint8_t c_type) { card_type_ = c_type; }
  uint8_t state_ = 0;  // IO transaction state  read; write; idle
  bool is_active_ = false;
  uint8_t last_err_ = 0;
  uint8_t response_;
  uint8_t card_type_;
  uint8_t card_con_state_ = STA_NOINIT;
};

}  // namespace fatfs_esp32_sdspi
}  // namespace esphome
