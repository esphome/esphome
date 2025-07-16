/// @file wk_reg_def.h
/// @author DrCoolZic
/// @brief  WeiKai component family - registers' definition
/// @date Last Modified: 2024/02/18 15:49:18
#pragma once

namespace esphome {
namespace weikai {

////////////////////////////////////////////////////////////////////////////////////////
/// Definition of the WeiKai registers
////////////////////////////////////////////////////////////////////////////////////////

/// @defgroup wk2168_gr_ WeiKai Global Registers
/// This section groups all **Global Registers**: these registers are global to the
/// the WeiKai chip (i.e. independent of the UART channel used)
/// @note only registers and parameters used have been fully documented
/// @{

/// @brief Global Control Register - 00 0000
/// @details @code
///  -------------------------------------------------------------------------
///  |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   | bit
///  -------------------------------------------------------------------------
///  |   M0   |   M1   |       RSV       |  C4EN  |  C3EN  |  C2EN  |  C1EN  | name
///  -------------------------------------------------------------------------
///  |    R   |    R   |    R   |    R   |   W/R  |   W/R  |   W/R  |   W/R  | type
///  -------------------------------------------------------------------------
///  |    1   |    1   |    1   |    1   |    0   |    0   |    0   |    0   | reset
///  -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t WKREG_GENA = 0x00;
/// @brief Channel 4 enable clock (0: disable, 1: enable)
constexpr uint8_t GENA_C4EN = 1 << 3;
/// @brief Channel 3 enable clock (0: disable, 1: enable)
constexpr uint8_t GENA_C3EN = 1 << 2;
/// @brief Channel 2 enable clock (0: disable, 1: enable)
constexpr uint8_t GENA_C2EN = 1 << 1;
/// @brief Channel 1 enable clock (0: disable, 1: enable)
constexpr uint8_t GENA_C1EN = 1 << 0;

/// @brief Global Reset Register - 00 0001
/// @details @code
///  -------------------------------------------------------------------------
///  |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   | bit
///  -------------------------------------------------------------------------
///  | C4SLEEP| C3SLEEP| C2SLEEP| C1SLEEP|  C4RST |  C3RST |  C2RST |  C1RST | name
///  -------------------------------------------------------------------------
///  |    R   |    R   |    R   |    R   |  W1/R0 |  W1/R0 |  W1/R0 |  W1/R0 | type
///  -------------------------------------------------------------------------
///  |    0   |    0   |    0   |    0   |    0   |    0   |    0   |    0   | reset
///  -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t WKREG_GRST = 0x01;
/// @brief Channel 4 soft reset (0: not reset, 1: reset)
constexpr uint8_t GRST_C4RST = 1 << 3;
/// @brief Channel 3 soft reset (0: not reset, 1: reset)
constexpr uint8_t GRST_C3RST = 1 << 2;
/// @brief Channel 2 soft reset (0: not reset, 1: reset)
constexpr uint8_t GRST_C2RST = 1 << 1;
/// @brief Channel 1 soft reset (0: not reset, 1: reset)
constexpr uint8_t GRST_C1RST = 1 << 0;

/// @brief Global Master channel control register (not used) - 000010
constexpr uint8_t WKREG_GMUT = 0x02;

/// Global interrupt register (not used) - 01 0000
constexpr uint8_t WKREG_GIER = 0x10;

/// Global interrupt flag register (not used) 01 0001
constexpr uint8_t WKREG_GIFR = 0x11;

/// @brief Global GPIO direction register - 10 0001
/// @details @code
///  -------------------------------------------------------------------------
///  |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   | bit
///  -------------------------------------------------------------------------
///  |   PD7  |   PD6  |   PD5  |   PD4  |   PD3  |   PD2  |   PD1  |   PD0  | name
///  -------------------------------------------------------------------------
///  |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  | type
///  -------------------------------------------------------------------------
///  |    0   |    0   |    0   |    0   |    0   |    0   |    0   |    0   | reset
///  -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t WKREG_GPDIR = 0x21;

/// @brief Global GPIO data register - 11 0001
/// @details @code
///  -------------------------------------------------------------------------
///  |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   | bit
///  -------------------------------------------------------------------------
///  |   PV7  |   PV6  |   PV5  |   PV4  |   PV3  |   PV2  |   PV1  |   PV0  | name
///  -------------------------------------------------------------------------
///  |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  | type
///  -------------------------------------------------------------------------
///  |    0   |    0   |    0   |    0   |    0   |    0   |    0   |    0   | reset
///  -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t WKREG_GPDAT = 0x31;

/// @}
/// @defgroup WeiKai_cr_ WeiKai Channel Registers
/// @brief Definition of the register linked to a particular channel
/// @details This topic groups all the **Channel Registers**: these registers are specific
/// to the a specific channel i.e. each channel has its own set of registers
/// @note only registers and parameters used have been documented
/// @{

/// @defgroup cr_p0 Channel registers when SPAGE=0
/// @brief Definition of the register linked to a particular channel when SPAGE=0
/// @details The channel registers are further splitted into two groups.
/// This first group is defined when the Global register WKREG_SPAGE is 0
/// @{

/// @brief Global Page register c0/c1 0011
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   | bit
/// -------------------------------------------------------------------------
/// |                             RSV                              |  PAGE  | name
/// -------------------------------------------------------------------------
/// |    R   |    R   |    R   |    R   |    R   |    R   |    R   |   W/R  | type
/// -------------------------------------------------------------------------
/// |    0   |    0   |    0   |    0   |    0   |    0   |    0   |    0   | reset
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t WKREG_SPAGE = 0x03;

/// @brief Serial Control Register - c0/c1 0100
/// @details @code
///  -------------------------------------------------------------------------
///  |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   | bit
///  -------------------------------------------------------------------------
///  |                     RSV                    | SLPEN  |  TXEN  |  RXEN  | name
///  -------------------------------------------------------------------------
///  |    R   |    R   |    R   |    R   |    R   |   R/W  |   R/W  |   W/R  | type
///  -------------------------------------------------------------------------
///  |    0   |    0   |    0   |    0   |    0   |    0   |    0   |    0   | reset
///  -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t WKREG_SCR = 0x04;
/// @brief transmission control (0: enable, 1: disable)
constexpr uint8_t SCR_TXEN = 1 << 1;
/// @brief receiving control (0: enable, 1: disable)
constexpr uint8_t SCR_RXEN = 1 << 0;

/// @brief Line Configuration Register - c0/c1 0101
/// @details @code
///  -------------------------------------------------------------------------
///  |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   | bit
///  -------------------------------------------------------------------------
///  |        RSV      |  BREAK |  IREN  |  PAEN  |      PARITY     |  STPL  | name
///  -------------------------------------------------------------------------
///  |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  | type
///  -------------------------------------------------------------------------
///  |    0   |    0   |    0   |    0   |    0   |    0   |    0   |    0   | reset
///  -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t WKREG_LCR = 0x05;
/// @brief Parity enable (0: no check, 1: check)
constexpr uint8_t LCR_PAEN = 1 << 3;
/// @brief Parity force 0
constexpr uint8_t LCR_PAR_F0 = 0 << 1;
/// @brief Parity odd
constexpr uint8_t LCR_PAR_ODD = 1 << 1;
/// @brief Parity even
constexpr uint8_t LCR_PAR_EVEN = 2 << 1;
/// @brief Parity force 1
constexpr uint8_t LCR_PAR_F1 = 3 << 1;
/// @brief Stop length (0: 1 bit, 1: 2 bits)
constexpr uint8_t LCR_STPL = 1 << 0;

/// @brief FIFO Control Register - c0/c1 0110
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   | bit
/// -------------------------------------------------------------------------
/// |      TFTRIG     |      RFTRIG     |  TFEN  |  RFEN  |  TFRST |  RFRST | name
/// -------------------------------------------------------------------------
/// |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  | type
/// -------------------------------------------------------------------------
/// |    0   |    0   |    0   |    0   |    0   |    0   |    0   |    0   | reset
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t WKREG_FCR = 0x06;
/// @brief Transmitter FIFO enable
constexpr uint8_t FCR_TFEN = 1 << 3;
/// @brief Receiver FIFO enable
constexpr uint8_t FCR_RFEN = 1 << 2;
/// @brief Transmitter FIFO reset
constexpr uint8_t FCR_TFRST = 1 << 1;
/// @brief Receiver FIFO reset
constexpr uint8_t FCR_RFRST = 1 << 0;

/// @brief Serial Interrupt Enable Register (not used)  - c0/c1 0111
constexpr uint8_t WKREG_SIER = 0x07;

/// @brief Serial Interrupt Flag Register (not used) - c0/c1 1000
constexpr uint8_t WKREG_SIFR = 0x08;

/// @brief Transmitter FIFO Count - c0/c1 1001
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// -------------------------------------------------------------------------
/// |                  NUMBER OF DATA IN TRANSMITTER FIFO                   |
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t WKREG_TFCNT = 0x09;

/// @brief Receiver FIFO count - c0/c1 1010
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// -------------------------------------------------------------------------
/// |                    NUMBER OF DATA IN RECEIVER FIFO                    |
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t WKREG_RFCNT = 0x0A;

/// @brief FIFO Status Register - c0/c1 1011
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   | bit
/// -------------------------------------------------------------------------
/// |  RFOE  |  RFLB  |  RFFE  |  RFPE  | RFDAT  | TFDAT  | TFFULL |  TBUSY | name
/// -------------------------------------------------------------------------
/// |    R   |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  |   W/R  | type
/// -------------------------------------------------------------------------
/// |    0   |    0   |    0   |    0   |    0   |    0   |    0   |    0   | reset
/// -------------------------------------------------------------------------
/// @endcode
/// @warning The received buffer can hold 256 bytes. However, as the RFCNT reg
/// is 8 bits, if we have 256 byte in the register this is reported as 0 ! Therefore
/// RFCNT=0 can indicate that there are 0 **or** 256 bytes in the buffer. If we
/// have RFDAT = 1 and RFCNT = 0 it should be interpreted as 256 bytes in the FIFO.
/// @note Note that in case of overflow the RFOE goes to one **but** as soon as you read
/// the FSR this bit is cleared. Therefore Overflow can be read only once.
/// @n The same problem applies to the transmit buffer but here we have to check the
/// TFFULL flag. So if TFFULL is set and TFCNT is 0 this should be interpreted as 256
constexpr uint8_t WKREG_FSR = 0x0B;
/// @brief Receiver FIFO Overflow Error (0: no OE, 1: OE)
constexpr uint8_t FSR_RFOE = 1 << 7;
/// @brief Receiver FIFO Line Break (0: no LB, 1: LB)
constexpr uint8_t FSR_RFLB = 1 << 6;
/// @brief Receiver FIFO Frame Error (0: no FE, 1: FE)
constexpr uint8_t FSR_RFFE = 1 << 5;
/// @brief Receiver Parity Error (0: no PE, 1: PE)
constexpr uint8_t FSR_RFPE = 1 << 4;
/// @brief Receiver FIFO count (0: empty, 1: not empty)
constexpr uint8_t FSR_RFDAT = 1 << 3;
/// @brief Transmitter FIFO count (0: empty, 1: not empty)
constexpr uint8_t FSR_TFDAT = 1 << 2;
/// @brief Transmitter FIFO full (0: not full, 1: full)
constexpr uint8_t FSR_TFFULL = 1 << 1;
/// @brief Transmitter busy (0 nothing to transmit, 1: transmitter busy sending)
constexpr uint8_t FSR_TBUSY = 1 << 0;

/// @brief Line Status Register (not used - using FIFO)
constexpr uint8_t WKREG_LSR = 0x0C;

/// @brief FIFO Data Register (not used - does not seems to work)
constexpr uint8_t WKREG_FDAT = 0x0D;

/// @}
/// @defgroup cr_p1 Channel registers for SPAGE=1
/// @brief Definition of the register linked to a particular channel when SPAGE=1
/// @details The channel registers are further splitted into two groups.
/// This second group is defined when the Global register WKREG_SPAGE is 1
/// @{

/// @brief Baud rate configuration register: high byte - c0/c1 0100
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// -------------------------------------------------------------------------
/// |                      High byte of the baud rate                       |
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t WKREG_BRH = 0x04;

/// @brief Baud rate configuration register: low byte - c0/c1 0101
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// -------------------------------------------------------------------------
/// |                       Low byte of the baud rate                       |
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t WKREG_BRL = 0x05;

/// @brief Baud rate configuration register decimal part - c0/c1 0110
constexpr uint8_t WKREG_BRD = 0x06;

/// @brief Receive FIFO Interrupt trigger configuration (not used) - c0/c1 0111
constexpr uint8_t WKREG_RFI = 0x07;

/// @brief Transmit FIFO interrupt trigger configuration (not used) - c0/c1 1000
constexpr uint8_t WKREG_TFI = 0x08;

/// @}
/// @}
}  // namespace weikai
}  // namespace esphome
