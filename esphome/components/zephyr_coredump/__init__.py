import esphome.codegen as cg
from esphome.components.nrf52.boards import Section
from esphome.components.zephyr import (
    zephyr_add_overlay,
    zephyr_add_pm_static,
    zephyr_add_prj_conf,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, Framework

Coredump = cg.esphome_ns.namespace("zephyr_coredump").class_("Coredump", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema({cv.GenerateID(CONF_ID): cv.declare_id(Coredump)}).extend(
        cv.COMPONENT_SCHEMA
    ),
    cv.only_with_framework(Framework.ZEPHYR),
)


async def to_code(config):
    zephyr_add_prj_conf("DEBUG", True)
    zephyr_add_prj_conf("DEBUG_COREDUMP", True)
    zephyr_add_prj_conf("DEBUG_COREDUMP_BACKEND_FLASH_PARTITION", True)
    zephyr_add_prj_conf("DEBUG_COREDUMP_MEMORY_DUMP_THREADS", True)
    # zephyr_add_prj_conf("EXTRA_EXCEPTION_INFO", True)
    # https://github.com/zephyrproject-rtos/zephyr/pull/79622
    zephyr_add_pm_static(
        [Section("coredump_partition", 0xE4000, 0x10000, "flash_primary")]
    )
    zephyr_add_overlay(
        """
            &flash0 {
                partitions {
                    coredump_partition: partition@d9000 {
                        label = "coredump-partition";
                        reg = <0xE4000 DT_SIZE_K(64)>;
                    };

                };
            };
        """
    )
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
