import esphome.codegen as cg
from esphome.components.nrf52 import NRF52840_FLASH_SIZE, TESTING_FLASH_SIZE
from esphome.components.zephyr import (
    Section,
    zephyr_add_overlay,
    zephyr_add_pm_static,
    zephyr_add_prj_conf,
    zephyr_data,
)
from esphome.components.zephyr.const import KEY_BOARD, KEY_PM_STATIC
import esphome.config_validation as cv
from esphome.const import CONF_ID, KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.types import ConfigType

CODEOWNERS = ["@tomaszduda23"]
DOMAIN = "zephyr_coredump"

Coredump = cg.esphome_ns.namespace(DOMAIN).class_("Coredump", cg.Component)

# SDK 2.9.2 and later can store every thread's stack; older ones only the faulting thread
DUMP_THREADS_MIN_VERSION = cv.Version(2, 9, 2)
# Zephyr 4.0 (from SDK 3.0) carries the fault stack pointer fix (zephyr PR 79622)
SP_FIX_MIN_VERSION = cv.Version(3, 0, 0)
# Multiples of the 4 KB flash page
PARTITION_SIZE_THREADS = 0x8000
PARTITION_SIZE_MIN = 0x2000

CONFIG_SCHEMA = cv.All(
    cv.Schema({cv.GenerateID(): cv.declare_id(Coredump)}).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_nrf52,
)


def _framework_version() -> cv.Version:
    return CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]


def _partition_size() -> int:
    if _framework_version() >= DUMP_THREADS_MIN_VERSION:
        return PARTITION_SIZE_THREADS
    return PARTITION_SIZE_MIN


def _partition_address(size: int) -> int:
    """Place the partition directly below the lowest section reserved above the
    SoftDevice (the bootloader), or at the end of flash when none is reserved."""
    sections = zephyr_data()[KEY_PM_STATIC]
    # The SoftDevice (or MBR) is the only section that can start at 0
    reserved = [s.address for s in sections if s.address > 0]
    if reserved:
        return min(reserved) - size
    # Without a bootloader layout (MCUboot) the flash size comes from the board
    if "nrf52840" not in zephyr_data()[KEY_BOARD]:
        raise cv.Invalid(
            f"{DOMAIN} does not know the flash size of board "
            f"'{zephyr_data()[KEY_BOARD]}', only nRF52840 boards are supported"
        )
    return (TESTING_FLASH_SIZE if CORE.testing_mode else NRF52840_FLASH_SIZE) - size


def _final_validate(config: ConfigType) -> ConfigType:
    _partition_address(_partition_size())
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


# Runs before zephyr_mcumgr OTA (OTA_UPDATES), which sizes its image slots below this partition
@coroutine_with_priority(CoroPriority.DIAGNOSTICS)
async def to_code(config: ConfigType) -> None:
    zephyr_add_prj_conf("DEBUG_COREDUMP", True)
    zephyr_add_prj_conf("DEBUG_COREDUMP_BACKEND_FLASH_PARTITION", True)
    framework_ver = _framework_version()
    if framework_ver >= DUMP_THREADS_MIN_VERSION:
        zephyr_add_prj_conf("DEBUG_COREDUMP_MEMORY_DUMP_THREADS", True)
    else:
        zephyr_add_prj_conf("DEBUG_COREDUMP_MEMORY_DUMP_MIN", True)
    if framework_ver < SP_FIX_MIN_VERSION:
        cg.add_build_flag("-Wl,--wrap=z_arm_fatal_error")
        cg.add_build_flag("-Wl,--wrap=z_arm_fault")
    zephyr_add_prj_conf("PARTITION_MANAGER_ENABLED", True)
    size = _partition_size()
    addr = _partition_address(size)
    zephyr_add_pm_static([Section("coredump_partition", addr, size, "flash_primary")])
    zephyr_add_overlay(
        f"""
        &flash0 {{
            partitions {{
                coredump_partition: partition@{addr:x} {{
                    label = "coredump-partition";
                    reg = <{addr:#x} {size:#x}>;
                }};
            }};
        }};
        """
    )
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
