import esphome.codegen as cg
from esphome.components.nrf52 import NRF52840_FLASH_SIZE, TESTING_FLASH_SIZE
from esphome.components.nrf52.boards import BOARDS_ZEPHYR, BOOTLOADER_CONFIG
from esphome.components.zephyr import (
    Section,
    zephyr_add_overlay,
    zephyr_add_pm_static,
    zephyr_add_prj_conf,
    zephyr_data,
    zephyr_require_partition,
    zephyr_variant,
)
from esphome.components.zephyr.const import (
    BOOTLOADER_MCUBOOT,
    KEY_BOARD,
    KEY_BOOTLOADER,
    KEY_PM_STATIC,
    ZEPHYR_VARIANT_ESP32,
    ZEPHYR_VARIANT_ESP32_S3,
    ZEPHYR_VARIANT_NATIVE_SIM,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.types import ConfigType

CODEOWNERS = ["@tomaszduda23"]
DEPENDENCIES = ["logger"]
DOMAIN = "zephyr_coredump"

Coredump = cg.esphome_ns.namespace(DOMAIN).class_("Coredump", cg.Component)

# SDK 2.9.2 and later can store every thread's stack; older ones only the faulting thread
DUMP_THREADS_MIN_VERSION = cv.Version(2, 9, 2)
# Zephyr 4.0 (from SDK 3.0) carries the fault stack pointer fix (zephyr PR 79622)
SP_FIX_MIN_VERSION = cv.Version(3, 0, 0)
# One size for every SDK, so enabling the component moves the OTA image slots the same way everywhere
PARTITION_SIZE = 0x8000

# Xtensa has no ARCH_SUPPORTS_COREDUMP_THREADS, so only the faulting thread is stored
XTENSA_VARIANTS = (ZEPHYR_VARIANT_ESP32, ZEPHYR_VARIANT_ESP32_S3)


def _validate_platform(config: ConfigType) -> ConfigType:
    if CORE.is_nrf52:
        return config
    if not CORE.is_zephyr or zephyr_variant() == ZEPHYR_VARIANT_NATIVE_SIM:
        raise cv.Invalid(
            f"{DOMAIN} is only available on platform: nrf52 and on platform: zephyr "
            "with real hardware"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema({cv.GenerateID(): cv.declare_id(Coredump)}).extend(cv.COMPONENT_SCHEMA),
    _validate_platform,
)


def _framework_version() -> cv.Version:
    return CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]


def _partition_address() -> int:
    """Place the partition directly below the lowest section reserved above the
    SoftDevice (the bootloader), or at the end of flash with MCUboot."""
    data = zephyr_data()
    # The SoftDevice (or MBR) is the only section that can start at 0
    reserved = [s.address for s in data[KEY_PM_STATIC] if s.address > 0]
    if reserved:
        return min(reserved) - PARTITION_SIZE
    if (bootloader := data[KEY_BOOTLOADER]) != BOOTLOADER_MCUBOOT:
        raise cv.Invalid(
            f"{DOMAIN} does not know the flash layout of bootloader '{bootloader}', "
            f"select one of: {', '.join(BOOTLOADER_CONFIG)}"
        )
    # MCUboot reserves nothing, so the flash size has to come from the board
    board = data[KEY_BOARD]
    if "nrf52840" not in board and board not in BOARDS_ZEPHYR:
        raise cv.Invalid(
            f"{DOMAIN} does not know the flash size of board '{board}', "
            "only nRF52840 boards are supported"
        )
    flash_size = TESTING_FLASH_SIZE if CORE.testing_mode else NRF52840_FLASH_SIZE
    return flash_size - PARTITION_SIZE


def _final_validate(config: ConfigType) -> ConfigType:
    # platform: zephyr takes the partition from the devicetree instead
    if CORE.is_nrf52:
        _partition_address()
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


# Runs before zephyr_mcumgr OTA (OTA_UPDATES), which sizes its image slots below this partition
@coroutine_with_priority(CoroPriority.DIAGNOSTICS)
async def to_code(config: ConfigType) -> None:
    # The logger's crash handler hooks: the dump reaches serial, API and MQTT log clients
    cg.add_define("USE_CRASH_HANDLER")
    zephyr_add_prj_conf("DEBUG_COREDUMP", True)
    zephyr_add_prj_conf("DEBUG_COREDUMP_BACKEND_FLASH_PARTITION", True)
    if not CORE.is_nrf52:
        await _zephyr_to_code(config)
        return
    framework_ver = _framework_version()
    if framework_ver >= DUMP_THREADS_MIN_VERSION:
        zephyr_add_prj_conf("DEBUG_COREDUMP_MEMORY_DUMP_THREADS", True)
    else:
        zephyr_add_prj_conf("DEBUG_COREDUMP_MEMORY_DUMP_MIN", True)
    if framework_ver < SP_FIX_MIN_VERSION:
        cg.add_define("USE_ZEPHYR_COREDUMP_SP_FIX")
        cg.add_build_flag("-Wl,--wrap=z_arm_fatal_error")
        cg.add_build_flag("-Wl,--wrap=z_arm_fault")
    zephyr_add_prj_conf("PARTITION_MANAGER_ENABLED", True)
    addr = _partition_address()
    zephyr_add_pm_static(
        [Section("coredump_partition", addr, PARTITION_SIZE, "flash_primary")]
    )
    zephyr_add_overlay(
        f"""
        &flash0 {{
            partitions {{
                coredump_partition: partition@{addr:x} {{
                    label = "coredump-partition";
                    reg = <{addr:#x} {PARTITION_SIZE:#x}>;
                }};
            }};
        }};
        """
    )
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)


async def _zephyr_to_code(config: ConfigType) -> None:
    """platform: zephyr: every supported SDK is Zephyr 4.x, so the stack pointer fix is never needed,
    and the user's devicetree provides the partition."""
    if zephyr_variant() in XTENSA_VARIANTS:
        zephyr_add_prj_conf("DEBUG_COREDUMP_MEMORY_DUMP_MIN", True)
    else:
        zephyr_add_prj_conf("DEBUG_COREDUMP_MEMORY_DUMP_THREADS", True)
    zephyr_require_partition("coredump_partition", DOMAIN)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
