#include "debug_component.h"
#ifdef USE_ZEPHYR
#include <climits>
#include "esphome/core/log.h"
#include <esphome/components/zephyr/reset_reason.h>
#include <zephyr/drivers/hwinfo.h>
#include <hal/nrf_power.h>
#include <cstdint>
#include <zephyr/storage/flash_map.h>

#define BOOTLOADER_VERSION_REGISTER NRF_TIMER2->CC[0]

namespace esphome::debug {

static const char *const TAG = "debug";
constexpr std::uintptr_t MBR_PARAM_PAGE_ADDR = 0xFFC;
constexpr std::uintptr_t MBR_BOOTLOADER_ADDR = 0xFF8;

static inline uint32_t read_mem_u32(uintptr_t addr) {
  // NOLINTNEXTLINE(performance-no-int-to-ptr,clang-analyzer-core.FixedAddressDereference)
  return *reinterpret_cast<volatile uint32_t *>(addr);
}

static inline uint8_t read_mem_u8(uintptr_t addr) {
  // NOLINTNEXTLINE(performance-no-int-to-ptr,clang-analyzer-core.FixedAddressDereference)
  return *reinterpret_cast<volatile uint8_t *>(addr);
}

// defines from https://github.com/adafruit/Adafruit_nRF52_Bootloader which prints those information
constexpr uint32_t SD_MAGIC_NUMBER = 0x51B1E5DB;
constexpr uintptr_t MBR_SIZE = 0x1000;
constexpr uintptr_t SOFTDEVICE_INFO_STRUCT_OFFSET = 0x2000;
constexpr uintptr_t SD_ID_OFFSET = SOFTDEVICE_INFO_STRUCT_OFFSET + 0x10;
constexpr uintptr_t SD_VERSION_OFFSET = SOFTDEVICE_INFO_STRUCT_OFFSET + 0x14;

static inline bool is_sd_present() {
  return read_mem_u32(SOFTDEVICE_INFO_STRUCT_OFFSET + MBR_SIZE + 4) == SD_MAGIC_NUMBER;
}
static inline uint32_t sd_id_get() {
  if (read_mem_u8(MBR_SIZE + SOFTDEVICE_INFO_STRUCT_OFFSET) > (SD_ID_OFFSET - SOFTDEVICE_INFO_STRUCT_OFFSET)) {
    return read_mem_u32(MBR_SIZE + SD_ID_OFFSET);
  }
  return 0;
}
static inline uint32_t sd_version_get() {
  if (read_mem_u8(MBR_SIZE + SOFTDEVICE_INFO_STRUCT_OFFSET) > (SD_VERSION_OFFSET - SOFTDEVICE_INFO_STRUCT_OFFSET)) {
    return read_mem_u32(MBR_SIZE + SD_VERSION_OFFSET);
  }
  return 0;
}

const char *DebugComponent::get_reset_reason_(std::span<char, RESET_REASON_BUFFER_SIZE> buffer) {
  const char *buf = zephyr::get_reset_reason(buffer);
  ESP_LOGD(TAG, "Reset Reason: %s", buf);
  return buf;
}

const char *DebugComponent::get_wakeup_cause_(std::span<char, WAKEUP_CAUSE_BUFFER_SIZE> buffer) {
  // Zephyr doesn't have detailed wakeup cause like ESP32
  return "";
}

uint32_t DebugComponent::get_free_heap_() { return INT_MAX; }

static void fa_cb(const struct flash_area *fa, void *user_data) {
#if CONFIG_FLASH_MAP_LABELS
  const char *fa_label = flash_area_label(fa);

  if (fa_label == nullptr) {
    fa_label = "-";
  }
  ESP_LOGCONFIG(TAG, "%2d   0x%0*" PRIxPTR "   %-26s  %-24.24s  0x%-10x 0x%-12x", (int) fa->fa_id,
                sizeof(uintptr_t) * 2, (uintptr_t) fa->fa_dev, fa->fa_dev->name, fa_label, (uint32_t) fa->fa_off,
                fa->fa_size);
#else
  ESP_LOGCONFIG(TAG, "%2d   0x%0*" PRIxPTR "   %-26s  0x%-10x 0x%-12x", (int) fa->fa_id, sizeof(uintptr_t) * 2,
                (uintptr_t) fa->fa_dev, fa->fa_dev->name, (uint32_t) fa->fa_off, fa->fa_size);
#endif
}

void DebugComponent::log_partition_info_() {
#if CONFIG_FLASH_MAP_LABELS
  ESP_LOGCONFIG(TAG, "ID | Device     | Device Name               "
                     "| Label                   | Offset     | Size");
  ESP_LOGCONFIG(TAG, "--------------------------------------------"
                     "-----------------------------------------------");
#else
  ESP_LOGCONFIG(TAG, "ID | Device     | Device Name               "
                     "| Offset     | Size");
  ESP_LOGCONFIG(TAG, "-----------------------------------------"
                     "------------------------------");
#endif
  flash_area_foreach(fa_cb, nullptr);
}

#ifdef ESPHOME_LOG_HAS_VERBOSE
// Check if an nRF peripheral's ENABLE register indicates it is enabled.
// periph: peripheral register prefix (e.g. USBD, UARTE, SPI)
// reg: register block pointer (e.g. NRF_USBD, NRF_UARTE0)
#define NRF_PERIPH_ENABLED(periph, reg) \
  YESNO(((reg)->ENABLE & periph##_ENABLE_ENABLE_Msk) == (periph##_ENABLE_ENABLE_Enabled << periph##_ENABLE_ENABLE_Pos))

// NOLINTBEGIN(clang-analyzer-core.FixedAddressDereference) -- nRF peripheral registers are MMIO at fixed addresses
static void log_peripherals_info() {
  // most peripherals are enabled only when in use so ESP_LOGV is enough
  ESP_LOGV(TAG, "Peripherals status:");
  ESP_LOGV(TAG, "  USBD:  %-3s| UARTE0: %-3s| UARTE1: %-3s| UART0: %-3s",  //
           NRF_PERIPH_ENABLED(USBD, NRF_USBD), NRF_PERIPH_ENABLED(UARTE, NRF_UARTE0),
           NRF_PERIPH_ENABLED(UARTE, NRF_UARTE1), NRF_PERIPH_ENABLED(UART, NRF_UART0));
  ESP_LOGV(TAG, "  TWIS0: %-3s| TWIS1:  %-3s| TWIM0:  %-3s| TWIM1: %-3s",  //
           NRF_PERIPH_ENABLED(TWIS, NRF_TWIS0), NRF_PERIPH_ENABLED(TWIS, NRF_TWIS1),
           NRF_PERIPH_ENABLED(TWIM, NRF_TWIM0), NRF_PERIPH_ENABLED(TWIM, NRF_TWIM1));
  ESP_LOGV(TAG, "  TWI0:  %-3s| TWI1:   %-3s| COMP:   %-3s| CCM:   %-3s",  //
           NRF_PERIPH_ENABLED(TWI, NRF_TWI0), NRF_PERIPH_ENABLED(TWI, NRF_TWI1), NRF_PERIPH_ENABLED(COMP, NRF_COMP),
           NRF_PERIPH_ENABLED(CCM, NRF_CCM));
  ESP_LOGV(TAG, "  PDM:   %-3s| SPIS0:  %-3s| SPIS1:  %-3s| SPIS2: %-3s",  //
           NRF_PERIPH_ENABLED(PDM, NRF_PDM), NRF_PERIPH_ENABLED(SPIS, NRF_SPIS0), NRF_PERIPH_ENABLED(SPIS, NRF_SPIS1),
           NRF_PERIPH_ENABLED(SPIS, NRF_SPIS2));
  ESP_LOGV(TAG, "  SPIM0: %-3s| SPIM1:  %-3s| SPIM2:  %-3s| SPIM3: %-3s",  //
           NRF_PERIPH_ENABLED(SPIM, NRF_SPIM0), NRF_PERIPH_ENABLED(SPIM, NRF_SPIM1),
           NRF_PERIPH_ENABLED(SPIM, NRF_SPIM2), NRF_PERIPH_ENABLED(SPIM, NRF_SPIM3));
  ESP_LOGV(TAG, "  SPI0:  %-3s| SPI1:   %-3s| SPI2:   %-3s| SAADC: %-3s",  //
           NRF_PERIPH_ENABLED(SPI, NRF_SPI0), NRF_PERIPH_ENABLED(SPI, NRF_SPI1), NRF_PERIPH_ENABLED(SPI, NRF_SPI2),
           NRF_PERIPH_ENABLED(SAADC, NRF_SAADC));
  ESP_LOGV(TAG, "  QSPI:  %-3s| QDEC:   %-3s| LPCOMP: %-3s| I2S:   %-3s",  //
           NRF_PERIPH_ENABLED(QSPI, NRF_QSPI), NRF_PERIPH_ENABLED(QDEC, NRF_QDEC),
           NRF_PERIPH_ENABLED(LPCOMP, NRF_LPCOMP), NRF_PERIPH_ENABLED(I2S, NRF_I2S));
  ESP_LOGV(TAG, "  PWM0:  %-3s| PWM1:   %-3s| PWM2:   %-3s| PWM3:  %-3s",  //
           NRF_PERIPH_ENABLED(PWM, NRF_PWM0), NRF_PERIPH_ENABLED(PWM, NRF_PWM1), NRF_PERIPH_ENABLED(PWM, NRF_PWM2),
           NRF_PERIPH_ENABLED(PWM, NRF_PWM3));
  ESP_LOGV(TAG, "  AAR:   %-3s| QSPI deep power-down:%-3s| CRYPTOCELL: %-3s", NRF_PERIPH_ENABLED(AAR, NRF_AAR),
           YESNO((NRF_QSPI->IFCONFIG0 & QSPI_IFCONFIG0_DPMENABLE_Msk) ==
                 (QSPI_IFCONFIG0_DPMENABLE_Enable << QSPI_IFCONFIG0_DPMENABLE_Pos)),
           YESNO((NRF_CRYPTOCELL->ENABLE & CRYPTOCELL_ENABLE_ENABLE_Msk) ==
                 (CRYPTOCELL_ENABLE_ENABLE_Enabled << CRYPTOCELL_ENABLE_ENABLE_Pos)));
}
// NOLINTEND(clang-analyzer-core.FixedAddressDereference)
#undef NRF_PERIPH_ENABLED
#endif

static const char *regout0_to_str(uint32_t value) {
  switch (value) {
    case (UICR_REGOUT0_VOUT_DEFAULT):
      return "1.8V (default)";
    case (UICR_REGOUT0_VOUT_1V8):
      return "1.8V";
    case (UICR_REGOUT0_VOUT_2V1):
      return "2.1V";
    case (UICR_REGOUT0_VOUT_2V4):
      return "2.4V";
    case (UICR_REGOUT0_VOUT_2V7):
      return "2.7V";
    case (UICR_REGOUT0_VOUT_3V0):
      return "3.0V";
    case (UICR_REGOUT0_VOUT_3V3):
      return "3.3V";
  }
  return "???V";
}

size_t DebugComponent::get_device_info_(std::span<char, DEVICE_INFO_BUFFER_SIZE> buffer, size_t pos) {
  constexpr size_t size = DEVICE_INFO_BUFFER_SIZE;
  char *buf = buffer.data();

  // Main supply status
  // NOLINTNEXTLINE(clang-analyzer-core.FixedAddressDereference) -- NRF_POWER is MMIO at a fixed address
  auto regstatus = nrf_power_mainregstatus_get(NRF_POWER);
  const char *supply_status = (regstatus == NRF_POWER_MAINREGSTATUS_NORMAL) ? "Normal voltage." : "High voltage.";
  ESP_LOGD(TAG, "Main supply status: %s", supply_status);
  pos = buf_append_str(buf, size, pos, "|Main supply status: ");
  pos = buf_append_str(buf, size, pos, supply_status);

  // Regulator stage 0
  if (nrf_power_mainregstatus_get(NRF_POWER) == NRF_POWER_MAINREGSTATUS_HIGH) {
    const char *reg0_type = nrf_power_dcdcen_vddh_get(NRF_POWER) ? "DC/DC" : "LDO";
    const char *reg0_voltage = regout0_to_str((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) >> UICR_REGOUT0_VOUT_Pos);
    ESP_LOGD(TAG, "Regulator stage 0: %s, %s", reg0_type, reg0_voltage);
    pos = buf_append_str(buf, size, pos, "|Regulator stage 0: ");
    pos = buf_append_str(buf, size, pos, reg0_type);
    pos = buf_append_str(buf, size, pos, ", ");
    pos = buf_append_str(buf, size, pos, reg0_voltage);
#ifdef USE_NRF52_REG0_VOUT
    if ((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) >> UICR_REGOUT0_VOUT_Pos != USE_NRF52_REG0_VOUT) {
      ESP_LOGE(TAG, "Regulator stage 0: expected %s", regout0_to_str(USE_NRF52_REG0_VOUT));
    }
#endif
  } else {
    ESP_LOGD(TAG, "Regulator stage 0: disabled");
    pos = buf_append_str(buf, size, pos, "|Regulator stage 0: disabled");
  }

  // Regulator stage 1
  const char *reg1_type = nrf_power_dcdcen_get(NRF_POWER) ? "DC/DC" : "LDO";
  ESP_LOGD(TAG, "Regulator stage 1: %s", reg1_type);
  pos = buf_append_str(buf, size, pos, "|Regulator stage 1: ");
  pos = buf_append_str(buf, size, pos, reg1_type);

  // USB power state
  const char *usb_state;
  if (nrf_power_usbregstatus_vbusdet_get(NRF_POWER)) {
    if (nrf_power_usbregstatus_outrdy_get(NRF_POWER)) {
      usb_state = "ready";
    } else {
      usb_state = "connected (regulator is not ready)";
    }
  } else {
    usb_state = "disconnected";
  }
  ESP_LOGD(TAG, "USB power state: %s", usb_state);
  pos = buf_append_str(buf, size, pos, "|USB power state: ");
  pos = buf_append_str(buf, size, pos, usb_state);

  // Power-fail comparator
  bool enabled;
  nrf_power_pof_thr_t pof_thr = nrf_power_pofcon_get(NRF_POWER, &enabled);
  if (enabled) {
    const char *pof_voltage;
    switch (pof_thr) {
      case POWER_POFCON_THRESHOLD_V17:
        pof_voltage = "1.7V";
        break;
      case POWER_POFCON_THRESHOLD_V18:
        pof_voltage = "1.8V";
        break;
      case POWER_POFCON_THRESHOLD_V19:
        pof_voltage = "1.9V";
        break;
      case POWER_POFCON_THRESHOLD_V20:
        pof_voltage = "2.0V";
        break;
      case POWER_POFCON_THRESHOLD_V21:
        pof_voltage = "2.1V";
        break;
      case POWER_POFCON_THRESHOLD_V22:
        pof_voltage = "2.2V";
        break;
      case POWER_POFCON_THRESHOLD_V23:
        pof_voltage = "2.3V";
        break;
      case POWER_POFCON_THRESHOLD_V24:
        pof_voltage = "2.4V";
        break;
      case POWER_POFCON_THRESHOLD_V25:
        pof_voltage = "2.5V";
        break;
      case POWER_POFCON_THRESHOLD_V26:
        pof_voltage = "2.6V";
        break;
      case POWER_POFCON_THRESHOLD_V27:
        pof_voltage = "2.7V";
        break;
      case POWER_POFCON_THRESHOLD_V28:
        pof_voltage = "2.8V";
        break;
      default:
        pof_voltage = "???V";
        break;
    }

    if (nrf_power_mainregstatus_get(NRF_POWER) == NRF_POWER_MAINREGSTATUS_HIGH) {
      const char *vddh_voltage;
      switch (nrf_power_pofcon_vddh_get(NRF_POWER)) {
        case NRF_POWER_POFTHRVDDH_V27:
          vddh_voltage = "2.7V";
          break;
        case NRF_POWER_POFTHRVDDH_V28:
          vddh_voltage = "2.8V";
          break;
        case NRF_POWER_POFTHRVDDH_V29:
          vddh_voltage = "2.9V";
          break;
        case NRF_POWER_POFTHRVDDH_V30:
          vddh_voltage = "3.0V";
          break;
        case NRF_POWER_POFTHRVDDH_V31:
          vddh_voltage = "3.1V";
          break;
        case NRF_POWER_POFTHRVDDH_V32:
          vddh_voltage = "3.2V";
          break;
        case NRF_POWER_POFTHRVDDH_V33:
          vddh_voltage = "3.3V";
          break;
        case NRF_POWER_POFTHRVDDH_V34:
          vddh_voltage = "3.4V";
          break;
        case NRF_POWER_POFTHRVDDH_V35:
          vddh_voltage = "3.5V";
          break;
        case NRF_POWER_POFTHRVDDH_V36:
          vddh_voltage = "3.6V";
          break;
        case NRF_POWER_POFTHRVDDH_V37:
          vddh_voltage = "3.7V";
          break;
        case NRF_POWER_POFTHRVDDH_V38:
          vddh_voltage = "3.8V";
          break;
        case NRF_POWER_POFTHRVDDH_V39:
          vddh_voltage = "3.9V";
          break;
        case NRF_POWER_POFTHRVDDH_V40:
          vddh_voltage = "4.0V";
          break;
        case NRF_POWER_POFTHRVDDH_V41:
          vddh_voltage = "4.1V";
          break;
        case NRF_POWER_POFTHRVDDH_V42:
          vddh_voltage = "4.2V";
          break;
        default:
          vddh_voltage = "???V";
          break;
      }
      ESP_LOGD(TAG, "Power-fail comparator: %s, VDDH: %s", pof_voltage, vddh_voltage);
      pos = buf_append_str(buf, size, pos, "|Power-fail comparator: ");
      pos = buf_append_str(buf, size, pos, pof_voltage);
      pos = buf_append_str(buf, size, pos, ", VDDH: ");
      pos = buf_append_str(buf, size, pos, vddh_voltage);
    } else {
      ESP_LOGD(TAG, "Power-fail comparator: %s", pof_voltage);
      pos = buf_append_str(buf, size, pos, "|Power-fail comparator: ");
      pos = buf_append_str(buf, size, pos, pof_voltage);
    }
  } else {
    ESP_LOGD(TAG, "Power-fail comparator: disabled");
    pos = buf_append_str(buf, size, pos, "|Power-fail comparator: disabled");
  }

  auto package = [](uint32_t value) {
    switch (value) {
      case 0x2004:
        return "QIxx - 7x7 73-pin aQFN";
      case 0x2000:
        return "QFxx - 6x6 48-pin QFN";
      case 0x2005:
        return "CKxx - 3.544 x 3.607 WLCSP";
    }
    return "Unspecified";
  };

  char mac_pretty[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  get_mac_address_pretty_into_buffer(mac_pretty);
  ESP_LOGD(TAG,
           "nRF debug info:\n"
           "  Code page size: %u, code size: %u, device id: 0x%08x%08x\n"
           "  Encryption root: 0x%08x%08x%08x%08x, Identity Root: 0x%08x%08x%08x%08x\n"
           "  Device address type: %s, address: %s\n"
           "  Part code: nRF%x, version: %c%c%c%c, package: %s\n"
           "  RAM: %ukB, Flash: %ukB, production test: %sdone",
           NRF_FICR->CODEPAGESIZE, NRF_FICR->CODESIZE, NRF_FICR->DEVICEID[1], NRF_FICR->DEVICEID[0], NRF_FICR->ER[0],
           NRF_FICR->ER[1], NRF_FICR->ER[2], NRF_FICR->ER[3], NRF_FICR->IR[0], NRF_FICR->IR[1], NRF_FICR->IR[2],
           NRF_FICR->IR[3], (NRF_FICR->DEVICEADDRTYPE & 0x1 ? "Random" : "Public"), mac_pretty, NRF_FICR->INFO.PART,
           NRF_FICR->INFO.VARIANT >> 24 & 0xFF, NRF_FICR->INFO.VARIANT >> 16 & 0xFF, NRF_FICR->INFO.VARIANT >> 8 & 0xFF,
           NRF_FICR->INFO.VARIANT & 0xFF, package(NRF_FICR->INFO.PACKAGE), NRF_FICR->INFO.RAM, NRF_FICR->INFO.FLASH,
           (NRF_FICR->PRODTEST[0] == 0xBB42319F ? "" : "not "));
  bool n_reset_enabled = NRF_UICR->PSELRESET[0] == NRF_UICR->PSELRESET[1] &&
                         (NRF_UICR->PSELRESET[0] & UICR_PSELRESET_CONNECT_Msk) == UICR_PSELRESET_CONNECT_Connected
                                                                                      << UICR_PSELRESET_CONNECT_Pos;
  ESP_LOGD(
      TAG, "  GPIO as NFC pins: %s, GPIO as nRESET pin: %s",
      YESNO((NRF_UICR->NFCPINS & UICR_NFCPINS_PROTECT_Msk) == (UICR_NFCPINS_PROTECT_NFC << UICR_NFCPINS_PROTECT_Pos)),
      YESNO(n_reset_enabled));
  if (n_reset_enabled) {
    uint8_t port = (NRF_UICR->PSELRESET[0] & UICR_PSELRESET_PORT_Msk) >> UICR_PSELRESET_PORT_Pos;
    uint8_t pin = (NRF_UICR->PSELRESET[0] & UICR_PSELRESET_PIN_Msk) >> UICR_PSELRESET_PIN_Pos;
    ESP_LOGD(TAG, "  nRESET port P%u.%02u", port, pin);
  }
#ifdef USE_BOOTLOADER_MCUBOOT
  ESP_LOGD(TAG, "  Bootloader: mcuboot");
#else
  ESP_LOGD(TAG, "  Bootloader: Adafruit, version %u.%u.%u", (BOOTLOADER_VERSION_REGISTER >> 16) & 0xFF,
           (BOOTLOADER_VERSION_REGISTER >> 8) & 0xFF, BOOTLOADER_VERSION_REGISTER & 0xFF);
  ESP_LOGD(TAG, "  MBR bootloader addr 0x%08x, UICR bootloader addr 0x%08x", read_mem_u32(MBR_BOOTLOADER_ADDR),
           NRF_UICR->NRFFW[0]);
  ESP_LOGD(TAG, "  MBR param page addr 0x%08x, UICR param page addr 0x%08x", read_mem_u32(MBR_PARAM_PAGE_ADDR),
           NRF_UICR->NRFFW[1]);
  if (is_sd_present()) {
    uint32_t const sd_id = sd_id_get();
    uint32_t const sd_version = sd_version_get();

    uint32_t ver[3];
    ver[0] = sd_version / 1000000;
    ver[1] = (sd_version - ver[0] * 1000000) / 1000;
    ver[2] = (sd_version - ver[0] * 1000000 - ver[1] * 1000);

    ESP_LOGD(TAG, "  SoftDevice: S%u %u.%u.%u", sd_id, ver[0], ver[1], ver[2]);
#ifdef USE_SOFTDEVICE_ID
#ifdef USE_SOFTDEVICE_VERSION
    if (USE_SOFTDEVICE_ID != sd_id || USE_SOFTDEVICE_VERSION != ver[0]) {
      ESP_LOGE(TAG, "Built for SoftDevice S%u %u.x.y. It may crash due to mismatch of bootloader version.",
               USE_SOFTDEVICE_ID, USE_SOFTDEVICE_VERSION);
    }
#else
    if (USE_SOFTDEVICE_ID != sd_id) {
      ESP_LOGE(TAG, "Built for SoftDevice S%u. It may crash due to mismatch of bootloader version.", USE_SOFTDEVICE_ID);
    }
#endif
#endif
  }
#endif
  auto uicr = [](volatile uint32_t *data, uint8_t size) {
    std::string res;
    for (size_t i = 0; i < size; i++) {
      if (i > 0) {
        res += ' ';
      }
      res += format_hex_pretty<uint32_t>(data[i], '\0', false);
    }
    return res;
  };
  ESP_LOGD(TAG, "  NRFFW %s", uicr(NRF_UICR->NRFFW, 13).c_str());
  ESP_LOGD(TAG, "  NRFHW %s", uicr(NRF_UICR->NRFHW, 12).c_str());
#ifdef ESPHOME_LOG_HAS_VERBOSE
  log_peripherals_info();
#endif
  return pos;
}

void DebugComponent::update_platform_() {}

}  // namespace esphome::debug
#endif
