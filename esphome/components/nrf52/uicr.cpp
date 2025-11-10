#include "esphome/core/defines.h"

#ifdef USE_NRF52_REG0_VOUT
#include <zephyr/init.h>
#include <hal/nrf_power.h>
#include <zephyr/sys/printk.h>

extern "C" {
void nvmc_config(uint32_t mode);
void nvmc_wait(void);
nrfx_err_t nrfx_nvmc_uicr_erase(void);
}

namespace esphome::nrf52 {

static bool regout0_ok() {
  return (NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) == (USE_NRF52_REG0_VOUT << UICR_REGOUT0_VOUT_Pos);
}

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

static StatusFlags set_regout0() {
  /* If the board is powered from USB (high voltage mode),
   * GPIO output voltage is set to 1.8 volts by default.
   */
  printk("REGOUT0 %ld, %d\n", NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk, USE_NRF52_REG0_VOUT << UICR_REGOUT0_VOUT_Pos);
  if (!regout0_ok()) {
    nvmc_config(NVMC_CONFIG_WEN_Wen);
    NRF_UICR->REGOUT0 =
        (NRF_UICR->REGOUT0 & ~((uint32_t) UICR_REGOUT0_VOUT_Msk)) | (USE_NRF52_REG0_VOUT << UICR_REGOUT0_VOUT_Pos);
    nvmc_wait();
    nvmc_config(NVMC_CONFIG_WEN_Ren);
    printk("REGOUT0 %ld, %d\n", NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk,
           USE_NRF52_REG0_VOUT << UICR_REGOUT0_VOUT_Pos);
    return regout0_ok() ? StatusFlags::NEED_RESET : StatusFlags::NEED_ERASE;
  }
  return StatusFlags::OK;
}

static StatusFlags set_uicr() {
  StatusFlags status = StatusFlags::OK;
  status |= set_regout0();
  return status;
}

static int board_esphome_init(void) {
  printk("board_esphome_init\n");

  bool need_reset = false;
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

static int board_esphome_init(void) { return esphome::nrf52::board_esphome_init(); }

SYS_INIT(board_esphome_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

#endif
