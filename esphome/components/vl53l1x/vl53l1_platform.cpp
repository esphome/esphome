
/**
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

  /** Platform Bridge functions used by the STMicroelectronics Ultra Light Driver */

  #include "vl53l1_platform.h"
  #include "VL53L1X_api.h"
  #include "esphome/core/hal.h"
  #include "esphome/core/log.h"
  #include "esphome/components/i2c/i2c.h"
  #include "esphome/core/helpers.h"
  
  #include <map>

  namespace esphome {
    namespace vl53l1x {
      // Until the proper I2C-address is setup we use the bootstrap I2CDevice 
      // for communication, this address is always 0x29.  
      static ::esphome::i2c::I2CDevice* bootstrap_device{nullptr};

      /** Set the bootstrap device, should be cleared afterwards. */
      void set_bootstrap_device(::esphome::i2c::I2CDevice* dev) {
        bootstrap_device = dev;
      }
      
      /** Clears the bootstrap device. */
      void clear_bootstrap_device() {
        bootstrap_device = nullptr;
      }

      // The ST-Electronic Ultra Light Driver only forwards the device address to differentiate 
      // between devices, therefore we instantiate a map to the proper I2CDevice as part of setup. 
      static std::map<uint16_t, ::esphome::i2c::I2CDevice*> devices;

      /** Register the sensor I2CDevice so it is accessible to the driver. */
      void register_sensor(::esphome::i2c::I2CDevice* dev) {
          devices[dev->get_i2c_address()] = dev;
      }

      /** Return the I2CDevice associated with an address. If bootstrap-device is set, 
       *  and address is 0x29, the bootstrap device is returned instead. */
      ::esphome::i2c::I2CDevice* lookup(uint16_t devAddr) {
        if (devAddr == 0x29 && bootstrap_device) {
          return bootstrap_device;
        }
        return devices.at(devAddr);
      }

    int8_t VL53L1_WriteMulti( uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count) {
      /* Warning : For big endian platforms, fields 'RegisterAdress' and 'value' need to be swapped. */

      uint8_t status = VL53L1X_ERROR_TIMEOUT; 
      uint8_t err = 0;
      if((err = lookup(dev)->write_register16(index, pdata, count)) == 0) {
        status = VL53L1X_ERROR_NONE;
      }
      return status;
    }
  
  int8_t VL53L1_ReadMulti(uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count){
      uint8_t status = VL53L1X_ERROR_TIMEOUT;
      uint8_t err = 0;
      if((err = lookup(dev)->read_register16(index, pdata, count)) == 0) {
        status = VL53L1X_ERROR_NONE;
      }
    
      return status;
  }
  
  int8_t VL53L1_WrByte(uint16_t dev, uint16_t index, uint8_t data) {
      return VL53L1_WriteMulti(dev, index, reinterpret_cast<uint8_t*>(&data), sizeof(data));
  }
  
  int8_t VL53L1_WrWord(uint16_t dev, uint16_t index, uint16_t data) {
    data = byteswap(data);
    return VL53L1_WriteMulti(dev, index, reinterpret_cast<uint8_t*>(&data), sizeof(data));
  }
  
  int8_t VL53L1_WrDWord(uint16_t dev, uint16_t index, uint32_t data) {
    data = byteswap(data);
    return VL53L1_WriteMulti(dev, index, reinterpret_cast<uint8_t*>(&data), sizeof(data));
  }
  
  int8_t VL53L1_RdByte(uint16_t dev, uint16_t index, uint8_t *data) {
    return VL53L1_ReadMulti(dev, index, reinterpret_cast<uint8_t*>(data), sizeof(*data));
  }
  
  int8_t VL53L1_RdWord(uint16_t dev, uint16_t index, uint16_t *data) {
    int8_t status = VL53L1X_ERROR_NONE;
    uint16_t tmp_data = 0; 
    if ((status = VL53L1_ReadMulti(dev, index, reinterpret_cast<uint8_t*>(&tmp_data), sizeof(tmp_data))) == VL53L1X_ERROR_NONE) {
      *data = byteswap(tmp_data);
    }
    return status;
  }
  
  int8_t VL53L1_RdDWord(uint16_t dev, uint16_t index, uint32_t *data) {
    int8_t status = VL53L1X_ERROR_NONE;
    uint32_t tmp_data = 0; 
    if ((status = VL53L1_ReadMulti(dev, index, reinterpret_cast<uint8_t*>(&tmp_data), sizeof(tmp_data))) == VL53L1X_ERROR_NONE) {
      *data = byteswap(tmp_data);
    }
    return status;
  }
  
  int8_t VL53L1_WaitMs(uint16_t dev, int32_t wait_ms){
      ::esphome::delay(wait_ms);
      return VL53L1X_ERROR_NONE;
  }
  


} // namespace vl53l1
} // namespace esphome
