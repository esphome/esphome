/*
  stm32flash - Open Source ST STM32 flash program for Arduino
  Copyright (C) 2010 Geoffrey McRae <geoff@spacevs.com>
  Copyright (C) 2014-2015 Antonio Borneo <borneo.antonio@gmail.com>

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
#include "esphome/core/hal.h"
#include "stm32flash.h"

namespace esphome::shelly_dimmer {

constexpr uint32_t SZ_128 = 0x00000080;
constexpr uint32_t SZ_256 = 0x00000100;
constexpr uint32_t SZ_1K = 0x00000400;
constexpr uint32_t SZ_2K = 0x00000800;
constexpr uint32_t SZ_16K = 0x00004000;
constexpr uint32_t SZ_32K = 0x00008000;
constexpr uint32_t SZ_64K = 0x00010000;
constexpr uint32_t SZ_128K = 0x00020000;
constexpr uint32_t SZ_256K = 0x00040000;

/*
 * Page-size for page-by-page flash erase.
 * Arrays are zero terminated; last non-zero value is automatically repeated
 * In PROGMEM and read by direct indexing: each entry is an aligned 32-bit load, which is safe from flash on ESP8266
 */

/* fixed size pages */
alignas(4) constexpr uint32_t p_128[] PROGMEM = {SZ_128, 0};  // NOLINT
alignas(4) constexpr uint32_t p_256[] PROGMEM = {SZ_256, 0};  // NOLINT
alignas(4) constexpr uint32_t p_1k[] PROGMEM = {SZ_1K, 0};    // NOLINT
alignas(4) constexpr uint32_t p_2k[] PROGMEM = {SZ_2K, 0};    // NOLINT
/* F2 and F4 page size */
alignas(4) constexpr uint32_t f2f4[] PROGMEM = {SZ_16K, SZ_16K, SZ_16K, SZ_16K, SZ_64K, SZ_128K, 0};  // NOLINT
/* F4 dual bank page size */
alignas(4) constexpr uint32_t f4db[] PROGMEM = {SZ_16K,  SZ_16K, SZ_16K, SZ_16K, SZ_64K, SZ_128K, SZ_128K,  // NOLINT
                                                SZ_128K, SZ_16K, SZ_16K, SZ_16K, SZ_16K, SZ_64K,  SZ_128K, 0};
/* F7 page size */
alignas(4) constexpr uint32_t f7[] PROGMEM = {SZ_32K, SZ_32K, SZ_32K, SZ_32K, SZ_128K, SZ_256K, 0};  // NOLINT

/*
 * Device table, corresponds to the "Bootloader device-dependant parameters"
 * table in ST document AN2606.
 * Only the columns the updater reads are kept.
 */
alignas(4) constexpr stm32_dev_t DEVICES[] PROGMEM = {
    /* ID   SRAM-start  FLASH-address-range  PSize  Flags */
    /* F0 */
    // STM32F030x8/F05xxx
    {0x440, 0x20000800, 0x08000000, 0x08010000, p_1k, 0},
    // STM32F030xC/F09xxx
    {0x442, 0x20001800, 0x08000000, 0x08040000, p_2k, F_OBLL},
    // STM32F03xx4/6
    {0x444, 0x20000800, 0x08000000, 0x08008000, p_1k, 0},
    // STM32F04xxx/F070x6
    {0x445, 0x20001800, 0x08000000, 0x08008000, p_1k, 0},
    // STM32F070xB/F071xx/F72xx
    {0x448, 0x20001800, 0x08000000, 0x08020000, p_2k, 0},
    /* F1 */
    // STM32F10xxx Low-density
    {0x412, 0x20000200, 0x08000000, 0x08008000, p_1k, 0},
    // STM32F10xxx Medium-density
    {0x410, 0x20000200, 0x08000000, 0x08020000, p_1k, 0},
    // STM32F10xxx High-density
    {0x414, 0x20000200, 0x08000000, 0x08080000, p_2k, 0},
    // STM32F10xxx Medium-density VL
    {0x420, 0x20000200, 0x08000000, 0x08020000, p_1k, 0},
    // STM32F10xxx High-density VL
    {0x428, 0x20000200, 0x08000000, 0x08080000, p_2k, 0},
    // STM32F105xx/F107xx
    {0x418, 0x20001000, 0x08000000, 0x08040000, p_2k, 0},
    // STM32F10xxx XL-density
    {0x430, 0x20000800, 0x08000000, 0x08100000, p_2k, 0},
    /* F2 */
    // STM32F2xxxx
    {0x411, 0x20002000, 0x08000000, 0x08100000, f2f4, 0},
    /* F3 */
    // STM32F373xx/F378xx
    {0x432, 0x20001400, 0x08000000, 0x08040000, p_2k, 0},
    // STM32F302xB(C)/F303xB(C)/F358xx
    {0x422, 0x20001400, 0x08000000, 0x08040000, p_2k, 0},
    // STM32F301xx/F302x4(6/8)/F318xx
    {0x439, 0x20001800, 0x08000000, 0x08010000, p_2k, 0},
    // STM32F303x4(6/8)/F334xx/F328xx
    {0x438, 0x20001800, 0x08000000, 0x08010000, p_2k, 0},
    // STM32F302xD(E)/F303xD(E)/F398xx
    {0x446, 0x20001800, 0x08000000, 0x08080000, p_2k, 0},
    /* F4 */
    // STM32F40xxx/41xxx
    {0x413, 0x20003000, 0x08000000, 0x08100000, f2f4, 0},
    // STM32F42xxx/43xxx
    {0x419, 0x20003000, 0x08000000, 0x08200000, f4db, 0},
    // STM32F401xB(C)
    {0x423, 0x20003000, 0x08000000, 0x08040000, f2f4, 0},
    // STM32F401xD(E)
    {0x433, 0x20003000, 0x08000000, 0x08080000, f2f4, 0},
    // STM32F410xx
    {0x458, 0x20003000, 0x08000000, 0x08020000, f2f4, 0},
    // STM32F411xx
    {0x431, 0x20003000, 0x08000000, 0x08080000, f2f4, 0},
    // STM32F446xx
    {0x421, 0x20003000, 0x08000000, 0x08080000, f2f4, 0},
    // STM32F469xx
    {0x434, 0x20003000, 0x08000000, 0x08200000, f4db, 0},
    /* F7 */
    // STM32F74xxx/75xxx
    {0x449, 0x20004000, 0x08000000, 0x08100000, f7, 0},
    /* L0 */
    // STM32L031xx/041xx
    {0x425, 0x20001000, 0x08000000, 0x08008000, p_128, 0},
    // STM32L05xxx/06xxx
    {0x417, 0x20001000, 0x08000000, 0x08010000, p_128, 0},
    // STM32L07xxx/08xxx
    {0x447, 0x20002000, 0x08000000, 0x08030000, p_128, 0},
    /* L1 */
    // STM32L1xxx6(8/B)
    {0x416, 0x20000800, 0x08000000, 0x08020000, p_256, F_NO_ME},
    // STM32L1xxx6(8/B)A
    {0x429, 0x20001000, 0x08000000, 0x08020000, p_256, 0},
    // STM32L1xxxC
    {0x427, 0x20001000, 0x08000000, 0x08040000, p_256, 0},
    // STM32L1xxxD
    {0x436, 0x20001000, 0x08000000, 0x08060000, p_256, 0},
    // STM32L1xxxE
    {0x437, 0x20001000, 0x08000000, 0x08080000, p_256, F_NO_ME},
    /* L4 */
    // STM32L476xx/486xx
    {0x415, 0x20003100, 0x08000000, 0x08100000, p_2k, 0},
    /* These are not (yet) in AN2606: */
    // Medium_Density PL
    {0x641, 0x20000200, 0x08000000, 0x08020000, p_1k, 0},
    // STM32W-128K
    {0x9a8, 0x20000200, 0x08000000, 0x08020000, p_1k, 0},
    // STM32W-256K
    {0x9b0, 0x20000200, 0x08000000, 0x08040000, p_2k, 0},
    {0x0, 0x0, 0x0, 0x0, nullptr, 0x0},
};

}  // namespace esphome::shelly_dimmer

#endif  // USE_SHD_FIRMWARE_DATA
