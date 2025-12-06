#include "deep_sleep_component.h"
#ifdef USE_ZEPHYR
#include "esphome/core/log.h"
#include <zephyr/pm/pm.h>
#include <zephyr/pm/policy.h>
#include <zephyr/pm/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/counter.h>
#include <hal/nrf_rtc.h>

namespace esphome::deep_sleep {

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
                YESNO((NRF_QSPI->IFCONFIG0 & QSPI_IFCONFIG0_DPMENABLE_Msk) != 0),
                YESNO((NRF_CRYPTOCELL->ENABLE & CRYPTOCELL_ENABLE_ENABLE_Msk) == CRYPTOCELL_ENABLE_ENABLE_Enabled));
}

bool DeepSleepComponent::prepare_to_sleep_() { return true; }

static void setup_rtc_wakeup(uint32_t sleep_us) {
  // Use RTC2 for wakeup timer (RTC0 is used by SoftDevice if BLE is enabled, RTC1 by Zephyr kernel)
  // Configure RTC2 to wake from System OFF

  // Stop RTC if running
  nrf_rtc_task_trigger(NRF_RTC2, NRF_RTC_TASK_STOP);
  nrf_rtc_task_trigger(NRF_RTC2, NRF_RTC_TASK_CLEAR);

  // RTC frequency is 32.768 kHz, so 1 tick = 30.517 us
  // Convert microseconds to RTC ticks: ticks = us / 30.517 = us * 32768 / 1000000
  uint64_t ticks = ((uint64_t) sleep_us * 32768ULL) / 1000000ULL;

  // RTC counter is 24-bit, max value is 0xFFFFFF (approximately 512 seconds)
  if (ticks > 0xFFFFFF) {
    ESP_LOGW(TAG, "Sleep duration too long for RTC (max ~512s), capping to maximum");
    ticks = 0xFFFFFF;
  }

  ESP_LOGD(TAG, "RTC wakeup in %" PRIu32 " us (%" PRIu32 " ticks)", sleep_us, (uint32_t) ticks);

  // Set compare register for wakeup
  nrf_rtc_cc_set(NRF_RTC2, 0, (uint32_t) ticks);

  // Enable compare event and interrupt
  nrf_rtc_event_clear(NRF_RTC2, NRF_RTC_EVENT_COMPARE_0);
  nrf_rtc_int_enable(NRF_RTC2, NRF_RTC_INT_COMPARE0_MASK);

  // Configure RTC prescaler (default 0 means 32.768 kHz)
  nrf_rtc_prescaler_set(NRF_RTC2, 0);

  // Start RTC
  nrf_rtc_task_trigger(NRF_RTC2, NRF_RTC_TASK_START);

  ESP_LOGD(TAG, "RTC2 configured for wakeup");
}

void DeepSleepComponent::deep_sleep_() {
  ESP_LOGI(TAG, "Entering deep sleep");

  if (this->sleep_duration_.has_value()) {
    uint32_t sleep_us = *this->sleep_duration_;
    ESP_LOGI(TAG, "Sleep duration: %" PRIu32 " us (%.2f seconds)", sleep_us, sleep_us / 1000000.0f);

    // Configure RTC timer for wakeup
    setup_rtc_wakeup(sleep_us);

    // Small delay to ensure RTC is running
    k_usleep(1000);

    // Enter System OFF mode - will wake on RTC compare event
    // Note: PM_STATE_SOFT_OFF on NRF52 = System OFF mode
    // Only GPIO DETECT signal and COMPARE event from RTC can wake the system
    ESP_LOGD(TAG, "Entering System OFF with RTC wakeup");
    pm_state_force(0u, &(struct pm_state_info){PM_STATE_SOFT_OFF, 0, 0});

    // If we return here, sleep failed
    ESP_LOGE(TAG, "Failed to enter deep sleep mode");
  } else {
    // Indefinite sleep - enter System OFF without wakeup timer
    // Only GPIO or reset can wake the system
    ESP_LOGI(TAG, "Entering indefinite deep sleep (System OFF)");
    ESP_LOGW(TAG, "No wakeup source configured - only reset or GPIO will wake device");

    pm_state_force(0u, &(struct pm_state_info){PM_STATE_SOFT_OFF, 0, 0});

    // If we return here, sleep failed
    ESP_LOGE(TAG, "Failed to enter deep sleep mode");
  }
}

}  // namespace esphome::deep_sleep
#endif  // USE_ZEPHYR
