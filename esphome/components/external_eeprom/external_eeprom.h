#pragma once
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace external_eeprom {

/// @brief This Class provides the methods to read and write data from an 24 LC/AT XX devices such as 24LC32. See
/// https://ww1.microchip.com/downloads/en/devicedoc/doc0336.pdf

class ExtEepromComponent : public i2c::I2CDevice, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  bool is_connected(uint8_t i2c_address = 255);
  bool is_busy(uint8_t i2c_address = 255);

  uint8_t read8(uint32_t memaddr);  // Read a single byte from address memaddr
  uint16_t read16(uint32_t memaddr);
  uint32_t read32(uint32_t memaddr);
  float read_float(uint32_t memaddr);
  double read_double(uint32_t memaddr);
  void read(uint32_t memaddr, uint8_t *buff,
            uint16_t buffer_size);  // Read a buffer of buffersize (bytes) from adress memaddr
  uint32_t read_string_from_eeprom(uint32_t memaddr,
                                   std::string &str_to_read);  // Read a Std::string from adress memaddr

  void write8(uint32_t memaddr, uint8_t data_to_write);  // Write a single byte to address memaddr
  void write16(uint32_t memaddr, uint16_t value);
  void write32(uint32_t memaddr, uint32_t value);
  void write_float(uint32_t memaddr, float value);
  void write_double(uint32_t memaddr, double value);
  void write(uint32_t memaddr, uint8_t *data_to_write,
             uint16_t buffer_size);  // Write a buffer of buffersize (bytes) to adress memaddr
  uint32_t write_string_to_eeprom(uint32_t memaddr, std::string &str_to_write);  // Write a string to adress memaddr

  void dump_eeprom(uint32_t start_addr, uint16_t word_count);  // Display the contents of EEPROM in hex to the debug log
  void erase(uint8_t value_to_write = 0x00);  // Erase the entire memory. Optional: write a given byte to each spot.

  // Getters and Setters for component config
  void set_memory_size(uint32_t mem_size);            // Set the size of memory in bytes
  uint32_t get_memory_size();                         // Return size of memory in bytes
  void set_page_size(uint8_t page_size);              // Set the size of the page we can write a page at a time
  uint8_t get_page_size();                            // Get the size of the page we can read a page at a time
  void set_page_write_time(uint8_t write_time_ms);    // Set the number of ms required per page write
  uint8_t get_page_write_time();                      // Get the number of ms required per page write
  void set_i2c_buffer_size(uint8_t i2c_buffer_size);  // Set the size of hw buffer -2 for control & addr
  uint8_t get_i2c_buffer_size();                      // Get the size of hw buffer -2 for control & addr
  // Functionality to 'get' and 'put' objects to and from EEPROM.
  template<typename T> T &read_object(uint32_t idx, T &t) {
    uint8_t *ptr = (uint8_t *) &t;
    read(idx, ptr, sizeof(T));  // Address, data, sizeOfData
    return t;
  }

  template<typename T>
  T &write_object(uint32_t idx, T &t)  // Address, data
  {
    uint8_t *ptr = (uint8_t *) &t;
    write(idx, ptr, sizeof(T));  // Address, data, sizeOfData
    return t;
  }

 private:
  void write_block_(uint8_t deviceaddr, uint32_t memaddr, const uint8_t *obj, uint8_t size);
  uint32_t memory_size_bytes_{0};
  uint8_t memory_page_size_bytes_{0};
  uint8_t memory_page_write_time_ms_{0};
  bool poll_for_write_complete_{true};
  uint8_t i2c_buffer_size_{126};
};

}  // namespace external_eeprom
}  // namespace esphome
