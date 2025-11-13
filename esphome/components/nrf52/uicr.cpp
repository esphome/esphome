#include "esphome/core/defines.h"

#ifdef USE_NRF52_REG0_VOUT
#include <zephyr/init.h>
#include <hal/nrf_power.h>
#include <zephyr/sys/printk.h>

extern "C" {
void nvmc_config(uint32_t mode);
void nvmc_wait();
nrfx_err_t nrfx_nvmc_uicr_erase();
}

namespace esphome::nrf52 {

enum class StatusFlags : uint8_t {
  OK = 0x00,
  NEED_RESET = 0x01,
  NEED_ERASE = 0x02,
};

constexpr StatusFlags &operator|=(StatusFlags &a, StatusFlags b) {
  a = static_cast<StatusFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
  return a;
}

constexpr bool operator&(StatusFlags a, StatusFlags b) {
  return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

static bool regout0_ok() {
  return (NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) == (USE_NRF52_REG0_VOUT << UICR_REGOUT0_VOUT_Pos);
}

static StatusFlags set_regout0() {
  /* If the board is powered from USB (high voltage mode),
   * GPIO output voltage is set to 1.8 volts by default.
   */
  if (!regout0_ok()) {
    nvmc_config(NVMC_CONFIG_WEN_Wen);
    NRF_UICR->REGOUT0 =
        (NRF_UICR->REGOUT0 & ~((uint32_t) UICR_REGOUT0_VOUT_Msk)) | (USE_NRF52_REG0_VOUT << UICR_REGOUT0_VOUT_Pos);
    nvmc_wait();
    nvmc_config(NVMC_CONFIG_WEN_Ren);
    return regout0_ok() ? StatusFlags::NEED_RESET : StatusFlags::NEED_ERASE;
  }
  return StatusFlags::OK;
}

#ifndef USE_BOOTLOADER_MCUBOOT
// https://github.com/adafruit/Adafruit_nRF52_Bootloader/blob/6a9a6a3e6d0f86918e9286188426a279976645bd/lib/sdk11/components/libraries/bootloader_dfu/dfu_types.h#L61
constexpr uint32_t BOOTLOADER_REGION_START = 0x000F4000;
constexpr uint32_t BOOTLOADER_MBR_PARAMS_PAGE_ADDRESS = 0x000FE000;

static bool bootloader_ok() {
  return NRF_UICR->NRFFW[0] == BOOTLOADER_REGION_START && NRF_UICR->NRFFW[1] == BOOTLOADER_MBR_PARAMS_PAGE_ADDRESS;
}

static StatusFlags fix_bootloader() {
  if (!bootloader_ok()) {
    nvmc_config(NVMC_CONFIG_WEN_Wen);
    NRF_UICR->NRFFW[0] = BOOTLOADER_REGION_START;
    NRF_UICR->NRFFW[1] = BOOTLOADER_MBR_PARAMS_PAGE_ADDRESS;
    nvmc_wait();
    nvmc_config(NVMC_CONFIG_WEN_Ren);
    return bootloader_ok() ? StatusFlags::NEED_RESET : StatusFlags::NEED_ERASE;
  }
  return StatusFlags::OK;
}
#endif

#define BOOTLOADER_VERSION_REGISTER NRF_TIMER2->CC[0]

static StatusFlags set_uicr() {
  StatusFlags status = StatusFlags::OK;
#ifndef USE_BOOTLOADER_MCUBOOT
  if (BOOTLOADER_VERSION_REGISTER <= 0x902) {
#ifdef CONFIG_PRINTK
    printk("cannot control regout0 for %#x\n", BOOTLOADER_VERSION_REGISTER);
#endif
  } else
#endif
  {
    status |= set_regout0();
  }
#ifndef USE_BOOTLOADER_MCUBOOT
  status |= fix_bootloader();
#endif
  return status;
}

static int board_esphome_init() {
  StatusFlags status = set_uicr();

#ifdef USE_NRF52_UICR_ERASE
  if (status & StatusFlags::NEED_ERASE) {
    nrfx_err_t ret = nrfx_nvmc_uicr_erase();
    if (ret != NRFX_SUCCESS) {
#ifdef CONFIG_PRINTK
      printk("nrfx_nvmc_uicr_erase failed %d\n", ret);
#endif
    } else {
      status |= set_uicr();
    }
  }
#endif

  if (status & StatusFlags::NEED_RESET) {
    /* a reset is required for changes to take effect */
    NVIC_SystemReset();
  }

  return 0;
}
}  // namespace esphome::nrf52

static int board_esphome_init() { return esphome::nrf52::board_esphome_init(); }

SYS_INIT(board_esphome_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

#endif
