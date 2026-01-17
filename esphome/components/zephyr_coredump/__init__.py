import esphome.codegen as cg
from esphome.components.nrf52.boards import Section
from esphome.components.zephyr import (
    zephyr_add_overlay,
    zephyr_add_pm_static,
    zephyr_add_prj_conf,
    zephyr_data,
)
from esphome.components.zephyr.const import BOOTLOADER_MCUBOOT, KEY_BOOTLOADER
import esphome.config_validation as cv
from esphome.const import CONF_ID, KEY_CORE, KEY_FRAMEWORK_VERSION, Framework
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@tomaszduda23"]

Coredump = cg.esphome_ns.namespace("zephyr_coredump").class_("Coredump", cg.Component)


def framework_version(config: ConfigType) -> ConfigType:
    framework_ver: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    if framework_ver < cv.Version(2, 9, 2):
        return cv.require_framework_version(
            nrf52_zephyr=cv.Version(2, 6, 1, "9"),
        )(config)
    return cv.require_framework_version(
        nrf52_zephyr=cv.Version(2, 9, 2, "1"),
    )(config)


CONFIG_SCHEMA = cv.All(
    cv.Schema({cv.GenerateID(CONF_ID): cv.declare_id(Coredump)}).extend(
        cv.COMPONENT_SCHEMA
    ),
    cv.only_with_framework(Framework.ZEPHYR),
    framework_version,
)


async def to_code(config):
    zephyr_add_prj_conf("DEBUG", True)
    zephyr_add_prj_conf("DEBUG_COREDUMP", True)
    zephyr_add_prj_conf("DEBUG_COREDUMP_BACKEND_FLASH_PARTITION", True)
    framework_ver: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    if framework_ver >= cv.Version(2, 9, 2):
        zephyr_add_prj_conf("DEBUG_COREDUMP_MEMORY_DUMP_THREADS", True)
    else:
        zephyr_add_prj_conf("DEBUG_COREDUMP_MEMORY_DUMP_MIN", True)
    cg.add_build_flag("-Wl,--wrap=z_arm_fatal_error")
    cg.add_build_flag("-Wl,--wrap=z_arm_fault")
    zephyr_add_prj_conf("PARTITION_MANAGER_ENABLED", True)
    addr = 0xE4000
    if zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT:
        addr = 0xF0000
    zephyr_add_pm_static(
        [Section("coredump_partition", addr, 0x10000, "flash_primary")]
    )
    zephyr_add_overlay(
        f"""
            &flash0 {{
                partitions {{
                    coredump_partition: partition@{hex(addr)} {{
                        label = "coredump-partition";
                        reg = <{hex(addr)} DT_SIZE_K(64)>;
                    }};
                }};
            }};
        """
    )
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
