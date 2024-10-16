#pragma once

// Datasheet: https://www.ti.com/lit/ds/symlink/cc1101.pdf

namespace esphome {
namespace cc1101 {

//***************************************CC1101 define**************************************************//
// CC1101 CONFIG REGSITER
static constexpr uint32_t CC1101_IOCFG2 = 0x00;    // GDO2 output pin configuration
static constexpr uint32_t CC1101_IOCFG1 = 0x01;    // GDO1 output pin configuration
static constexpr uint32_t CC1101_IOCFG0 = 0x02;    // GDO0 output pin configuration
static constexpr uint32_t CC1101_FIFOTHR = 0x03;   // RX FIFO and TX FIFO thresholds
static constexpr uint32_t CC1101_SYNC1 = 0x04;     // Sync word, high INT8U
static constexpr uint32_t CC1101_SYNC0 = 0x05;     // Sync word, low INT8U
static constexpr uint32_t CC1101_PKTLEN = 0x06;    // Packet length
static constexpr uint32_t CC1101_PKTCTRL1 = 0x07;  // Packet automation control
static constexpr uint32_t CC1101_PKTCTRL0 = 0x08;  // Packet automation control
static constexpr uint32_t CC1101_ADDR = 0x09;      // Device address
static constexpr uint32_t CC1101_CHANNR = 0x0A;    // Channel number
static constexpr uint32_t CC1101_FSCTRL1 = 0x0B;   // Frequency synthesizer control
static constexpr uint32_t CC1101_FSCTRL0 = 0x0C;   // Frequency synthesizer control
static constexpr uint32_t CC1101_FREQ2 = 0x0D;     // Frequency control word, high INT8U
static constexpr uint32_t CC1101_FREQ1 = 0x0E;     // Frequency control word, middle INT8U
static constexpr uint32_t CC1101_FREQ0 = 0x0F;     // Frequency control word, low INT8U
static constexpr uint32_t CC1101_MDMCFG4 = 0x10;   // Modem configuration
static constexpr uint32_t CC1101_MDMCFG3 = 0x11;   // Modem configuration
static constexpr uint32_t CC1101_MDMCFG2 = 0x12;   // Modem configuration
static constexpr uint32_t CC1101_MDMCFG1 = 0x13;   // Modem configuration
static constexpr uint32_t CC1101_MDMCFG0 = 0x14;   // Modem configuration
static constexpr uint32_t CC1101_DEVIATN = 0x15;   // Modem deviation setting
static constexpr uint32_t CC1101_MCSM2 = 0x16;     // Main Radio Control State Machine configuration
static constexpr uint32_t CC1101_MCSM1 = 0x17;     // Main Radio Control State Machine configuration
static constexpr uint32_t CC1101_MCSM0 = 0x18;     // Main Radio Control State Machine configuration
static constexpr uint32_t CC1101_FOCCFG = 0x19;    // Frequency Offset Compensation configuration
static constexpr uint32_t CC1101_BSCFG = 0x1A;     // Bit Synchronization configuration
static constexpr uint32_t CC1101_AGCCTRL2 = 0x1B;  // AGC control
static constexpr uint32_t CC1101_AGCCTRL1 = 0x1C;  // AGC control
static constexpr uint32_t CC1101_AGCCTRL0 = 0x1D;  // AGC control
static constexpr uint32_t CC1101_WOREVT1 = 0x1E;   // High INT8U Event 0 timeout
static constexpr uint32_t CC1101_WOREVT0 = 0x1F;   // Low INT8U Event 0 timeout
static constexpr uint32_t CC1101_WORCTRL = 0x20;   // Wake On Radio control
static constexpr uint32_t CC1101_FREND1 = 0x21;    // Front end RX configuration
static constexpr uint32_t CC1101_FREND0 = 0x22;    // Front end TX configuration
static constexpr uint32_t CC1101_FSCAL3 = 0x23;    // Frequency synthesizer calibration
static constexpr uint32_t CC1101_FSCAL2 = 0x24;    // Frequency synthesizer calibration
static constexpr uint32_t CC1101_FSCAL1 = 0x25;    // Frequency synthesizer calibration
static constexpr uint32_t CC1101_FSCAL0 = 0x26;    // Frequency synthesizer calibration
static constexpr uint32_t CC1101_RCCTRL1 = 0x27;   // RC oscillator configuration
static constexpr uint32_t CC1101_RCCTRL0 = 0x28;   // RC oscillator configuration
static constexpr uint32_t CC1101_FSTEST = 0x29;    // Frequency synthesizer calibration control
static constexpr uint32_t CC1101_PTEST = 0x2A;     // Production test
static constexpr uint32_t CC1101_AGCTEST = 0x2B;   // AGC test
static constexpr uint32_t CC1101_TEST2 = 0x2C;     // Various test settings
static constexpr uint32_t CC1101_TEST1 = 0x2D;     // Various test settings
static constexpr uint32_t CC1101_TEST0 = 0x2E;     // Various test settings

// CC1101 Strobe commands
static constexpr uint32_t CC1101_SRES = 0x30;     // Reset chip.
static constexpr uint32_t CC1101_SFSTXON = 0x31;  // Enable and calibrate frequency synthesizer (if MCSM0.FS_AUTOCAL=1).
                                                  // If in RX/TX: Go to a wait state where only the synthesizer is
                                                  // running (for quick RX / TX turnaround).
static constexpr uint32_t CC1101_SXOFF = 0x32;    // Turn off crystal oscillator.
static constexpr uint32_t CC1101_SCAL = 0x33;     // Calibrate frequency synthesizer and turn it off
                                                  // (enables quick start).
static constexpr uint32_t CC1101_SRX = 0x34;      // Enable RX. Perform calibration first if coming from IDLE and
                                                  // MCSM0.FS_AUTOCAL=1.
static constexpr uint32_t CC1101_STX = 0x35;      // In IDLE state: Enable TX. Perform calibration first if
                                                  // MCSM0.FS_AUTOCAL=1. If in RX state and CCA is enabled:
                                                  // Only go to TX if channel is clear.
static constexpr uint32_t CC1101_SIDLE = 0x36;    // Exit RX / TX, turn off frequency synthesizer and exit
                                                  // Wake-On-Radio mode if applicable.
static constexpr uint32_t CC1101_SAFC = 0x37;     // Perform AFC adjustment of the frequency synthesizer
static constexpr uint32_t CC1101_SWOR = 0x38;     // Start automatic RX polling sequence (Wake-on-Radio)
static constexpr uint32_t CC1101_SPWD = 0x39;     // Enter power down mode when CSn goes high.
static constexpr uint32_t CC1101_SFRX = 0x3A;     // Flush the RX FIFO buffer.
static constexpr uint32_t CC1101_SFTX = 0x3B;     // Flush the TX FIFO buffer.
static constexpr uint32_t CC1101_SWORRST = 0x3C;  // Reset real time clock.
static constexpr uint32_t CC1101_SNOP = 0x3D;     // No operation. May be used to pad strobe commands to two
                                                  // INT8Us for simpler software.
// CC1101 STATUS REGSITER
static constexpr uint32_t CC1101_PARTNUM = 0x30;
static constexpr uint32_t CC1101_VERSION = 0x31;
static constexpr uint32_t CC1101_FREQEST = 0x32;
static constexpr uint32_t CC1101_LQI = 0x33;
static constexpr uint32_t CC1101_RSSI = 0x34;
static constexpr uint32_t CC1101_MARCSTATE = 0x35;
static constexpr uint32_t CC1101_WORTIME1 = 0x36;
static constexpr uint32_t CC1101_WORTIME0 = 0x37;
static constexpr uint32_t CC1101_PKTSTATUS = 0x38;
static constexpr uint32_t CC1101_VCO_VC_DAC = 0x39;
static constexpr uint32_t CC1101_TXBYTES = 0x3A;
static constexpr uint32_t CC1101_RXBYTES = 0x3B;

// CC1101 PATABLE,TXFIFO,RXFIFO
static constexpr uint32_t CC1101_PATABLE = 0x3E;
static constexpr uint32_t CC1101_TXFIFO = 0x3F;
static constexpr uint32_t CC1101_RXFIFO = 0x3F;

static constexpr uint32_t CC1101_WRITE_BURST = 0x40;      // write burst
static constexpr uint32_t CC1101_READ_SINGLE = 0x80;      // read single
static constexpr uint32_t CC1101_READ_BURST = 0xC0;       // read burst
static constexpr uint32_t CC1101_BYTES_IN_RXFIFO = 0x7F;  // byte number in RXfifo

static constexpr uint32_t CC1101_MARCSTATE_IDLE = 0x01;

static constexpr uint32_t CC1101_MARCSTATE_RX = 0x0D;
static constexpr uint32_t CC1101_MARCSTATE_RX_END = 0x0E;
static constexpr uint32_t CC1101_MARCSTATE_TXRX_SWITCH = 0x0F;

static constexpr uint32_t CC1101_MARCSTATE_TX = 0x13;
static constexpr uint32_t CC1101_MARCSTATE_TX_END = 0x14;
static constexpr uint32_t CC1101_MARCSTATE_RXTX_SWITCH = 0x15;

}  // namespace cc1101
}  // namespace esphome
