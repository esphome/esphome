#include "lc709203f.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lc709203f {

static const char *const TAG = "lc709203f.sensor";

// Device I2C address. This address is fixed.
static const uint8_t LC709203F_I2C_ADDR_DEFAULT = 0x0B;

// Device registers
static const uint8_t LC709203F_BEFORE_RSOC = 0x04;
static const uint8_t LC709203F_THERMISTOR_B = 0x06;
static const uint8_t LC709203F_INITIAL_RSOC = 0x07;
static const uint8_t LC709203F_CELL_TEMPERATURE = 0x08;
static const uint8_t LC709203F_CELL_VOLTAGE = 0x09;
static const uint8_t LC709203F_CURRENT_DIRECTION = 0x0A;
static const uint8_t LC709203F_APA = 0x0B;
static const uint8_t LC709203F_APT = 0x0C;
static const uint8_t LC709203F_RSOC = 0x0D;
static const uint8_t LC709203F_ITE = 0x0F;
static const uint8_t LC709203F_IC_VERSION = 0x11;
static const uint8_t LC709203F_CHANGE_OF_THE_PARAMETER = 0x12;
static const uint8_t LC709203F_ALARM_LOW_RSOC = 0x13;
static const uint8_t LC709203F_ALARM_LOW_CELL_VOLTAGE = 0x14;
static const uint8_t LC709203F_IC_POWER_MODE = 0x15;
static const uint8_t LC709203F_STATUS_BIT = 0x16;
static const uint8_t LC709203F_NUMBER_OF_THE_PARAMETER = 0x1A;

static const uint8_t LC709203F_POWER_MODE_ON = 0x0001;
static const uint8_t LC709203F_POWER_MODE_SLEEP = 0x0002;

// The number of times to retry an I2C transaction before giving up. In my experience,
//  10 is a good number here that will take care of most bus issues that require retry.
static const uint8_t LC709203F_I2C_RETRY_COUNT = 10;

void Lc709203f::setup() {
  // Note: The setup implements a small state machine. This is because we want to have
  //  delays before and after sending the RSOC command. The full init process should be:
  //       INIT->RSOC->TEMP_SETUP->NORMAL
  //  The setup() function will only perform the first part of the initialization process.
  //  Assuming no errors, the whole process should occur during the setup() function and
  //  the first two calls to update(). After that, the part should remain in normal mode
  //  until a device reset.
  //
  //  This device can be picky about I2C communication and can error out occasionally. The
  //  get/set register functions impelment retry logic to retry the I2C transactions. The
  //  initialization code checks the return code from those functions. If they don't return
  //  NO_ERROR (0x00), that part of the initialization aborts and will be retried on the next
  //  call to update().
  // Set power mode to on. Note that, unlike some other similar devices, in sleep mode the IC
  //  does not record power usage. If there is significant power consumption during sleep mode,
  //  the pack RSOC will likely no longer be correct. Because of that, I do not implement
  //  sleep mode on this device.

  // Initialize device registers. If any of these fail, retry during the update() function.
  if (this->set_register_(LC709203F_IC_POWER_MODE, LC709203F_POWER_MODE_ON) != i2c::NO_ERROR) {
    return;
  }

  if (this->set_register_(LC709203F_APA, this->apa_) != i2c::NO_ERROR) {
    return;
  }

  if (this->set_register_(LC709203F_CHANGE_OF_THE_PARAMETER, this->pack_voltage_) != i2c::NO_ERROR) {
    return;
  }

  this->state_ = STATE_RSOC;
  // Note: Initialization continues in the update() function.
}

void Lc709203f::update() {
  uint16_t buffer;

  if (this->state_ == STATE_NORMAL) {
    // Note: If we fail to read from the data registers, we do not report any sensor reading.
    if (this->voltage_sensor_ != nullptr) {
      if (this->get_register_(LC709203F_CELL_VOLTAGE, &buffer) == i2c::NO_ERROR) {
        // Raw units are mV
        this->voltage_sensor_->publish_state(static_cast<float>(buffer) / 1000.0f);
        this->status_clear_warning();
      }
    }
    if (this->battery_remaining_sensor_ != nullptr) {
      if (this->get_register_(LC709203F_ITE, &buffer) == i2c::NO_ERROR) {
        // Raw units are .1%
        this->battery_remaining_sensor_->publish_state(static_cast<float>(buffer) / 10.0f);
        this->status_clear_warning();
      }
    }
    if (this->temperature_sensor_ != nullptr) {
      // I can't test this with a real thermistor because I don't have a device with
      //  an attached thermistor. I have turned on the sensor and made sure that it
      //  sets up the registers properly.
      if (this->get_register_(LC709203F_CELL_TEMPERATURE, &buffer) == i2c::NO_ERROR) {
        // Raw units are .1 K
        this->temperature_sensor_->publish_state((static_cast<float>(buffer) / 10.0f) - 273.15f);
        this->status_clear_warning();
      }
    }
  } else if (this->state_ == STATE_INIT) {
    // Retry initializing the device registers. We should only get here if the init sequence
    //  failed during the setup() function. This would likely occur because of a repeated failures
    //  on the I2C bus. If any of these fail, retry the next time the update() function is called.
    if (this->set_register_(LC709203F_IC_POWER_MODE, LC709203F_POWER_MODE_ON) != i2c::NO_ERROR) {
      return;
    }

    if (this->set_register_(LC709203F_APA, this->apa_) != i2c::NO_ERROR) {
      return;
    }

    if (this->set_register_(LC709203F_CHANGE_OF_THE_PARAMETER, this->pack_voltage_) != i2c::NO_ERROR) {
      return;
    }

    this->state_ = STATE_RSOC;

  } else if (this->state_ == STATE_RSOC) {
    // We implement a delay here to send the initial RSOC command.
    //  This should run once on the first update() after initialization.
    if (this->set_register_(LC709203F_INITIAL_RSOC, 0xAA55) == i2c::NO_ERROR) {
      this->state_ = STATE_TEMP_SETUP;
    }
  } else if (this->state_ == STATE_TEMP_SETUP) {
    // This should run once on the second update() after initialization.
    if (this->temperature_sensor_ != nullptr) {
      // This assumes that a thermistor is attached to the device as shown in the datahseet.
      if (this->set_register_(LC709203F_STATUS_BIT, 0x0001) == i2c::NO_ERROR) {
        if (this->set_register_(LC709203F_THERMISTOR_B, this->b_constant_) == i2c::NO_ERROR) {
          this->state_ = STATE_NORMAL;
        }
      }
    } else if (this->set_register_(LC709203F_STATUS_BIT, 0x0000) == i2c::NO_ERROR) {
      // The device expects to get updates to the temperature in this mode.
      //  I am not doing that now. The temperature register defaults to 25C.
      //  In theory, we could have another temperature sensor and have ESPHome
      //  send updated temperature to the device occasionally, but I have no idea
      //  how to make that happen.
      this->state_ = STATE_NORMAL;
    }
  }
}

void Lc709203f::dump_config() {
  ESP_LOGCONFIG(TAG, "LC709203F:");
  LOG_I2C_DEVICE(this);

  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG,
                "  Pack Size: %d mAH\n"
                "  Pack APA: 0x%02X",
                this->pack_size_, this->apa_);

  // This is only true if the pack_voltage_ is either 0x0000 or 0x0001. The config validator
  //  should have already verified this.
  ESP_LOGCONFIG(TAG, "  Pack Rated Voltage: 3.%sV", this->pack_voltage_ == 0x0000 ? "8" : "7");

  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Battery Remaining", this->battery_remaining_sensor_);

  if (this->temperature_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
    ESP_LOGCONFIG(TAG, "    B_Constant: %d", this->b_constant_);
  } else {
    ESP_LOGCONFIG(TAG, "  No Temperature Sensor");
  }
}

uint8_t Lc709203f::get_register_(uint8_t register_to_read, uint16_t *register_value) {
  i2c::ErrorCode return_code;
  uint8_t read_buffer[6];

  read_buffer[0] = (this->address_) << 1;
  read_buffer[1] = register_to_read;
  read_buffer[2] = ((this->address_) << 1) | 0x01;

  for (uint8_t i = 0; i <= LC709203F_I2C_RETRY_COUNT; i++) {
    // Note: the read_register() function does not send a stop between the write and
    //  the read portions of the I2C transation when you set the last variable to 'false'
    //  as we do below. Some of the other I2C read functions such as the generic read()
    //  function will send a stop between the read and the write portion of the I2C
    //  transaction. This is bad in this case and will result in reading nothing but 0xFFFF
    //  from the registers.
    return_code = this->read_register(register_to_read, &read_buffer[3], 3);
    if (return_code != i2c::NO_ERROR) {
      // Error on the i2c bus
      this->status_set_warning(
          str_sprintf("Error code %d when reading from register 0x%02X", return_code, register_to_read).c_str());
    } else if (crc8(read_buffer, 5, 0x00, 0x07, true) != read_buffer[5]) {
      // I2C indicated OK, but the CRC of the data does not matcth.
      this->status_set_warning(str_sprintf("CRC error reading from register 0x%02X", register_to_read).c_str());
    } else {
      *register_value = ((uint16_t) read_buffer[4] << 8) | (uint16_t) read_buffer[3];
      return i2c::NO_ERROR;
    }
  }

  // If we get here, we tried LC709203F_I2C_RETRY_COUNT times to read the register and
  //  failed each time. Set the register value to 0 and return the I2C error code or 0xFF
  //  to indicate a CRC failure. It will be up to the higher level code what to do when
  //  this happens.
  *register_value = 0x0000;
  if (return_code != i2c::NO_ERROR) {
    return return_code;
  } else {
    return 0xFF;
  }
}

uint8_t Lc709203f::set_register_(uint8_t register_to_set, uint16_t value_to_set) {
  i2c::ErrorCode return_code;
  uint8_t write_buffer[5];

  // Note: We don't actually send byte[0] of the buffer. We include it because it is
  //  part of the CRC calculation.
  write_buffer[0] = (this->address_) << 1;
  write_buffer[1] = register_to_set;
  write_buffer[2] = value_to_set & 0xFF;         // Low byte
  write_buffer[3] = (value_to_set >> 8) & 0xFF;  // High byte
  write_buffer[4] = crc8(write_buffer, 4, 0x00, 0x07, true);

  for (uint8_t i = 0; i <= LC709203F_I2C_RETRY_COUNT; i++) {
    // Note: we don't write the first byte of the write buffer to the device.
    //  This is done automatically by the write() function.
    return_code = this->write(&write_buffer[1], 4);
    if (return_code == i2c::NO_ERROR) {
      return return_code;
    } else {
      this->status_set_warning(
          str_sprintf("Error code %d when writing to register 0x%02X", return_code, register_to_set).c_str());
    }
  }

  // If we get here, we tried to send the data LC709203F_I2C_RETRY_COUNT times and failed.
  //  We return the I2C error code, it is up to the higher level code what to do about it.
  return return_code;
}

void Lc709203f::set_pack_size(uint16_t pack_size) {
  static const uint16_t PACK_SIZE_ARRAY[6] = {100, 200, 500, 1000, 2000, 3000};
  static const uint16_t APA_ARRAY[6] = {0x08, 0x0B, 0x10, 0x19, 0x2D, 0x36};
  float slope;
  float intercept;

  this->pack_size_ = pack_size;  // Pack size in mAH

  // The size is used to calculate the 'Adjustment Pack Application' number.
  // Here we assume a type 01 or type 03 battery and do a linear curve fit to find the APA.
  for (uint8_t i = 0; i < 6; i++) {
    if (PACK_SIZE_ARRAY[i] == pack_size) {
      // If the pack size is exactly one of the values in the array.
      this->apa_ = APA_ARRAY[i];
      return;
    } else if ((i > 0) && (PACK_SIZE_ARRAY[i] > pack_size) && (PACK_SIZE_ARRAY[i - 1] < pack_size)) {
      // If the pack size is between the current array element and the previous. Do a linear
      //  Curve fit to determine the APA value.

      // Type casting is required here to avoid interger division
      slope = static_cast<float>(APA_ARRAY[i] - APA_ARRAY[i - 1]) /
              static_cast<float>(PACK_SIZE_ARRAY[i] - PACK_SIZE_ARRAY[i - 1]);

      // Type casting might not be needed here.
      intercept = static_cast<float>(APA_ARRAY[i]) - slope * static_cast<float>(PACK_SIZE_ARRAY[i]);

      this->apa_ = static_cast<uint8_t>(slope * pack_size + intercept);
      return;
    }
  }
  // We should never get here. If we do, it means we never set the pack APA. This should
  //  not be possible because of the config validation. However, if it does happen, the
  //  consequence is that the RSOC values will likley not be as accurate. However, it should
  //  not cause an error or crash, so I am not doing any additional checking here.
}

void Lc709203f::set_thermistor_b_constant(uint16_t b_constant) { this->b_constant_ = b_constant; }

void Lc709203f::set_pack_voltage(LC709203FBatteryVoltage pack_voltage) { this->pack_voltage_ = pack_voltage; }

}  // namespace lc709203f
}  // namespace esphome
