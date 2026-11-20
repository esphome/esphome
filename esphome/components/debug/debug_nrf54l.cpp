#include "debug_component.h"
#ifdef USE_ZEPHYR
#include <nrfx.h>
#endif
#if defined(USE_ZEPHYR) && defined(NRF54L_SERIES)
#include <cinttypes>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::debug {

ESPHOME_LOG_TAG(TAG, "debug");

// Check if an nRF peripheral's ENABLE register indicates it is enabled.
// periph: peripheral register prefix (e.g. USBD, UARTE, SPI)
// reg: register block pointer (e.g. NRF_USBD, NRF_UARTE0)
#define NRF_PERIPH_ENABLED(periph, reg) \
  YESNO(((reg)->ENABLE & periph##_ENABLE_ENABLE_Msk) == (periph##_ENABLE_ENABLE_Enabled << periph##_ENABLE_ENABLE_Pos))

#ifdef ESPHOME_LOG_HAS_VERBOSE
// NOLINTBEGIN(clang-analyzer-core.FixedAddressDereference) -- nRF peripheral registers are MMIO at fixed addresses
static void log_peripherals_info() {
  // most peripherals are enabled only when in use so ESP_LOGV is enough
  // UARTE, SPIM, SPIS, TWIM and TWIS with the same number share one instance
  ESP_LOGV(TAG, "Peripherals status:");
  ESP_LOGV(TAG, "  UARTE00: %-3s| UARTE20: %-3s| UARTE21: %-3s| UARTE22: %-3s| UARTE30: %-3s",  //
           NRF_PERIPH_ENABLED(UARTE, NRF_UARTE00), NRF_PERIPH_ENABLED(UARTE, NRF_UARTE20),
           NRF_PERIPH_ENABLED(UARTE, NRF_UARTE21), NRF_PERIPH_ENABLED(UARTE, NRF_UARTE22),
           NRF_PERIPH_ENABLED(UARTE, NRF_UARTE30));
  ESP_LOGV(TAG, "  SPIM00:  %-3s| SPIM20:  %-3s| SPIM21:  %-3s| SPIM22:  %-3s| SPIM30:  %-3s",  //
           NRF_PERIPH_ENABLED(SPIM, NRF_SPIM00), NRF_PERIPH_ENABLED(SPIM, NRF_SPIM20),
           NRF_PERIPH_ENABLED(SPIM, NRF_SPIM21), NRF_PERIPH_ENABLED(SPIM, NRF_SPIM22),
           NRF_PERIPH_ENABLED(SPIM, NRF_SPIM30));
  ESP_LOGV(TAG, "  SPIS00:  %-3s| SPIS20:  %-3s| SPIS21:  %-3s| SPIS22:  %-3s| SPIS30:  %-3s",  //
           NRF_PERIPH_ENABLED(SPIS, NRF_SPIS00), NRF_PERIPH_ENABLED(SPIS, NRF_SPIS20),
           NRF_PERIPH_ENABLED(SPIS, NRF_SPIS21), NRF_PERIPH_ENABLED(SPIS, NRF_SPIS22),
           NRF_PERIPH_ENABLED(SPIS, NRF_SPIS30));
  ESP_LOGV(TAG, "  TWIM20:  %-3s| TWIM21:  %-3s| TWIM22:  %-3s| TWIM30:  %-3s",  //
           NRF_PERIPH_ENABLED(TWIM, NRF_TWIM20), NRF_PERIPH_ENABLED(TWIM, NRF_TWIM21),
           NRF_PERIPH_ENABLED(TWIM, NRF_TWIM22), NRF_PERIPH_ENABLED(TWIM, NRF_TWIM30));
  ESP_LOGV(TAG, "  TWIS20:  %-3s| TWIS21:  %-3s| TWIS22:  %-3s| TWIS30:  %-3s",  //
           NRF_PERIPH_ENABLED(TWIS, NRF_TWIS20), NRF_PERIPH_ENABLED(TWIS, NRF_TWIS21),
           NRF_PERIPH_ENABLED(TWIS, NRF_TWIS22), NRF_PERIPH_ENABLED(TWIS, NRF_TWIS30));
  ESP_LOGV(TAG, "  PWM20:   %-3s| PWM21:   %-3s| PWM22:   %-3s| SAADC:   %-3s",  //
           NRF_PERIPH_ENABLED(PWM, NRF_PWM20), NRF_PERIPH_ENABLED(PWM, NRF_PWM21), NRF_PERIPH_ENABLED(PWM, NRF_PWM22),
           NRF_PERIPH_ENABLED(SAADC, NRF_SAADC));
  ESP_LOGV(TAG, "  PDM20:   %-3s| PDM21:   %-3s| QDEC20:  %-3s| QDEC21:  %-3s",  //
           NRF_PERIPH_ENABLED(PDM, NRF_PDM20), NRF_PERIPH_ENABLED(PDM, NRF_PDM21), NRF_PERIPH_ENABLED(QDEC, NRF_QDEC20),
           NRF_PERIPH_ENABLED(QDEC, NRF_QDEC21));
  ESP_LOGV(TAG, "  COMP:    %-3s| LPCOMP:  %-3s| AAR00:   %-3s| CCM00:   %-3s",  //
           NRF_PERIPH_ENABLED(COMP, NRF_COMP), NRF_PERIPH_ENABLED(LPCOMP, NRF_LPCOMP),
           NRF_PERIPH_ENABLED(AAR, NRF_AAR00), NRF_PERIPH_ENABLED(CCM, NRF_CCM00));
}
// NOLINTEND(clang-analyzer-core.FixedAddressDereference)
#endif

// NOLINTBEGIN(clang-analyzer-core.FixedAddressDereference) -- nRF registers are MMIO at fixed addresses
size_t DebugComponent::get_device_info_(std::span<char, DEVICE_INFO_BUFFER_SIZE> buffer, size_t pos) {
  constexpr size_t size = DEVICE_INFO_BUFFER_SIZE;
  char *buf = buffer.data();

  // Main regulator
  const char *reg_type = (NRF_REGULATORS->VREGMAIN.DCDCEN & REGULATORS_VREGMAIN_DCDCEN_VAL_Msk) ==
                                 (REGULATORS_VREGMAIN_DCDCEN_VAL_Enabled << REGULATORS_VREGMAIN_DCDCEN_VAL_Pos)
                             ? "DC/DC"
                             : "LDO";
  ESP_LOGD(TAG, "Main regulator: %s", reg_type);
  pos = buf_append_str(buf, size, pos, "|Main regulator: ");
  pos = buf_append_str(buf, size, pos, reg_type);

  // Power-fail comparator, the threshold goes from 1.7V up in 0.1V steps
  const uint32_t pofcon = NRF_REGULATORS->POFCON;
  if ((pofcon & REGULATORS_POFCON_POF_Msk) == (REGULATORS_POFCON_POF_Enabled << REGULATORS_POFCON_POF_Pos)) {
    const uint32_t decivolts = 17 + ((pofcon & REGULATORS_POFCON_THRESHOLD_Msk) >> REGULATORS_POFCON_THRESHOLD_Pos);
    ESP_LOGD(TAG, "Power-fail comparator: %" PRIu32 ".%" PRIu32 "V", decivolts / 10, decivolts % 10);
    pos = buf_append_printf(buf, size, pos, "|Power-fail comparator: %" PRIu32 ".%" PRIu32 "V", decivolts / 10,
                            decivolts % 10);
  } else {
    ESP_LOGD(TAG, "Power-fail comparator: disabled");
    pos = buf_append_str(buf, size, pos, "|Power-fail comparator: disabled");
  }

  char mac_pretty[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  get_mac_address_pretty_into_buffer(mac_pretty);
  const uint32_t variant = NRF_FICR->INFO.VARIANT;
  ESP_LOGD(TAG,
           "nRF debug info:\n"
           "  Device id: 0x%08" PRIx32 "%08" PRIx32 "\n"
           "  Encryption root: 0x%08" PRIx32 "%08" PRIx32 "%08" PRIx32 "%08" PRIx32 ", Identity Root: 0x%08" PRIx32
           "%08" PRIx32 "%08" PRIx32 "%08" PRIx32 "\n"
           "  Device address type: %s, address: %s\n"
           "  Part code: 0x%" PRIx32 ", version: %c%c%c%c, package: 0x%" PRIx32 "\n"
           "  RAM: %" PRIu32 "kB, RRAM: %" PRIu32 "kB",
           NRF_FICR->INFO.DEVICEID[1], NRF_FICR->INFO.DEVICEID[0], NRF_FICR->ER[0], NRF_FICR->ER[1], NRF_FICR->ER[2],
           NRF_FICR->ER[3], NRF_FICR->IR[0], NRF_FICR->IR[1], NRF_FICR->IR[2], NRF_FICR->IR[3],
           (NRF_FICR->DEVICEADDRTYPE & 0x1 ? LOG_STR_LITERAL("Random") : LOG_STR_LITERAL("Public")), mac_pretty,
           NRF_FICR->INFO.PART, static_cast<char>(variant >> 24 & 0xFF), static_cast<char>(variant >> 16 & 0xFF),
           static_cast<char>(variant >> 8 & 0xFF), static_cast<char>(variant & 0xFF), NRF_FICR->INFO.PACKAGE,
           NRF_FICR->INFO.RAM, NRF_FICR->INFO.RRAM);
#ifdef USE_BOOTLOADER_MCUBOOT
  ESP_LOGD(TAG, "  Bootloader: mcuboot");
#endif
#ifdef ESPHOME_LOG_HAS_VERBOSE
  log_peripherals_info();
#endif
  return pos;
}
// NOLINTEND(clang-analyzer-core.FixedAddressDereference)

#undef NRF_PERIPH_ENABLED

}  // namespace esphome::debug
#endif
