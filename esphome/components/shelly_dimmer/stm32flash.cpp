/*
 stm32flash - Open Source ST STM32 flash program for Arduino
 Copyright 2010 Geoffrey McRae <geoff@spacevs.com>
 Copyright 2012-2014 Tormod Volden <debian.tormod@gmail.com>

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

#include "esphome/core/defines.h"
#ifdef USE_SHD_FIRMWARE_DATA

#include <cstdint>

#include "stm32flash.h"
#include "debug.h"

#include "dev_table.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <memory>

namespace {

constexpr uint8_t STM32_ACK = 0x79;
constexpr uint8_t STM32_NACK = 0x1F;
constexpr uint8_t STM32_BUSY = 0x76;

constexpr uint8_t STM32_CMD_INIT = 0x7F;
constexpr uint8_t STM32_CMD_GET = 0x00;   /* get the version and command supported */
constexpr uint8_t STM32_CMD_GVR = 0x01;   /* get version and read protection status */
constexpr uint8_t STM32_CMD_GID = 0x02;   /* get ID */
constexpr uint8_t STM32_CMD_RM = 0x11;    /* read memory */
constexpr uint8_t STM32_CMD_GO = 0x21;    /* go */
constexpr uint8_t STM32_CMD_WM = 0x31;    /* write memory */
constexpr uint8_t STM32_CMD_WM_NS = 0x32; /* no-stretch write memory */
constexpr uint8_t STM32_CMD_ER = 0x43;    /* erase */
constexpr uint8_t STM32_CMD_EE = 0x44;    /* extended erase */
constexpr uint8_t STM32_CMD_EE_NS = 0x45; /* extended erase no-stretch */
constexpr uint8_t STM32_CMD_WP = 0x63;    /* write protect */
constexpr uint8_t STM32_CMD_WP_NS = 0x64; /* write protect no-stretch */
constexpr uint8_t STM32_CMD_UW = 0x73;    /* write unprotect */
constexpr uint8_t STM32_CMD_UW_NS = 0x74; /* write unprotect no-stretch */
constexpr uint8_t STM32_CMD_RP = 0x82;    /* readout protect */
constexpr uint8_t STM32_CMD_RP_NS = 0x83; /* readout protect no-stretch */
constexpr uint8_t STM32_CMD_UR = 0x92;    /* readout unprotect */
constexpr uint8_t STM32_CMD_UR_NS = 0x93; /* readout unprotect no-stretch */
constexpr uint8_t STM32_CMD_CRC = 0xA1;   /* compute CRC */
constexpr uint8_t STM32_CMD_ERR = 0xFF;   /* not a valid command */

constexpr uint32_t STM32_RESYNC_TIMEOUT = 35 * 1000;    /* milliseconds */
constexpr uint32_t STM32_MASSERASE_TIMEOUT = 35 * 1000; /* milliseconds */
constexpr uint32_t STM32_PAGEERASE_TIMEOUT = 5 * 1000;  /* milliseconds */
constexpr uint32_t STM32_BLKWRITE_TIMEOUT = 1 * 1000;   /* milliseconds */
constexpr uint32_t STM32_WUNPROT_TIMEOUT = 1 * 1000;    /* milliseconds */
constexpr uint32_t STM32_WPROT_TIMEOUT = 1 * 1000;      /* milliseconds */
constexpr uint32_t STM32_RPROT_TIMEOUT = 1 * 1000;      /* milliseconds */
constexpr uint32_t DEFAULT_TIMEOUT = 5 * 1000;          /* milliseconds */

constexpr uint8_t STM32_CMD_GET_LENGTH = 17; /* bytes in the reply */

/* Reset code for ARMv7-M (Cortex-M3) and ARMv6-M (Cortex-M0)
 * see ARMv7-M or ARMv6-M Architecture Reference Manual (table B3-8)
 * or "The definitive guide to the ARM Cortex-M3", section 14.4.
 */
constexpr uint8_t STM_RESET_CODE[] = {
    0x01, 0x49,              // ldr     r1, [pc, #4] ; (<AIRCR_OFFSET>)
    0x02, 0x4A,              // ldr     r2, [pc, #8] ; (<AIRCR_RESET_VALUE>)
    0x0A, 0x60,              // str     r2, [r1, #0]
    0xfe, 0xe7,              // endless: b endless
    0x0c, 0xed, 0x00, 0xe0,  // .word 0xe000ed0c <AIRCR_OFFSET> = NVIC AIRCR register address
    0x04, 0x00, 0xfa, 0x05   // .word 0x05fa0004 <AIRCR_RESET_VALUE> = VECTKEY | SYSRESETREQ
};

constexpr uint32_t STM_RESET_CODE_SIZE = sizeof(STM_RESET_CODE);

/* RM0360, Empty check
 * On STM32F070x6 and STM32F030xC devices only, internal empty check flag is
 * implemented to allow easy programming of the virgin devices by the boot loader. This flag is
 * used when BOOT0 pin is defining Main Flash memory as the target boot space. When the
 * flag is set, the device is considered as empty and System memory (boot loader) is selected
 * instead of the Main Flash as a boot space to allow user to program the Flash memory.
 * This flag is updated only during Option bytes loading: it is set when the content of the
 * address 0x08000 0000 is read as 0xFFFF FFFF, otherwise it is cleared. It means a power
 * on or setting of OBL_LAUNCH bit in FLASH_CR register is needed to clear this flag after
 * programming of a virgin device to execute user code after System reset.
 */
constexpr uint8_t STM_OBL_LAUNCH_CODE[] = {
    0x01, 0x49,              // ldr     r1, [pc, #4] ; (<FLASH_CR>)
    0x02, 0x4A,              // ldr     r2, [pc, #8] ; (<OBL_LAUNCH>)
    0x0A, 0x60,              // str     r2, [r1, #0]
    0xfe, 0xe7,              // endless: b endless
    0x10, 0x20, 0x02, 0x40,  // address: FLASH_CR = 40022010
    0x00, 0x20, 0x00, 0x00   // value: OBL_LAUNCH = 00002000
};

constexpr uint32_t STM_OBL_LAUNCH_CODE_SIZE = sizeof(STM_OBL_LAUNCH_CODE);

constexpr char TAG[] = "stm32flash";

}  // Anonymous namespace

namespace esphome {
namespace shelly_dimmer {

namespace {

int flash_addr_to_page_ceil(const stm32_unique_ptr &stm, uint32_t addr) {
  if (!(addr >= stm->dev->fl_start && addr <= stm->dev->fl_end))
    return 0;

  int page = 0;
  addr -= stm->dev->fl_start;
  const auto *psize = stm->dev->fl_ps;

  while (addr >= psize[0]) {
    addr -= psize[0];
    page++;
    if (psize[1])
      psize++;
  }

  return addr ? page + 1 : page;
}

stm32_err_t stm32_get_ack_timeout(const stm32_unique_ptr &stm, uint32_t timeout) {
  auto *stream = stm->stream;
  uint8_t rxbyte;

  if (!(stm->flags & STREAM_OPT_RETRY))
    timeout = 0;

  if (timeout == 0)
    timeout = DEFAULT_TIMEOUT;

  const uint32_t start_time = millis();
  do {
    yield();
    if (!stream->available()) {
      if (millis() < start_time + timeout)
        continue;
      ESP_LOGD(TAG, "Failed to read ACK timeout=%i", timeout);
      return STM32_ERR_UNKNOWN;
    }

    stream->read_byte(&rxbyte);

    if (rxbyte == STM32_ACK)
      return STM32_ERR_OK;
    if (rxbyte == STM32_NACK)
      return STM32_ERR_NACK;
    if (rxbyte != STM32_BUSY) {
      ESP_LOGD(TAG, "Got byte 0x%02x instead of ACK", rxbyte);
      return STM32_ERR_UNKNOWN;
    }
  } while (true);
}

stm32_err_t stm32_get_ack(const stm32_unique_ptr &stm) { return stm32_get_ack_timeout(stm, 0); }

stm32_err_t stm32_send_command_timeout(const stm32_unique_ptr &stm, const uint8_t cmd, const uint32_t timeout) {
  auto *const stream = stm->stream;

  static constexpr auto BUFFER_SIZE = 2;
  const uint8_t buf[] = {
      cmd,
      static_cast<uint8_t>(cmd ^ 0xFF),
  };
  static_assert(sizeof(buf) == BUFFER_SIZE, "Buf expected to be 2 bytes");

  stream->write_array(buf, BUFFER_SIZE);
  stream->flush();

  stm32_err_t s_err = stm32_get_ack_timeout(stm, timeout);
  if (s_err == STM32_ERR_OK)
    return STM32_ERR_OK;
  if (s_err == STM32_ERR_NACK) {
    ESP_LOGD(TAG, "Got NACK from device on command 0x%02x", cmd);
  } else {
    ESP_LOGD(TAG, "Unexpected reply from device on command 0x%02x", cmd);
  }
  return STM32_ERR_UNKNOWN;
}

stm32_err_t stm32_send_command(const stm32_unique_ptr &stm, const uint8_t cmd) {
  return stm32_send_command_timeout(stm, cmd, 0);
}

/* if we have lost sync, send a wrong command and expect a NACK */
stm32_err_t stm32_resync(const stm32_unique_ptr &stm) {
  auto *const stream = stm->stream;
  uint32_t t0 = millis();
  auto t1 = t0;

  static constexpr auto BUFFER_SIZE = 2;
  const uint8_t buf[] = {
      STM32_CMD_ERR,
      static_cast<uint8_t>(STM32_CMD_ERR ^ 0xFF),
  };
  static_assert(sizeof(buf) == BUFFER_SIZE, "Buf expected to be 2 bytes");

  uint8_t ack;
  while (t1 < t0 + STM32_RESYNC_TIMEOUT) {
    stream->write_array(buf, BUFFER_SIZE);
    stream->flush();
    if (!stream->read_array(&ack, 1)) {
      t1 = millis();
      continue;
    }
    if (ack == STM32_NACK)
      return STM32_ERR_OK;
    t1 = millis();
  }
  return STM32_ERR_UNKNOWN;
}

/*
 * some command receive reply frame with variable length, and length is
 * embedded in reply frame itself.
 * We can guess the length, but if we guess wrong the protocol gets out
 * of sync.
 * Use resync for frame oriented interfaces (e.g. I2C) and byte-by-byte
 * read for byte oriented interfaces (e.g. UART).
 *
 * to run safely, data buffer should be allocated for 256+1 bytes
 *
 * len is value of the first byte in the frame.
 */
stm32_err_t stm32_guess_len_cmd(const stm32_unique_ptr &stm, const uint8_t cmd, uint8_t *const data, unsigned int len) {
  auto *const stream = stm->stream;

  if (stm32_send_command(stm, cmd) != STM32_ERR_OK)
    return STM32_ERR_UNKNOWN;
  if (stm->flags & STREAM_OPT_BYTE) {
    /* interface is UART-like */
    if (!stream->read_array(data, 1))
      return STM32_ERR_UNKNOWN;
    len = data[0];
    if (!stream->read_array(data + 1, len + 1))
      return STM32_ERR_UNKNOWN;
    return STM32_ERR_OK;
  }

  const auto ret = stream->read_array(data, len + 2);
  if (ret && len == data[0])
    return STM32_ERR_OK;
  if (!ret) {
    /* restart with only one byte */
    if (stm32_resync(stm) != STM32_ERR_OK)
      return STM32_ERR_UNKNOWN;
    if (stm32_send_command(stm, cmd) != STM32_ERR_OK)
      return STM32_ERR_UNKNOWN;
    if (!stream->read_array(data, 1))
      return STM32_ERR_UNKNOWN;
  }

  ESP_LOGD(TAG, "Re sync (len = %d)", data[0]);
  if (stm32_resync(stm) != STM32_ERR_OK)
    return STM32_ERR_UNKNOWN;

  len = data[0];
  if (stm32_send_command(stm, cmd) != STM32_ERR_OK)
    return STM32_ERR_UNKNOWN;

  if (!stream->read_array(data, len + 2))
    return STM32_ERR_UNKNOWN;
  return STM32_ERR_OK;
}

/*
 * Some interface, e.g. UART, requires a specific init sequence to let STM32
 * autodetect the interface speed.
 * The sequence is only required one time after reset.
 * This function sends the init sequence and, in case of timeout, recovers
 * the interface.
 */
stm32_err_t stm32_send_init_seq(const stm32_unique_ptr &stm) {
  auto *const stream = stm->stream;

  stream->write_array(&STM32_CMD_INIT, 1);
  stream->flush();

  uint8_t byte;
  bool ret = stream->read_array(&byte, 1);
  if (ret && byte == STM32_ACK)
    return STM32_ERR_OK;
  if (ret && byte == STM32_NACK) {
    /* We could get error later, but let's continue, for now. */
    ESP_LOGD(TAG, "Warning: the interface was not closed properly.");
    return STM32_ERR_OK;
  }
  if (!ret) {
    ESP_LOGD(TAG, "Failed to init device.");
    return STM32_ERR_UNKNOWN;
  }

  /*
   * Check if previous STM32_CMD_INIT was taken as first byte
   * of a command. Send a new byte, we should get back a NACK.
   */
  stream->write_array(&STM32_CMD_INIT, 1);
  stream->flush();

  ret = stream->read_array(&byte, 1);
  if (ret && byte == STM32_NACK)
    return STM32_ERR_OK;
  ESP_LOGD(TAG, "Failed to init device.");
  return STM32_ERR_UNKNOWN;
}

stm32_err_t stm32_mass_erase(const stm32_unique_ptr &stm) {
  auto *const stream = stm->stream;

  if (stm32_send_command(stm, stm->cmd->er) != STM32_ERR_OK) {
    ESP_LOGD(TAG, "Can't initiate chip mass erase!");
    return STM32_ERR_UNKNOWN;
  }

  /* regular erase (0x43) */
  if (stm->cmd->er == STM32_CMD_ER) {
    const auto s_err = stm32_send_command_timeout(stm, 0xFF, STM32_MASSERASE_TIMEOUT);
    if (s_err != STM32_ERR_OK) {
      return STM32_ERR_UNKNOWN;
    }
    return STM32_ERR_OK;
  }

  /* extended erase */
  static constexpr auto BUFFER_SIZE = 3;
  const uint8_t buf[] = {
      0xFF,       /* 0xFFFF the magic number for mass erase */
      0xFF, 0x00, /* checksum */
  };
  static_assert(sizeof(buf) == BUFFER_SIZE, "Expected the buffer to be 3 bytes");
  stream->write_array(buf, 3);
  stream->flush();

  const auto s_err = stm32_get_ack_timeout(stm, STM32_MASSERASE_TIMEOUT);
  if (s_err != STM32_ERR_OK) {
    ESP_LOGD(TAG, "Mass erase failed. Try specifying the number of pages to be erased.");
    return STM32_ERR_UNKNOWN;
  }
  return STM32_ERR_OK;
}

template<typename T> std::unique_ptr<T[], void (*)(T *memory)> malloc_array_raii(size_t size) {
  // Could be constexpr in c++17
  static const auto DELETOR = [](T *memory) {
    free(memory);  // NOLINT
  };
  return std::unique_ptr<T[], decltype(DELETOR)>{static_cast<T *>(malloc(size)),  // NOLINT
                                                 DELETOR};
}

stm32_err_t stm32_pages_erase(const stm32_unique_ptr &stm, const uint32_t spage, const uint32_t pages) {
  auto *const stream = stm->stream;
  uint8_t cs = 0;
  int i = 0;

  /* The erase command reported by the bootloader is either 0x43, 0x44 or 0x45 */
  /* 0x44 is Extended Erase, a 2 byte based protocol and needs to be handled differently. */
  /* 0x45 is clock no-stretching version of Extended Erase for I2C port. */
  if (stm32_send_command(stm, stm->cmd->er) != STM32_ERR_OK) {
    ESP_LOGD(TAG, "Can't initiate chip mass erase!");
    return STM32_ERR_UNKNOWN;
  }

  /* regular erase (0x43) */
  if (stm->cmd->er == STM32_CMD_ER) {
    // Free memory with RAII
    auto buf = malloc_array_raii<uint8_t>(1 + pages + 1);

    if (!buf)
      return STM32_ERR_UNKNOWN;

    buf[i++] = pages - 1;
    cs ^= (pages - 1);
    for (auto pg_num = spage; pg_num < (pages + spage); pg_num++) {
      buf[i++] = pg_num;
      cs ^= pg_num;
    }
    buf[i++] = cs;
    stream->write_array(&buf[0], i);
    stream->flush();

    const auto s_err = stm32_get_ack_timeout(stm, pages * STM32_PAGEERASE_TIMEOUT);
    if (s_err != STM32_ERR_OK) {
      return STM32_ERR_UNKNOWN;
    }
    return STM32_ERR_OK;
  }

  /* extended erase */

  // Free memory with RAII
  auto buf = malloc_array_raii<uint8_t>(2 + 2 * pages + 1);

  if (!buf)
    return STM32_ERR_UNKNOWN;

  /* Number of pages to be erased - 1, two bytes, MSB first */
  uint8_t pg_byte = (pages - 1) >> 8;
  buf[i++] = pg_byte;
  cs ^= pg_byte;
  pg_byte = (pages - 1) & 0xFF;
  buf[i++] = pg_byte;
  cs ^= pg_byte;

  for (auto pg_num = spage; pg_num < spage + pages; pg_num++) {
    pg_byte = pg_num >> 8;
    cs ^= pg_byte;
    buf[i++] = pg_byte;
    pg_byte = pg_num & 0xFF;
    cs ^= pg_byte;
    buf[i++] = pg_byte;
  }
  buf[i++] = cs;
  stream->write_array(&buf[0], i);
  stream->flush();

  const auto s_err = stm32_get_ack_timeout(stm, pages * STM32_PAGEERASE_TIMEOUT);
  if (s_err != STM32_ERR_OK) {
    ESP_LOGD(TAG, "Page-by-page erase failed. Check the maximum pages your device supports.");
    return STM32_ERR_UNKNOWN;
  }

  return STM32_ERR_OK;
}

template<typename T> stm32_err_t stm32_check_ack_timeout(const stm32_err_t s_err, const T &&log) {
  switch (s_err) {
    case STM32_ERR_OK:
      return STM32_ERR_OK;
    case STM32_ERR_NACK:
      log();
      // TODO: c++17 [[fallthrough]]
      /* fallthrough */
    default:
      return STM32_ERR_UNKNOWN;
  }
}

/* detect CPU endian */
bool cpu_le() {
  static constexpr int N = 1;

  // returns true if little endian
  return *reinterpret_cast<const char *>(&N) == 1;
}

uint32_t le_u32(const uint32_t v) {
  if (!cpu_le())
    return ((v & 0xFF000000) >> 24) | ((v & 0x00FF0000) >> 8) | ((v & 0x0000FF00) << 8) | ((v & 0x000000FF) << 24);
  return v;
}

template<size_t N> void populate_buffer_with_address(uint8_t (&buffer)[N], uint32_t address) {
  buffer[0] = static_cast<uint8_t>(address >> 24);
  buffer[1] = static_cast<uint8_t>((address >> 16) & 0xFF);
  buffer[2] = static_cast<uint8_t>((address >> 8) & 0xFF);
  buffer[3] = static_cast<uint8_t>(address & 0xFF);
  buffer[4] = static_cast<uint8_t>(buffer[0] ^ buffer[1] ^ buffer[2] ^ buffer[3]);
}

template<typename T> stm32_unique_ptr make_stm32_with_deletor(T ptr) {
  static const auto CLOSE = [](stm32_t *stm32) {
    if (stm32) {
      free(stm32->cmd);  // NOLINT
    }
    free(stm32);  // NOLINT
  };

  // Cleanup with RAII
  return std::unique_ptr<stm32_t, decltype(CLOSE)>{ptr, CLOSE};
}

}  // Anonymous namespace

}  // namespace shelly_dimmer
}  // namespace esphome

namespace esphome {
namespace shelly_dimmer {

/* find newer command by higher code */
#define newer(prev, a) (((prev) == STM32_CMD_ERR) ? (a) : (((prev) > (a)) ? (prev) : (a)))

stm32_unique_ptr stm32_init(uart::UARTDevice *stream, const uint8_t flags, const char init) {
  uint8_t buf[257];

  auto stm = make_stm32_with_deletor(static_cast<stm32_t *>(calloc(sizeof(stm32_t), 1)));  // NOLINT

  if (!stm) {
    return make_stm32_with_deletor(nullptr);
  }
  stm->stream = stream;
  stm->flags = flags;

  stm->cmd = static_cast<stm32_cmd_t *>(malloc(sizeof(stm32_cmd_t)));  // NOLINT
  if (!stm->cmd) {
    return make_stm32_with_deletor(nullptr);
  }
  memset(stm->cmd, STM32_CMD_ERR, sizeof(stm32_cmd_t));

  if ((stm->flags & STREAM_OPT_CMD_INIT) && init) {
    if (stm32_send_init_seq(stm) != STM32_ERR_OK)
      return make_stm32_with_deletor(nullptr);
  }

  /* get the version and read protection status  */
  if (stm32_send_command(stm, STM32_CMD_GVR) != STM32_ERR_OK) {
    return make_stm32_with_deletor(nullptr);
  }

  /* From AN, only UART bootloader returns 3 bytes */
  {
    const auto len = (stm->flags & STREAM_OPT_GVR_ETX) ? 3 : 1;
    if (!stream->read_array(buf, len))
      return make_stm32_with_deletor(nullptr);

    stm->version = buf[0];
    stm->option1 = (stm->flags & STREAM_OPT_GVR_ETX) ? buf[1] : 0;
    stm->option2 = (stm->flags & STREAM_OPT_GVR_ETX) ? buf[2] : 0;
    if (stm32_get_ack(stm) != STM32_ERR_OK) {
      return make_stm32_with_deletor(nullptr);
    }
  }

  {
    const auto len = ([&]() {
      /* get the bootloader information */
      if (stm->cmd_get_reply) {
        for (auto i = 0; stm->cmd_get_reply[i].length; ++i) {
          if (stm->version == stm->cmd_get_reply[i].version) {
            return stm->cmd_get_reply[i].length;
          }
        }
      }

      return STM32_CMD_GET_LENGTH;
    })();

    if (stm32_guess_len_cmd(stm, STM32_CMD_GET, buf, len) != STM32_ERR_OK)
      return make_stm32_with_deletor(nullptr);
  }

  const auto stop = buf[0] + 1;
  stm->bl_version = buf[1];
  int new_cmds = 0;
  for (auto i = 1; i < stop; ++i) {
    const auto val = buf[i + 1];
    switch (val) {
      case STM32_CMD_GET:
        stm->cmd->get = val;
        break;
      case STM32_CMD_GVR:
        stm->cmd->gvr = val;
        break;
      case STM32_CMD_GID:
        stm->cmd->gid = val;
        break;
      case STM32_CMD_RM:
        stm->cmd->rm = val;
        break;
      case STM32_CMD_GO:
        stm->cmd->go = val;
        break;
      case STM32_CMD_WM:
      case STM32_CMD_WM_NS:
        stm->cmd->wm = newer(stm->cmd->wm, val);
        break;
      case STM32_CMD_ER:
      case STM32_CMD_EE:
      case STM32_CMD_EE_NS:
        stm->cmd->er = newer(stm->cmd->er, val);
        break;
      case STM32_CMD_WP:
      case STM32_CMD_WP_NS:
        stm->cmd->wp = newer(stm->cmd->wp, val);
        break;
      case STM32_CMD_UW:
      case STM32_CMD_UW_NS:
        stm->cmd->uw = newer(stm->cmd->uw, val);
        break;
      case STM32_CMD_RP:
      case STM32_CMD_RP_NS:
        stm->cmd->rp = newer(stm->cmd->rp, val);
        break;
      case STM32_CMD_UR:
      case STM32_CMD_UR_NS:
        stm->cmd->ur = newer(stm->cmd->ur, val);
        break;
      case STM32_CMD_CRC:
        stm->cmd->crc = newer(stm->cmd->crc, val);
        break;
      default:
        if (new_cmds++ == 0) {
          ESP_LOGD(TAG, "GET returns unknown commands (0x%2x", val);
        } else {
          ESP_LOGD(TAG, ", 0x%2x", val);
        }
    }
  }
  if (new_cmds) {
    ESP_LOGD(TAG, ")");
  }
  if (stm32_get_ack(stm) != STM32_ERR_OK) {
    return make_stm32_with_deletor(nullptr);
  }

  if (stm->cmd->get == STM32_CMD_ERR || stm->cmd->gvr == STM32_CMD_ERR || stm->cmd->gid == STM32_CMD_ERR) {
    ESP_LOGD(TAG, "Error: bootloader did not returned correct information from GET command");
    return make_stm32_with_deletor(nullptr);
  }

  /* get the device ID */
  if (stm32_guess_len_cmd(stm, stm->cmd->gid, buf, 1) != STM32_ERR_OK) {
    return make_stm32_with_deletor(nullptr);
  }
  const auto returned = buf[0] + 1;
  if (returned < 2) {
    ESP_LOGD(TAG, "Only %d bytes sent in the PID, unknown/unsupported device", returned);
    return make_stm32_with_deletor(nullptr);
  }
  stm->pid = (buf[1] << 8) | buf[2];
  if (returned > 2) {
    ESP_LOGD(TAG, "This bootloader returns %d extra bytes in PID:", returned);
    for (auto i = 2; i <= returned; i++)
      ESP_LOGD(TAG, " %02x", buf[i]);
  }
  if (stm32_get_ack(stm) != STM32_ERR_OK) {
    return make_stm32_with_deletor(nullptr);
  }

  stm->dev = DEVICES;
  while (stm->dev->id != 0x00 && stm->dev->id != stm->pid)
    ++stm->dev;

  if (!stm->dev->id) {
    ESP_LOGD(TAG, "Unknown/unsupported device (Device ID: 0x%03x)", stm->pid);
    return make_stm32_with_deletor(nullptr);
  }

  return stm;
}

stm32_err_t stm32_read_memory(const stm32_unique_ptr &stm, const uint32_t address, uint8_t *data,
                              const unsigned int len) {
  auto *const stream = stm->stream;

  if (!len)
    return STM32_ERR_OK;

  if (len > 256) {
    ESP_LOGD(TAG, "Error: READ length limit at 256 bytes");
    return STM32_ERR_UNKNOWN;
  }

  if (stm->cmd->rm == STM32_CMD_ERR) {
    ESP_LOGD(TAG, "Error: READ command not implemented in bootloader.");
    return STM32_ERR_NO_CMD;
  }

  if (stm32_send_command(stm, stm->cmd->rm) != STM32_ERR_OK)
    return STM32_ERR_UNKNOWN;

  static constexpr auto BUFFER_SIZE = 5;
  uint8_t buf[BUFFER_SIZE];
  populate_buffer_with_address(buf, address);

  stream->write_array(buf, BUFFER_SIZE);
  stream->flush();

  if (stm32_get_ack(stm) != STM32_ERR_OK)
    return STM32_ERR_UNKNOWN;

  if (stm32_send_command(stm, len - 1) != STM32_ERR_OK)
    return STM32_ERR_UNKNOWN;

  if (!stream->read_array(data, len))
    return STM32_ERR_UNKNOWN;

  return STM32_ERR_OK;
}

stm32_err_t stm32_write_memory(const stm32_unique_ptr &stm, uint32_t address, const uint8_t *data,
                               const unsigned int len) {
  auto *const stream = stm->stream;

  if (!len)
    return STM32_ERR_OK;

  if (len > 256) {
    ESP_LOGD(TAG, "Error: READ length limit at 256 bytes");
    return STM32_ERR_UNKNOWN;
  }

  /* must be 32bit aligned */
  if (address & 0x3) {
    ESP_LOGD(TAG, "Error: WRITE address must be 4 byte aligned");
    return STM32_ERR_UNKNOWN;
  }

  if (stm->cmd->wm == STM32_CMD_ERR) {
    ESP_LOGD(TAG, "Error: WRITE command not implemented in bootloader.");
    return STM32_ERR_NO_CMD;
  }

  /* send the address and checksum */
  if (stm32_send_command(stm, stm->cmd->wm) != STM32_ERR_OK)
    return STM32_ERR_UNKNOWN;

  static constexpr auto BUFFER_SIZE = 5;
  uint8_t buf1[BUFFER_SIZE];
  populate_buffer_with_address(buf1, address);

  stream->write_array(buf1, BUFFER_SIZE);
  stream->flush();
  if (stm32_get_ack(stm) != STM32_ERR_OK)
    return STM32_ERR_UNKNOWN;

  const unsigned int aligned_len = (len + 3) & ~3;
  uint8_t cs = aligned_len - 1;
  uint8_t buf[256 + 2];

  buf[0] = aligned_len - 1;
  for (auto i = 0; i < len; i++) {
    cs ^= data[i];
    buf[i + 1] = data[i];
  }
  /* padding data */
  for (auto i = len; i < aligned_len; i++) {
    cs ^= 0xFF;
    buf[i + 1] = 0xFF;
  }
  buf[aligned_len + 1] = cs;
  stream->write_array(buf, aligned_len + 2);
  stream->flush();

  const auto s_err = stm32_get_ack_timeout(stm, STM32_BLKWRITE_TIMEOUT);
  if (s_err != STM32_ERR_OK) {
    return STM32_ERR_UNKNOWN;
  }
  return STM32_ERR_OK;
}

stm32_err_t stm32_wunprot_memory(const stm32_unique_ptr &stm) {
  if (stm->cmd->uw == STM32_CMD_ERR) {
    ESP_LOGD(TAG, "Error: WRITE UNPROTECT command not implemented in bootloader.");
    return STM32_ERR_NO_CMD;
  }

  if (stm32_send_command(stm, stm->cmd->uw) != STM32_ERR_OK)
    return STM32_ERR_UNKNOWN;

  return stm32_check_ack_timeout(stm32_get_ack_timeout(stm, STM32_WUNPROT_TIMEOUT),
                                 []() { ESP_LOGD(TAG, "Error: Failed to WRITE UNPROTECT"); });
}

stm32_err_t stm32_wprot_memory(const stm32_unique_ptr &stm) {
  if (stm->cmd->wp == STM32_CMD_ERR) {
    ESP_LOGD(TAG, "Error: WRITE PROTECT command not implemented in bootloader.");
    return STM32_ERR_NO_CMD;
  }

  if (stm32_send_command(stm, stm->cmd->wp) != STM32_ERR_OK)
    return STM32_ERR_UNKNOWN;

  return stm32_check_ack_timeout(stm32_get_ack_timeout(stm, STM32_WPROT_TIMEOUT),
                                 []() { ESP_LOGD(TAG, "Error: Failed to WRITE PROTECT"); });
}

stm32_err_t stm32_runprot_memory(const stm32_unique_ptr &stm) {
  if (stm->cmd->ur == STM32_CMD_ERR) {
    ESP_LOGD(TAG, "Error: READOUT UNPROTECT command not implemented in bootloader.");
    return STM32_ERR_NO_CMD;
  }

  if (stm32_send_command(stm, stm->cmd->ur) != STM32_ERR_OK)
    return STM32_ERR_UNKNOWN;

  return stm32_check_ack_timeout(stm32_get_ack_timeout(stm, STM32_MASSERASE_TIMEOUT),
                                 []() { ESP_LOGD(TAG, "Error: Failed to READOUT UNPROTECT"); });
}

stm32_err_t stm32_readprot_memory(const stm32_unique_ptr &stm) {
  if (stm->cmd->rp == STM32_CMD_ERR) {
    ESP_LOGD(TAG, "Error: READOUT PROTECT command not implemented in bootloader.");
    return STM32_ERR_NO_CMD;
  }

  if (stm32_send_command(stm, stm->cmd->rp) != STM32_ERR_OK)
    return STM32_ERR_UNKNOWN;

  return stm32_check_ack_timeout(stm32_get_ack_timeout(stm, STM32_RPROT_TIMEOUT),
                                 []() { ESP_LOGD(TAG, "Error: Failed to READOUT PROTECT"); });
}

stm32_err_t stm32_erase_memory(const stm32_unique_ptr &stm, uint32_t spage, uint32_t pages) {
  if (!pages || spage > STM32_MAX_PAGES || ((pages != STM32_MASS_ERASE) && ((spage + pages) > STM32_MAX_PAGES)))
    return STM32_ERR_OK;

  if (stm->cmd->er == STM32_CMD_ERR) {
    ESP_LOGD(TAG, "Error: ERASE command not implemented in bootloader.");
    return STM32_ERR_NO_CMD;
  }

  if (pages == STM32_MASS_ERASE) {
    /*
     * Not all chips support mass erase.
     * Mass erase can be obtained executing a "readout protect"
     * followed by "readout un-protect". This method is not
     * suggested because can hang the target if a debug SWD/JTAG
     * is connected. When the target enters in "readout
     * protection" mode it will consider the debug connection as
     * a tentative of intrusion and will hang.
     * Erasing the flash page-by-page is the safer way to go.
     */
    if (!(stm->dev->flags & F_NO_ME))
      return stm32_mass_erase(stm);

    pages = flash_addr_to_page_ceil(stm, stm->dev->fl_end);
  }

  /*
   * Some device, like STM32L152, cannot erase more than 512 pages in
   * one command. Split the call.
   */
  static constexpr uint32_t MAX_PAGE_SIZE = 512;
  while (pages) {
    const auto n = std::min(pages, MAX_PAGE_SIZE);
    const auto s_err = stm32_pages_erase(stm, spage, n);
    if (s_err != STM32_ERR_OK)
      return s_err;
    spage += n;
    pages -= n;
  }
  return STM32_ERR_OK;
}

static stm32_err_t stm32_run_raw_code(const stm32_unique_ptr &stm, uint32_t target_address, const uint8_t *code,
                                      uint32_t code_size) {
  static constexpr uint32_t BUFFER_SIZE = 256;

  const auto stack_le = le_u32(0x20002000);
  const auto code_address_le = le_u32(target_address + 8 + 1);  // thumb mode address (!)
  uint32_t length = code_size + 8;

  /* Must be 32-bit aligned */
  if (target_address & 0x3) {
    ESP_LOGD(TAG, "Error: code address must be 4 byte aligned");
    return STM32_ERR_UNKNOWN;
  }

  // Could be constexpr in c++17
  static const auto DELETOR = [](uint8_t *memory) {
    free(memory);  // NOLINT
  };

  // Free memory with RAII
  std::unique_ptr<uint8_t, decltype(DELETOR)> mem{static_cast<uint8_t *>(malloc(length)),  // NOLINT
                                                  DELETOR};

  if (!mem)
    return STM32_ERR_UNKNOWN;

  memcpy(mem.get(), &stack_le, sizeof(stack_le));
  memcpy(mem.get() + 4, &code_address_le, sizeof(code_address_le));
  memcpy(mem.get() + 8, code, code_size);

  auto *pos = mem.get();
  auto address = target_address;
  while (length > 0) {
    const auto w = std::min(length, BUFFER_SIZE);
    if (stm32_write_memory(stm, address, pos, w) != STM32_ERR_OK) {
      return STM32_ERR_UNKNOWN;
    }

    address += w;
    pos += w;
    length -= w;
  }

  return stm32_go(stm, target_address);
}

stm32_err_t stm32_go(const stm32_unique_ptr &stm, const uint32_t address) {
  auto *const stream = stm->stream;

  if (stm->cmd->go == STM32_CMD_ERR) {
    ESP_LOGD(TAG, "Error: GO command not implemented in bootloader.");
    return STM32_ERR_NO_CMD;
  }

  if (stm32_send_command(stm, stm->cmd->go) != STM32_ERR_OK)
    return STM32_ERR_UNKNOWN;

  static constexpr auto BUFFER_SIZE = 5;
  uint8_t buf[BUFFER_SIZE];
  populate_buffer_with_address(buf, address);

  stream->write_array(buf, BUFFER_SIZE);
  stream->flush();

  if (stm32_get_ack(stm) != STM32_ERR_OK)
    return STM32_ERR_UNKNOWN;
  return STM32_ERR_OK;
}

stm32_err_t stm32_reset_device(const stm32_unique_ptr &stm) {
  const auto target_address = stm->dev->ram_start;

  if (stm->dev->flags & F_OBLL) {
    /* set the OBL_LAUNCH bit to reset device (see RM0360, 2.5) */
    return stm32_run_raw_code(stm, target_address, STM_OBL_LAUNCH_CODE, STM_OBL_LAUNCH_CODE_SIZE);
  } else {
    return stm32_run_raw_code(stm, target_address, STM_RESET_CODE, STM_RESET_CODE_SIZE);
  }
}

stm32_err_t stm32_crc_memory(const stm32_unique_ptr &stm, const uint32_t address, const uint32_t length,
                             uint32_t *const crc) {
  static constexpr auto BUFFER_SIZE = 5;
  auto *const stream = stm->stream;

  if (address & 0x3 || length & 0x3) {
    ESP_LOGD(TAG, "Start and end addresses must be 4 byte aligned");
    return STM32_ERR_UNKNOWN;
  }

  if (stm->cmd->crc == STM32_CMD_ERR) {
    ESP_LOGD(TAG, "Error: CRC command not implemented in bootloader.");
    return STM32_ERR_NO_CMD;
  }

  if (stm32_send_command(stm, stm->cmd->crc) != STM32_ERR_OK)
    return STM32_ERR_UNKNOWN;

  {
    static constexpr auto BUFFER_SIZE = 5;
    uint8_t buf[BUFFER_SIZE];
    populate_buffer_with_address(buf, address);

    stream->write_array(buf, BUFFER_SIZE);
    stream->flush();
  }

  if (stm32_get_ack(stm) != STM32_ERR_OK)
    return STM32_ERR_UNKNOWN;

  {
    static constexpr auto BUFFER_SIZE = 5;
    uint8_t buf[BUFFER_SIZE];
    populate_buffer_with_address(buf, address);

    stream->write_array(buf, BUFFER_SIZE);
    stream->flush();
  }

  if (stm32_get_ack(stm) != STM32_ERR_OK)
    return STM32_ERR_UNKNOWN;

  if (stm32_get_ack(stm) != STM32_ERR_OK)
    return STM32_ERR_UNKNOWN;

  {
    uint8_t buf[BUFFER_SIZE];
    if (!stream->read_array(buf, BUFFER_SIZE))
      return STM32_ERR_UNKNOWN;

    if (buf[4] != (buf[0] ^ buf[1] ^ buf[2] ^ buf[3]))
      return STM32_ERR_UNKNOWN;

    *crc = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
  }

  return STM32_ERR_OK;
}

/*
 * CRC computed by STM32 is similar to the standard crc32_be()
 * implemented, for example, in Linux kernel in ./lib/crc32.c
 * But STM32 computes it on units of 32 bits word and swaps the
 * bytes of the word before the computation.
 * Due to byte swap, I cannot use any CRC available in existing
 * libraries, so here is a simple not optimized implementation.
 */
uint32_t stm32_sw_crc(uint32_t crc, uint8_t *buf, unsigned int len) {
  static constexpr uint32_t CRCPOLY_BE = 0x04c11db7;
  static constexpr uint32_t CRC_MSBMASK = 0x80000000;

  if (len & 0x3) {
    ESP_LOGD(TAG, "Buffer length must be multiple of 4 bytes");
    return 0;
  }

  while (len) {
    uint32_t data = *buf++;
    data |= *buf++ << 8;
    data |= *buf++ << 16;
    data |= *buf++ << 24;
    len -= 4;

    crc ^= data;

    for (size_t i = 0; i < 32; ++i) {
      if (crc & CRC_MSBMASK) {
        crc = (crc << 1) ^ CRCPOLY_BE;
      } else {
        crc = (crc << 1);
      }
    }
  }
  return crc;
}

stm32_err_t stm32_crc_wrapper(const stm32_unique_ptr &stm, uint32_t address, uint32_t length, uint32_t *crc) {
  static constexpr uint32_t CRC_INIT_VALUE = 0xFFFFFFFF;
  static constexpr uint32_t BUFFER_SIZE = 256;

  uint8_t buf[BUFFER_SIZE];

  if (address & 0x3 || length & 0x3) {
    ESP_LOGD(TAG, "Start and end addresses must be 4 byte aligned");
    return STM32_ERR_UNKNOWN;
  }

  if (stm->cmd->crc != STM32_CMD_ERR)
    return stm32_crc_memory(stm, address, length, crc);

  const auto start = address;
  const auto total_len = length;
  uint32_t current_crc = CRC_INIT_VALUE;
  while (length) {
    const auto len = std::min(BUFFER_SIZE, length);
    if (stm32_read_memory(stm, address, buf, len) != STM32_ERR_OK) {
      ESP_LOGD(TAG, "Failed to read memory at address 0x%08x, target write-protected?", address);
      return STM32_ERR_UNKNOWN;
    }
    current_crc = stm32_sw_crc(current_crc, buf, len);
    length -= len;
    address += len;

    ESP_LOGD(TAG, "\rCRC address 0x%08x (%.2f%%) ", address, (100.0f / (float) total_len) * (float) (address - start));
  }
  ESP_LOGD(TAG, "Done.");
  *crc = current_crc;
  return STM32_ERR_OK;
}

}  // namespace shelly_dimmer
}  // namespace esphome

#endif  // USE_SHD_FIRMWARE_DATA
