import esphome.codegen as cg
from esphome.components.nrf52 import TESTING_FLASH_SIZE
from esphome.components.nrf52.boards import BOARDS_ZEPHYR, BOOTLOADER_CONFIG
from esphome.components.nrf52.const import FLASH_SECTOR_SIZE, VARIANT_FLASH_SIZE
from esphome.components.nrf52.framework import get_nrf52_variant
from esphome.components.zephyr import (
    Section,
    zephyr_add_overlay,
    zephyr_add_pm_static,
    zephyr_add_prj_conf,
    zephyr_data,
)
from esphome.components.zephyr.const import (
    BOOTLOADER_MCUBOOT,
    KEY_BOARD,
    KEY_BOOTLOADER,
    KEY_PM_STATIC,
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

CONFIG_SCHEMA = cv.All(
    cv.Schema({cv.GenerateID(): cv.declare_id(Coredump)}).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_nrf52,
)


def _framework_version() -> cv.Version:
    return CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]


def _partition_address() -> int:
    """Place the partition below the lowest section reserved above the SoftDevice
    (the bootloader), or at the end of flash with MCUboot, on an erase page boundary."""
    data = zephyr_data()
    # The SoftDevice (or MBR) is the only section that can start at 0
    reserved = [s.address for s in data[KEY_PM_STATIC] if s.address > 0]
    if reserved:
        return (min(reserved) - PARTITION_SIZE) // FLASH_SECTOR_SIZE * FLASH_SECTOR_SIZE
    if (bootloader := data[KEY_BOOTLOADER]) != BOOTLOADER_MCUBOOT:
        raise cv.Invalid(
            f"{DOMAIN} does not know the flash layout of bootloader '{bootloader}', "
            f"select one of: {', '.join(BOOTLOADER_CONFIG)}"
        )
    # MCUboot reserves nothing, so the flash size has to come from the board
    board = data[KEY_BOARD]
    # Boards that do not name their SoC are taken as an nRF52840, so only trust the known ones
    if board not in BOARDS_ZEPHYR and not any(
        variant.lower() in board for variant in VARIANT_FLASH_SIZE
    ):
        known = " or ".join(f"n{v[1:]}" for v in VARIANT_FLASH_SIZE)
        raise cv.Invalid(
            f"{DOMAIN} does not know the flash size of board '{board}', "
            f"only {known} boards are supported"
        )
    if CORE.testing_mode:
        return TESTING_FLASH_SIZE - PARTITION_SIZE
    variant = get_nrf52_variant()
    if (flash_size := VARIANT_FLASH_SIZE.get(variant)) is None:
        raise cv.Invalid(f"{DOMAIN} does not know the flash size of the {variant}")
    return flash_size - PARTITION_SIZE


def _final_validate(config: ConfigType) -> ConfigType:
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
    end = addr + PARTITION_SIZE
    sections = [Section("coredump_partition", addr, PARTITION_SIZE, "flash_primary")]
    # Reserve what page alignment left below the next section, so the app keeps one gap
    above = min(
        (s.address for s in zephyr_data()[KEY_PM_STATIC] if s.address >= end),
        default=end,
    )
    if above > end:
        sections.append(Section("coredump_padding", end, above - end, "flash_primary"))
    zephyr_add_pm_static(sections)
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
