#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace rc522 {

class RC522BinarySensor;
class RC522Trigger;
class RC522 : public PollingComponent {
 public:
  void setup() override;

  void dump_config() override;

  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; };

  void loop() override;

  void register_tag(RC522BinarySensor *tag) { this->binary_sensors_.push_back(tag); }
  void register_trigger(RC522Trigger *trig) { this->triggers_.push_back(trig); }

  void set_reset_pin(GPIOPin *reset) { this->reset_pin_ = reset; }

 protected:
  // Return codes from the functions in this class. Remember to update GetStatusCodeName() if you add more.
  // last value set to 0xff, then compiler uses less ram, it seems some optimisations are triggered
  enum StatusCode : uint8_t {
    STATUS_OK,                 // Success
    STATUS_WAITING,            // Waiting result from RC522 chip
    STATUS_ERROR,              // Error in communication
    STATUS_COLLISION,          // Collision detected
    STATUS_TIMEOUT,            // Timeout in communication.
    STATUS_NO_ROOM,            // A buffer is not big enough.
    STATUS_INTERNAL_ERROR,     // Internal error in the code. Should not happen ;-)
    STATUS_INVALID,            // Invalid argument.
    STATUS_CRC_WRONG,          // The CRC_A does not match
    STATUS_MIFARE_NACK = 0xff  // A MIFARE PICC responded with NAK.
  };

  enum State {
    STATE_NONE = 0,
    STATE_SETUP,
    STATE_INIT,
    STATE_PICC_REQUEST_A,
    STATE_READ_SERIAL,
    STATE_SELECT_SERIAL,
    STATE_SELECT_SERIAL_DONE,
    STATE_READ_SERIAL_DONE,
    STATE_DONE,
  } state_{STATE_NONE};

  enum PcdRegister : uint8_t {
    // Page 0: Command and status
    // 0x00      // reserved for future use
    COMMAND_REG = 0x01 << 1,      // starts and stops command execution
    COM_I_EN_REG = 0x02 << 1,     // enable and disable interrupt request control bits
    DIV_I_EN_REG = 0x03 << 1,     // enable and disable interrupt request control bits
    COM_IRQ_REG = 0x04 << 1,      // interrupt request bits
    DIV_IRQ_REG = 0x05 << 1,      // interrupt request bits
    ERROR_REG = 0x06 << 1,        // error bits showing the error status of the last command executed
    STATUS1_REG = 0x07 << 1,      // communication status bits
    STATUS2_REG = 0x08 << 1,      // receiver and transmitter status bits
    FIFO_DATA_REG = 0x09 << 1,    // input and output of 64 uint8_t FIFO buffer
    FIFO_LEVEL_REG = 0x0A << 1,   // number of uint8_ts stored in the FIFO buffer
    WATER_LEVEL_REG = 0x0B << 1,  // level for FIFO underflow and overflow warning
    CONTROL_REG = 0x0C << 1,      // miscellaneous control registers
    BIT_FRAMING_REG = 0x0D << 1,  // adjustments for bit-oriented frames
    COLL_REG = 0x0E << 1,         // bit position of the first bit-collision detected on the RF interface
    //                 0x0F     // reserved for future use

    // Page 1: Command
    //               0x10      // reserved for future use
    MODE_REG = 0x11 << 1,          // defines general modes for transmitting and receiving
    TX_MODE_REG = 0x12 << 1,       // defines transmission data rate and framing
    RX_MODE_REG = 0x13 << 1,       // defines reception data rate and framing
    TX_CONTROL_REG = 0x14 << 1,    // controls the logical behavior of the antenna driver pins TX1 and TX2
    TX_ASK_REG = 0x15 << 1,        // controls the setting of the transmission modulation
    TX_SEL_REG = 0x16 << 1,        // selects the internal sources for the antenna driver
    RX_SEL_REG = 0x17 << 1,        // selects internal receiver settings
    RX_THRESHOLD_REG = 0x18 << 1,  // selects thresholds for the bit decoder
    DEMOD_REG = 0x19 << 1,         // defines demodulator settings
    //               0x1A      // reserved for future use
    //               0x1B      // reserved for future use
    MF_TX_REG = 0x1C << 1,  // controls some MIFARE communication transmit parameters
    MF_RX_REG = 0x1D << 1,  // controls some MIFARE communication receive parameters
    //               0x1E      // reserved for future use
    SERIAL_SPEED_REG = 0x1F << 1,  // selects the speed of the serial UART interface

    // Page 2: Configuration
    //               0x20      // reserved for future use
    CRC_RESULT_REG_H = 0x21 << 1,  // shows the MSB and LSB values of the CRC calculation
    CRC_RESULT_REG_L = 0x22 << 1,
    //               0x23      // reserved for future use
    MOD_WIDTH_REG = 0x24 << 1,  // controls the ModWidth setting?
    //               0x25      // reserved for future use
    RF_CFG_REG = 0x26 << 1,       // configures the receiver gain
    GS_N_REG = 0x27 << 1,         // selects the conductance of the antenna driver pins TX1 and TX2 for modulation
    CW_GS_P_REG = 0x28 << 1,      // defines the conductance of the p-driver output during periods of no modulation
    MOD_GS_P_REG = 0x29 << 1,     // defines the conductance of the p-driver output during periods of modulation
    T_MODE_REG = 0x2A << 1,       // defines settings for the internal timer
    T_PRESCALER_REG = 0x2B << 1,  // the lower 8 bits of the TPrescaler value. The 4 high bits are in TModeReg.
    T_RELOAD_REG_H = 0x2C << 1,   // defines the 16-bit timer reload value
    T_RELOAD_REG_L = 0x2D << 1,
    T_COUNTER_VALUE_REG_H = 0x2E << 1,  // shows the 16-bit timer value
    T_COUNTER_VALUE_REG_L = 0x2F << 1,

    // Page 3: Test Registers
    //               0x30      // reserved for future use
    TEST_SEL1_REG = 0x31 << 1,       // general test signal configuration
    TEST_SEL2_REG = 0x32 << 1,       // general test signal configuration
    TEST_PIN_EN_REG = 0x33 << 1,     // enables pin output driver on pins D1 to D7
    TEST_PIN_VALUE_REG = 0x34 << 1,  // defines the values for D1 to D7 when it is used as an I/O bus
    TEST_BUS_REG = 0x35 << 1,        // shows the status of the internal test bus
    AUTO_TEST_REG = 0x36 << 1,       // controls the digital self-test
    VERSION_REG = 0x37 << 1,         // shows the software version
    ANALOG_TEST_REG = 0x38 << 1,     // controls the pins AUX1 and AUX2
    TEST_DA_C1_REG = 0x39 << 1,      // defines the test value for TestDAC1
    TEST_DA_C2_REG = 0x3A << 1,      // defines the test value for TestDAC2
    TEST_ADC_REG = 0x3B << 1         // shows the value of ADC I and Q channels
                                     //               0x3C      // reserved for production tests
                                     //               0x3D      // reserved for production tests
                                     //               0x3E      // reserved for production tests
                                     //               0x3F      // reserved for production tests
  };

  // MFRC522 commands. Described in chapter 10 of the datasheet.
  enum PcdCommand : uint8_t {
    PCD_IDLE = 0x00,                // no action, cancels current command execution
    PCD_MEM = 0x01,                 // stores 25 uint8_ts into the internal buffer
    PCD_GENERATE_RANDOM_ID = 0x02,  // generates a 10-uint8_t random ID number
    PCD_CALC_CRC = 0x03,            // activates the CRC coprocessor or performs a self-test
    PCD_TRANSMIT = 0x04,            // transmits data from the FIFO buffer
    PCD_NO_CMD_CHANGE = 0x07,       // no command change, can be used to modify the CommandReg register bits without
                                    // affecting the command, for example, the PowerDown bit
    PCD_RECEIVE = 0x08,             // activates the receiver circuits
    PCD_TRANSCEIVE =
        0x0C,  // transmits data from FIFO buffer to antenna and automatically activates the receiver after transmission
    PCD_MF_AUTHENT = 0x0E,  // performs the MIFARE standard authentication as a reader
    PCD_SOFT_RESET = 0x0F   // resets the MFRC522
  };

  // Commands sent to the PICC.
  enum PiccCommand : uint8_t {
    // The commands used by the PCD to manage communication with several PICCs (ISO 14443-3, Type A, section 6.4)
    PICC_CMD_REQA = 0x26,     // REQuest command, Type A. Invites PICCs in state IDLE to go to READY and prepare for
                              // anticollision or selection. 7 bit frame.
    PICC_CMD_WUPA = 0x52,     // Wake-UP command, Type A. Invites PICCs in state IDLE and HALT to go to READY(*) and
                              // prepare for anticollision or selection. 7 bit frame.
    PICC_CMD_CT = 0x88,       // Cascade Tag. Not really a command, but used during anti collision.
    PICC_CMD_SEL_CL1 = 0x93,  // Anti collision/Select, Cascade Level 1
    PICC_CMD_SEL_CL2 = 0x95,  // Anti collision/Select, Cascade Level 2
    PICC_CMD_SEL_CL3 = 0x97,  // Anti collision/Select, Cascade Level 3
    PICC_CMD_HLTA = 0x50,     // HaLT command, Type A. Instructs an ACTIVE PICC to go to state HALT.
    PICC_CMD_RATS = 0xE0,     // Request command for Answer To Reset.
    // The commands used for MIFARE Classic (from http://www.mouser.com/ds/2/302/MF1S503x-89574.pdf, Section 9)
    // Use PCD_MFAuthent to authenticate access to a sector, then use these commands to read/write/modify the blocks on
    // the sector.
    // The read/write commands can also be used for MIFARE Ultralight.
    PICC_CMD_MF_AUTH_KEY_A = 0x60,  // Perform authentication with Key A
    PICC_CMD_MF_AUTH_KEY_B = 0x61,  // Perform authentication with Key B
    PICC_CMD_MF_READ =
        0x30,  // Reads one 16 uint8_t block from the authenticated sector of the PICC. Also used for MIFARE Ultralight.
    PICC_CMD_MF_WRITE = 0xA0,  // Writes one 16 uint8_t block to the authenticated sector of the PICC. Called
                               // "COMPATIBILITY WRITE" for MIFARE Ultralight.
    PICC_CMD_MF_DECREMENT =
        0xC0,  // Decrements the contents of a block and stores the result in the internal data register.
    PICC_CMD_MF_INCREMENT =
        0xC1,  // Increments the contents of a block and stores the result in the internal data register.
    PICC_CMD_MF_RESTORE = 0xC2,   // Reads the contents of a block into the internal data register.
    PICC_CMD_MF_TRANSFER = 0xB0,  // Writes the contents of the internal data register to a block.
    // The commands used for MIFARE Ultralight (from http://www.nxp.com/documents/data_sheet/MF0ICU1.pdf, Section 8.6)
    // The PICC_CMD_MF_READ and PICC_CMD_MF_WRITE can also be used for MIFARE Ultralight.
    PICC_CMD_UL_WRITE = 0xA2  // Writes one 4 uint8_t page to the PICC.
  };

  void pcd_reset_();
  void initialize_();
  void pcd_antenna_on_();
  void pcd_antenna_off_();

  virtual uint8_t pcd_read_register(PcdRegister reg  ///< The register to read from. One of the PCD_Register enums.
                                    ) = 0;

  /**
   * Reads a number of uint8_ts from the specified register in the MFRC522 chip.
   * The interface is described in the datasheet section 8.1.2.
   */
  virtual void pcd_read_register(PcdRegister reg,  ///< The register to read from. One of the PCD_Register enums.
                                 uint8_t count,    ///< The number of uint8_ts to read
                                 uint8_t *values,  ///< uint8_t array to store the values in.
                                 uint8_t rx_align  ///< Only bit positions rxAlign..7 in values[0] are updated.
                                 ) = 0;
  virtual void pcd_write_register(PcdRegister reg,  ///< The register to write to. One of the PCD_Register enums.
                                  uint8_t value     ///< The value to write.
                                  ) = 0;

  /**
   * Writes a number of uint8_ts to the specified register in the MFRC522 chip.
   * The interface is described in the datasheet section 8.1.2.
   */
  virtual void pcd_write_register(PcdRegister reg,  ///< The register to write to. One of the PCD_Register enums.
                                  uint8_t count,    ///< The number of uint8_ts to write to the register
                                  uint8_t *values   ///< The values to write. uint8_t array.
                                  ) = 0;

  void pcd_set_register_bit_mask_(PcdRegister reg,  ///< The register to update. One of the PCD_Register enums.
                                  uint8_t mask      ///< The bits to set.
  );
  void pcd_clear_register_bit_mask_(PcdRegister reg,  ///< The register to update. One of the PCD_Register enums.
                                    uint8_t mask      ///< The bits to clear.
  );

  void pcd_transceive_data_(uint8_t send_len);

  void pcd_calculate_crc_(uint8_t *data,  ///< In: Pointer to the data to transfer to the FIFO for CRC calculation.
                          uint8_t length  ///< In: The number of uint8_ts to transfer.
  );

  bool awaiting_comm_;
  uint32_t awaiting_comm_time_;
  StatusCode await_transceive_();
  StatusCode await_crc_();

  uint8_t buffer_[9];       ///< buffer for communication, the first bits [0..back_idx-1] are for tx ,
                            ///< [back_idx..back_idx+back_len] for rx
  uint8_t send_len_;        // index of first byte for RX
  uint8_t back_length_;     ///< In: Max number of uint8_ts to write to *backData. Out: The number of uint8_ts returned.
  uint8_t uid_buffer_[10];  // buffer to construct the uid (for 7 and 10 bit uids)
  uint8_t uid_idx_ = 0;     // number of read uid bytes e.g. index of the next available position in uid_buffer
  uint8_t error_counter_ = 0;  // to reset if unresponsive
  uint8_t rx_align_;
  uint8_t *valid_bits_;

  GPIOPin *reset_pin_{nullptr};
  uint8_t reset_count_{0};
  uint32_t reset_timeout_{0};
  std::vector<RC522BinarySensor *> binary_sensors_;
  std::vector<RC522Trigger *> triggers_;
  std::vector<uint8_t> current_uid_;

  enum RC522Error {
    NONE = 0,
    RESET_FAILED,
  } error_code_{NONE};
};

class RC522BinarySensor : public binary_sensor::BinarySensor {
 public:
  void set_uid(const std::vector<uint8_t> &uid) { uid_ = uid; }

  bool process(std::vector<uint8_t> &data);

  void on_scan_end() {
    if (!this->found_) {
      this->publish_state(false);
    }
    this->found_ = false;
  }

 protected:
  std::vector<uint8_t> uid_;
  bool found_{false};
};

class RC522Trigger : public Trigger<std::string> {
 public:
  void process(std::vector<uint8_t> &data);
};

}  // namespace rc522
}  // namespace esphome
