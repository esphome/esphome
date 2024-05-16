#ifdef USE_ZEPHYR
#include "deep_sleep_component.h"
#include "esphome/core/log.h"
#include <zephyr/sys/poweroff.h>

namespace esphome {
namespace deep_sleep {

static const char *const TAG = "deep_sleep";

optional<uint32_t> DeepSleepComponent::get_run_duration_() const { return this->run_duration_; }

void DeepSleepComponent::dump_config_platform_() {
  ESP_LOGCONFIG(TAG, "Enabled peripherals:");
  ESP_LOGCONFIG(TAG, "  USBD:  %-3s| UARTE0: %-3s| UARTE1: %-3s| UART0  %-3s",
                YESNO((NRF_USBD->ENABLE & USBD_ENABLE_ENABLE_Msk) == USBD_ENABLE_ENABLE_Enabled),
                YESNO((NRF_UARTE0->ENABLE & UARTE_ENABLE_ENABLE_Msk) == UARTE_ENABLE_ENABLE_Enabled),
                YESNO((NRF_UARTE1->ENABLE & UARTE_ENABLE_ENABLE_Msk) == UARTE_ENABLE_ENABLE_Enabled),
                YESNO((NRF_UART0->ENABLE & UART_ENABLE_ENABLE_Msk) == UART_ENABLE_ENABLE_Enabled));
  ESP_LOGCONFIG(TAG, "  TWIS0: %-3s| TWIS1:  %-3s| TWIM0:  %-3s| TWIM1: %-3s",
                YESNO((NRF_TWIS0->ENABLE & TWIS_ENABLE_ENABLE_Msk) == TWIS_ENABLE_ENABLE_Enabled),
                YESNO((NRF_TWIS1->ENABLE & TWIS_ENABLE_ENABLE_Msk) == TWIS_ENABLE_ENABLE_Enabled),
                YESNO((NRF_TWIM0->ENABLE & TWIM_ENABLE_ENABLE_Msk) == TWIM_ENABLE_ENABLE_Enabled),
                YESNO((NRF_TWIM1->ENABLE & TWIM_ENABLE_ENABLE_Msk) == TWIM_ENABLE_ENABLE_Enabled));
  ESP_LOGCONFIG(TAG, "  TWI0:  %-3s| TWI1:   %-3s| COMP:   %-3s| CCM:   %-3s",
                YESNO((NRF_TWI0->ENABLE & TWI_ENABLE_ENABLE_Msk) == TWI_ENABLE_ENABLE_Enabled),
                YESNO((NRF_TWI1->ENABLE & TWI_ENABLE_ENABLE_Msk) == TWI_ENABLE_ENABLE_Enabled),
                YESNO((NRF_COMP->ENABLE & COMP_ENABLE_ENABLE_Msk) == COMP_ENABLE_ENABLE_Enabled),
                YESNO((NRF_CCM->ENABLE & CCM_ENABLE_ENABLE_Msk) == CCM_ENABLE_ENABLE_Enabled));
  ESP_LOGCONFIG(TAG, "  PDM:   %-3s| SPIS0:  %-3s| SPIS1:  %-3s| SPIS2: %-3s",
                YESNO((NRF_PDM->ENABLE & PDM_ENABLE_ENABLE_Msk) == PDM_ENABLE_ENABLE_Enabled),
                YESNO((NRF_SPIS0->ENABLE & SPIS_ENABLE_ENABLE_Msk) == SPIS_ENABLE_ENABLE_Enabled),
                YESNO((NRF_SPIS1->ENABLE & SPIS_ENABLE_ENABLE_Msk) == SPIS_ENABLE_ENABLE_Enabled),
                YESNO((NRF_SPIS2->ENABLE & SPIS_ENABLE_ENABLE_Msk) == SPIS_ENABLE_ENABLE_Enabled));
  ESP_LOGCONFIG(TAG, "  SPIM0: %-3s| SPIM1:  %-3s| SPIM2:  %-3s| SPIM3: %-3s",
                YESNO((NRF_SPIM0->ENABLE & SPIM_ENABLE_ENABLE_Msk) == SPIM_ENABLE_ENABLE_Enabled),
                YESNO((NRF_SPIM1->ENABLE & SPIM_ENABLE_ENABLE_Msk) == SPIM_ENABLE_ENABLE_Enabled),
                YESNO((NRF_SPIM2->ENABLE & SPIM_ENABLE_ENABLE_Msk) == SPIM_ENABLE_ENABLE_Enabled),
                YESNO((NRF_SPIM3->ENABLE & SPIM_ENABLE_ENABLE_Msk) == SPIM_ENABLE_ENABLE_Enabled));
  ESP_LOGCONFIG(TAG, "  SPI0:  %-3s| SPI1:   %-3s| SPI2:   %-3s| SAADC: %-3s",
                YESNO((NRF_SPI0->ENABLE & SPI_ENABLE_ENABLE_Msk) == SPI_ENABLE_ENABLE_Enabled),
                YESNO((NRF_SPI1->ENABLE & SPI_ENABLE_ENABLE_Msk) == SPI_ENABLE_ENABLE_Enabled),
                YESNO((NRF_SPI2->ENABLE & SPI_ENABLE_ENABLE_Msk) == SPI_ENABLE_ENABLE_Enabled),
                YESNO((NRF_SAADC->ENABLE & SAADC_ENABLE_ENABLE_Msk) == SAADC_ENABLE_ENABLE_Enabled));
  ESP_LOGCONFIG(TAG, "  QSPI:  %-3s| QDEC:   %-3s| LPCOMP: %-3s| I2S:   %-3s",
                YESNO((NRF_QSPI->ENABLE & QSPI_ENABLE_ENABLE_Msk) == QSPI_ENABLE_ENABLE_Enabled),
                YESNO((NRF_QDEC->ENABLE & QDEC_ENABLE_ENABLE_Msk) == QDEC_ENABLE_ENABLE_Enabled),
                YESNO((NRF_LPCOMP->ENABLE & LPCOMP_ENABLE_ENABLE_Msk) == LPCOMP_ENABLE_ENABLE_Enabled),
                YESNO((NRF_I2S->ENABLE & I2S_ENABLE_ENABLE_Msk) == I2S_ENABLE_ENABLE_Enabled));
  ESP_LOGCONFIG(TAG, "  PWM0:  %-3s| PWM1:   %-3s| PWM2:   %-3s| PWM3:  %-3s",
                YESNO((NRF_PWM0->ENABLE & PWM_ENABLE_ENABLE_Msk) == PWM_ENABLE_ENABLE_Enabled),
                YESNO((NRF_PWM1->ENABLE & PWM_ENABLE_ENABLE_Msk) == PWM_ENABLE_ENABLE_Enabled),
                YESNO((NRF_PWM2->ENABLE & PWM_ENABLE_ENABLE_Msk) == PWM_ENABLE_ENABLE_Enabled),
                YESNO((NRF_PWM3->ENABLE & PWM_ENABLE_ENABLE_Msk) == PWM_ENABLE_ENABLE_Enabled));
  ESP_LOGCONFIG(TAG, "  AAR:   %-3s| QSPI deep power-down:%-3s| CRYPTOCELL: %-3s",
                YESNO((NRF_AAR->ENABLE & AAR_ENABLE_ENABLE_Msk) == AAR_ENABLE_ENABLE_Enabled),
                YESNO((NRF_QSPI->IFCONFIG0 & QSPI_IFCONFIG0_DPMENABLE_Msk) == QSPI_IFCONFIG0_DPMENABLE_Enable),
                YESNO((NRF_CRYPTOCELL->ENABLE & CRYPTOCELL_ENABLE_ENABLE_Msk) == CRYPTOCELL_ENABLE_ENABLE_Enabled));
}

bool DeepSleepComponent::prepare_to_sleep_() { return true; }

void DeepSleepComponent::deep_sleep_() { sys_poweroff(); }

}  // namespace deep_sleep
}  // namespace esphome
#endif
