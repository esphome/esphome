#ifdef USE_NRF52
#include <zephyr/init.h>
#include <hal/nrf_power.h>

namespace esphome {
namespace nrf52 {

static int board_esphome_init(void) {
  /* if the board is powered from USB
   * (high voltage mode), GPIO output voltage is set to 1.8 volts by
   * default and that is not enough to turn the green and blue LEDs on.
   * Increase GPIO voltage to 3.3 volts.
   */
  if ((nrf_power_mainregstatus_get(NRF_POWER) == NRF_POWER_MAINREGSTATUS_HIGH) &&
      ((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) == (UICR_REGOUT0_VOUT_DEFAULT << UICR_REGOUT0_VOUT_Pos))) {
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
      ;
    }

    NRF_UICR->REGOUT0 =
        (NRF_UICR->REGOUT0 & ~((uint32_t) UICR_REGOUT0_VOUT_Msk)) | (UICR_REGOUT0_VOUT_3V0 << UICR_REGOUT0_VOUT_Pos);

    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {
      ;
    }
    /* a reset is required for changes to take effect */
    NVIC_SystemReset();
  }

  return 0;
}
}  // namespace nrf52
}  // namespace esphome

static int board_esphome_init(void) { return esphome::nrf52::board_esphome_init(); }

SYS_INIT(board_esphome_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

#endif
