#ifdef USE_NRF52
#ifdef USE_NRF52_REG0_VOUT
#include <zephyr/init.h>
#include <hal/nrf_power.h>

namespace esphome::nrf52 {

static int board_esphome_init(void) {
  /* If the board is powered from USB (high voltage mode),
   * GPIO output voltage is set to 1.8 volts by default.
   */
  if ((nrf_power_mainregstatus_get(NRF_POWER) == NRF_POWER_MAINREGSTATUS_HIGH) &&
      ((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) == (UICR_REGOUT0_VOUT_DEFAULT << UICR_REGOUT0_VOUT_Pos))) {
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
      ;
    }

    NRF_UICR->REGOUT0 =
        (NRF_UICR->REGOUT0 & ~((uint32_t) UICR_REGOUT0_VOUT_Msk)) | (USE_NRF52_REG0_VOUT << UICR_REGOUT0_VOUT_Pos);

    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
      ;
    }
    /* a reset is required for changes to take effect */
    NVIC_SystemReset();
  }

  return 0;
}
}  // namespace esphome::nrf52

static int board_esphome_init(void) { return esphome::nrf52::board_esphome_init(); }

SYS_INIT(board_esphome_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

#endif
#endif
