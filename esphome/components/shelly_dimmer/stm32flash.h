/*
  stm32flash - Open Source ST STM32 flash program for Arduino
  Copyright (C) 2010 Geoffrey McRae <geoff@spacevs.com>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#pragma once

#include "esphome/core/defines.h"
#ifdef USE_SHD_FIRMWARE_DATA

#include <cstdint>
#include <memory>
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace shelly_dimmer {

/* flags */
constexpr auto STREAM_OPT_BYTE = (1 << 0);      /* byte (not frame) oriented */
constexpr auto STREAM_OPT_GVR_ETX = (1 << 1);   /* cmd GVR returns protection status */
constexpr auto STREAM_OPT_CMD_INIT = (1 << 2);  /* use INIT cmd to autodetect speed */
constexpr auto STREAM_OPT_RETRY = (1 << 3);     /* allowed read() retry after timeout */
constexpr auto STREAM_OPT_I2C = (1 << 4);       /* i2c */
constexpr auto STREAM_OPT_STRETCH_W = (1 << 5); /* warning for no-stretching commands */

constexpr auto STREAM_SERIAL = (STREAM_OPT_BYTE | STREAM_OPT_GVR_ETX | STREAM_OPT_CMD_INIT | STREAM_OPT_RETRY);
constexpr auto STREAM_I2C = (STREAM_OPT_I2C | STREAM_OPT_STRETCH_W);

constexpr auto STM32_MAX_RX_FRAME = 256;           /* cmd read memory */
constexpr auto STM32_MAX_TX_FRAME = (1 + 256 + 1); /* cmd write memory */

constexpr auto STM32_MAX_PAGES = 0x0000ffff;
constexpr auto STM32_MASS_ERASE = 0x00100000; /* > 2 x max_pages */

using stm32_err_t = enum Stm32Err {
  STM32_ERR_OK = 0,
  STM32_ERR_UNKNOWN, /* Generic error */
  STM32_ERR_NACK,
  STM32_ERR_NO_CMD, /* Command not available in bootloader */
};

using flags_t = enum Flags {
  F_NO_ME = 1 << 0, /* Mass-Erase not supported */
  F_OBLL = 1 << 1,  /* OBL_LAUNCH required */
};

using stm32_cmd_t = struct Stm32Cmd {
  uint8_t get;
  uint8_t gvr;
  uint8_t gid;
  uint8_t rm;
  uint8_t go;
  uint8_t wm;
  uint8_t er; /* this may be extended erase */
  uint8_t wp;
  uint8_t uw;
  uint8_t rp;
  uint8_t ur;
  uint8_t crc;
};

using stm32_dev_t = struct Stm32Dev {  // NOLINT
  const uint16_t id;
  const char *name;
  const uint32_t ram_start, ram_end;
  const uint32_t fl_start, fl_end;
  const uint16_t fl_pps;  // pages per sector
  const uint32_t *fl_ps;  // page size
  const uint32_t opt_start, opt_end;
  const uint32_t mem_start, mem_end;
  const uint32_t flags;
};

using stm32_t = struct Stm32 {
  uart::UARTDevice *stream;
  uint8_t flags;
  struct VarlenCmd *cmd_get_reply;
  uint8_t bl_version;
  uint8_t version;
  uint8_t option1, option2;
  uint16_t pid;
  stm32_cmd_t *cmd;
  const stm32_dev_t *dev;
};

/*
 * Specify the length of reply for command GET
 * This is helpful for frame-oriented protocols, e.g. i2c, to avoid time
 * consuming try-fail-timeout-retry operation.
 * On byte-oriented protocols, i.e. UART, this information would be skipped
 * after read the first byte, so not needed.
 */
struct VarlenCmd {
  uint8_t version;
  uint8_t length;
};

using stm32_unique_ptr = std::unique_ptr<stm32_t, void (*)(stm32_t *)>;

stm32_unique_ptr stm32_init(uart::UARTDevice *stream, uint8_t flags, char init);
stm32_err_t stm32_read_memory(const stm32_unique_ptr &stm, uint32_t address, uint8_t *data, unsigned int len);
stm32_err_t stm32_write_memory(const stm32_unique_ptr &stm, uint32_t address, const uint8_t *data, unsigned int len);
stm32_err_t stm32_wunprot_memory(const stm32_unique_ptr &stm);
stm32_err_t stm32_wprot_memory(const stm32_unique_ptr &stm);
stm32_err_t stm32_erase_memory(const stm32_unique_ptr &stm, uint32_t spage, uint32_t pages);
stm32_err_t stm32_go(const stm32_unique_ptr &stm, uint32_t address);
stm32_err_t stm32_reset_device(const stm32_unique_ptr &stm);
stm32_err_t stm32_readprot_memory(const stm32_unique_ptr &stm);
stm32_err_t stm32_runprot_memory(const stm32_unique_ptr &stm);
stm32_err_t stm32_crc_memory(const stm32_unique_ptr &stm, uint32_t address, uint32_t length, uint32_t *crc);
stm32_err_t stm32_crc_wrapper(const stm32_unique_ptr &stm, uint32_t address, uint32_t length, uint32_t *crc);
uint32_t stm32_sw_crc(uint32_t crc, uint8_t *buf, unsigned int len);

}  // namespace shelly_dimmer
}  // namespace esphome

#endif  // USE_SHD_FIRMWARE_DATA
