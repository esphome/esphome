#include "debug_component.h"
#ifdef USE_ZEPHYR
#include <climits>
#include "esphome/core/log.h"
#include <zephyr/drivers/hwinfo.h>
#include <hal/nrf_power.h>
#include <cstdint>
#include <zephyr/storage/flash_map.h>

#define BOOTLOADER_VERSION_REGISTER NRF_TIMER2->CC[0]

namespace esphome::debug {

static const char *const TAG = "debug";
constexpr std::uintptr_t MBR_PARAM_PAGE_ADDR = 0xFFC;
constexpr std::uintptr_t MBR_BOOTLOADER_ADDR = 0xFF8;

static void show_reset_reason(std::string &reset_reason, bool set, const char *reason) {
  if (!set) {
    return;
  }
  if (!reset_reason.empty()) {
    reset_reason += ", ";
  }
  reset_reason += reason;
}

static inline uint32_t read_mem_u32(uintptr_t addr) {
  return *reinterpret_cast<volatile uint32_t *>(addr);  // NOLINT(performance-no-int-to-ptr)
}

static inline uint8_t read_mem_u8(uintptr_t addr) {
  return *reinterpret_cast<volatile uint8_t *>(addr);  // NOLINT(performance-no-int-to-ptr)
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

std::string DebugComponent::get_reset_reason_() {
  uint32_t cause;
  auto ret = hwinfo_get_reset_cause(&cause);
  if (ret) {
    ESP_LOGE(TAG, "Unable to get reset cause: %d", ret);
    return "";
  }
  std::string reset_reason;

  show_reset_reason(reset_reason, cause & RESET_PIN, "External pin");
  show_reset_reason(reset_reason, cause & RESET_SOFTWARE, "Software reset");
  show_reset_reason(reset_reason, cause & RESET_BROWNOUT, "Brownout (drop in voltage)");
  show_reset_reason(reset_reason, cause & RESET_POR, "Power-on reset (POR)");
  show_reset_reason(reset_reason, cause & RESET_WATCHDOG, "Watchdog timer expiration");
  show_reset_reason(reset_reason, cause & RESET_DEBUG, "Debug event");
  show_reset_reason(reset_reason, cause & RESET_SECURITY, "Security violation");
  show_reset_reason(reset_reason, cause & RESET_LOW_POWER_WAKE, "Waking up from low power mode");
  show_reset_reason(reset_reason, cause & RESET_CPU_LOCKUP, "CPU lock-up detected");
  show_reset_reason(reset_reason, cause & RESET_PARITY, "Parity error");
  show_reset_reason(reset_reason, cause & RESET_PLL, "PLL error");
  show_reset_reason(reset_reason, cause & RESET_CLOCK, "Clock error");
  show_reset_reason(reset_reason, cause & RESET_HARDWARE, "Hardware reset");
  show_reset_reason(reset_reason, cause & RESET_USER, "User reset");
  show_reset_reason(reset_reason, cause & RESET_TEMPERATURE, "Temperature reset");

  ESP_LOGD(TAG, "Reset Reason: %s", reset_reason.c_str());
  return reset_reason;
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

void DebugComponent::get_device_info_(std::string &device_info) {
  std::string supply = "Main supply status: ";
  if (nrf_power_mainregstatus_get(NRF_POWER) == NRF_POWER_MAINREGSTATUS_NORMAL) {
    supply += "Normal voltage.";
  } else {
    supply += "High voltage.";
  }
  ESP_LOGD(TAG, "%s", supply.c_str());
  device_info += "|" + supply;

  std::string reg0 = "Regulator stage 0: ";
  if (nrf_power_mainregstatus_get(NRF_POWER) == NRF_POWER_MAINREGSTATUS_HIGH) {
    reg0 += nrf_power_dcdcen_vddh_get(NRF_POWER) ? "DC/DC" : "LDO";
    reg0 += ", ";
    switch (NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) {
      case (UICR_REGOUT0_VOUT_DEFAULT << UICR_REGOUT0_VOUT_Pos):
        reg0 += "1.8V (default)";
        break;
      case (UICR_REGOUT0_VOUT_1V8 << UICR_REGOUT0_VOUT_Pos):
        reg0 += "1.8V";
        break;
      case (UICR_REGOUT0_VOUT_2V1 << UICR_REGOUT0_VOUT_Pos):
        reg0 += "2.1V";
        break;
      case (UICR_REGOUT0_VOUT_2V4 << UICR_REGOUT0_VOUT_Pos):
        reg0 += "2.4V";
        break;
      case (UICR_REGOUT0_VOUT_2V7 << UICR_REGOUT0_VOUT_Pos):
        reg0 += "2.7V";
        break;
      case (UICR_REGOUT0_VOUT_3V0 << UICR_REGOUT0_VOUT_Pos):
        reg0 += "3.0V";
        break;
      case (UICR_REGOUT0_VOUT_3V3 << UICR_REGOUT0_VOUT_Pos):
        reg0 += "3.3V";
        break;
      default:
        reg0 += "???V";
    }
  } else {
    reg0 += "disabled";
  }
  ESP_LOGD(TAG, "%s", reg0.c_str());
  device_info += "|" + reg0;

  std::string reg1 = "Regulator stage 1: ";
  reg1 += nrf_power_dcdcen_get(NRF_POWER) ? "DC/DC" : "LDO";
  ESP_LOGD(TAG, "%s", reg1.c_str());
  device_info += "|" + reg1;

  std::string usb_power = "USB power state: ";
  if (nrf_power_usbregstatus_vbusdet_get(NRF_POWER)) {
    if (nrf_power_usbregstatus_outrdy_get(NRF_POWER)) {
      /**< From the power viewpoint, USB is ready for working. */
      usb_power += "ready";
    } else {
      /**< The USB power is detected, but USB power regulator is not ready. */
      usb_power += "connected (regulator is not ready)";
    }
  } else {
    /**< No power on USB lines detected. */
    usb_power += "disconected";
  }
  ESP_LOGD(TAG, "%s", usb_power.c_str());
  device_info += "|" + usb_power;

  bool enabled;
  nrf_power_pof_thr_t pof_thr;

  pof_thr = nrf_power_pofcon_get(NRF_POWER, &enabled);
  std::string pof = "Power-fail comparator: ";
  if (enabled) {
    switch (pof_thr) {
      case POWER_POFCON_THRESHOLD_V17:
        pof += "1.7V";
        break;
      case POWER_POFCON_THRESHOLD_V18:
        pof += "1.8V";
        break;
      case POWER_POFCON_THRESHOLD_V19:
        pof += "1.9V";
        break;
      case POWER_POFCON_THRESHOLD_V20:
        pof += "2.0V";
        break;
      case POWER_POFCON_THRESHOLD_V21:
        pof += "2.1V";
        break;
      case POWER_POFCON_THRESHOLD_V22:
        pof += "2.2V";
        break;
      case POWER_POFCON_THRESHOLD_V23:
        pof += "2.3V";
        break;
      case POWER_POFCON_THRESHOLD_V24:
        pof += "2.4V";
        break;
      case POWER_POFCON_THRESHOLD_V25:
        pof += "2.5V";
        break;
      case POWER_POFCON_THRESHOLD_V26:
        pof += "2.6V";
        break;
      case POWER_POFCON_THRESHOLD_V27:
        pof += "2.7V";
        break;
      case POWER_POFCON_THRESHOLD_V28:
        pof += "2.8V";
        break;
    }

    if (nrf_power_mainregstatus_get(NRF_POWER) == NRF_POWER_MAINREGSTATUS_HIGH) {
      pof += ", VDDH: ";
      switch (nrf_power_pofcon_vddh_get(NRF_POWER)) {
        case NRF_POWER_POFTHRVDDH_V27:
          pof += "2.7V";
          break;
        case NRF_POWER_POFTHRVDDH_V28:
          pof += "2.8V";
          break;
        case NRF_POWER_POFTHRVDDH_V29:
          pof += "2.9V";
          break;
        case NRF_POWER_POFTHRVDDH_V30:
          pof += "3.0V";
          break;
        case NRF_POWER_POFTHRVDDH_V31:
          pof += "3.1V";
          break;
        case NRF_POWER_POFTHRVDDH_V32:
          pof += "3.2V";
          break;
        case NRF_POWER_POFTHRVDDH_V33:
          pof += "3.3V";
          break;
        case NRF_POWER_POFTHRVDDH_V34:
          pof += "3.4V";
          break;
        case NRF_POWER_POFTHRVDDH_V35:
          pof += "3.5V";
          break;
        case NRF_POWER_POFTHRVDDH_V36:
          pof += "3.6V";
          break;
        case NRF_POWER_POFTHRVDDH_V37:
          pof += "3.7V";
          break;
        case NRF_POWER_POFTHRVDDH_V38:
          pof += "3.8V";
          break;
        case NRF_POWER_POFTHRVDDH_V39:
          pof += "3.9V";
          break;
        case NRF_POWER_POFTHRVDDH_V40:
          pof += "4.0V";
          break;
        case NRF_POWER_POFTHRVDDH_V41:
          pof += "4.1V";
          break;
        case NRF_POWER_POFTHRVDDH_V42:
          pof += "4.2V";
          break;
      }
    }
  } else {
    pof += "disabled";
  }
  ESP_LOGD(TAG, "%s", pof.c_str());
  device_info += "|" + pof;

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

  ESP_LOGD(TAG, "Code page size: %u, code size: %u, device id: 0x%08x%08x", NRF_FICR->CODEPAGESIZE, NRF_FICR->CODESIZE,
           NRF_FICR->DEVICEID[1], NRF_FICR->DEVICEID[0]);
  ESP_LOGD(TAG, "Encryption root: 0x%08x%08x%08x%08x, Identity Root: 0x%08x%08x%08x%08x", NRF_FICR->ER[0],
           NRF_FICR->ER[1], NRF_FICR->ER[2], NRF_FICR->ER[3], NRF_FICR->IR[0], NRF_FICR->IR[1], NRF_FICR->IR[2],
           NRF_FICR->IR[3]);
  ESP_LOGD(TAG, "Device address type: %s, address: %s", (NRF_FICR->DEVICEADDRTYPE & 0x1 ? "Random" : "Public"),
           get_mac_address_pretty().c_str());
  ESP_LOGD(TAG, "Part code: nRF%x, version: %c%c%c%c, package: %s", NRF_FICR->INFO.PART,
           NRF_FICR->INFO.VARIANT >> 24 & 0xFF, NRF_FICR->INFO.VARIANT >> 16 & 0xFF, NRF_FICR->INFO.VARIANT >> 8 & 0xFF,
           NRF_FICR->INFO.VARIANT & 0xFF, package(NRF_FICR->INFO.PACKAGE));
  ESP_LOGD(TAG, "RAM: %ukB, Flash: %ukB, production test: %sdone", NRF_FICR->INFO.RAM, NRF_FICR->INFO.FLASH,
           (NRF_FICR->PRODTEST[0] == 0xBB42319F ? "" : "not "));
  bool n_reset_enabled = NRF_UICR->PSELRESET[0] == NRF_UICR->PSELRESET[1] &&
                         (NRF_UICR->PSELRESET[0] & UICR_PSELRESET_CONNECT_Msk) == UICR_PSELRESET_CONNECT_Connected
                                                                                      << UICR_PSELRESET_CONNECT_Pos;
  ESP_LOGD(
      TAG, "GPIO as NFC pins: %s, GPIO as nRESET pin: %s",
      YESNO((NRF_UICR->NFCPINS & UICR_NFCPINS_PROTECT_Msk) == (UICR_NFCPINS_PROTECT_NFC << UICR_NFCPINS_PROTECT_Pos)),
      YESNO(n_reset_enabled));
  if (n_reset_enabled) {
    uint8_t port = (NRF_UICR->PSELRESET[0] & UICR_PSELRESET_PORT_Msk) >> UICR_PSELRESET_PORT_Pos;
    uint8_t pin = (NRF_UICR->PSELRESET[0] & UICR_PSELRESET_PIN_Msk) >> UICR_PSELRESET_PIN_Pos;
    ESP_LOGD(TAG, "nRESET port P%u.%02u", port, pin);
  }
#ifdef USE_BOOTLOADER_MCUBOOT
  ESP_LOGD(TAG, "bootloader: mcuboot");
#else
  ESP_LOGD(TAG, "bootloader: Adafruit, version %u.%u.%u", (BOOTLOADER_VERSION_REGISTER >> 16) & 0xFF,
           (BOOTLOADER_VERSION_REGISTER >> 8) & 0xFF, BOOTLOADER_VERSION_REGISTER & 0xFF);
  ESP_LOGD(TAG, "MBR bootloader addr 0x%08x, UICR bootloader addr 0x%08x", read_mem_u32(MBR_BOOTLOADER_ADDR),
           NRF_UICR->NRFFW[0]);
  ESP_LOGD(TAG, "MBR param page addr 0x%08x, UICR param page addr 0x%08x", read_mem_u32(MBR_PARAM_PAGE_ADDR),
           NRF_UICR->NRFFW[1]);
  if (is_sd_present()) {
    uint32_t const sd_id = sd_id_get();
    uint32_t const sd_version = sd_version_get();

    uint32_t ver[3];
    ver[0] = sd_version / 1000000;
    ver[1] = (sd_version - ver[0] * 1000000) / 1000;
    ver[2] = (sd_version - ver[0] * 1000000 - ver[1] * 1000);

    ESP_LOGD(TAG, "SoftDevice: S%u %u.%u.%u", sd_id, ver[0], ver[1], ver[2]);
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
    char buf[sizeof(uint32_t) * 2 + 1];
    for (size_t i = 0; i < size; i++) {
      if (i > 0) {
        res += ' ';
      }
      res += format_hex_pretty<uint32_t>(data[i], '\0', false);
    }
    return res;
  };
  ESP_LOGD(TAG, "NRFFW %s", uicr(NRF_UICR->NRFFW, 13).c_str());
  ESP_LOGD(TAG, "NRFHW %s", uicr(NRF_UICR->NRFHW, 12).c_str());
}

void DebugComponent::update_platform_() {}

}  // namespace esphome::debug
#endif
