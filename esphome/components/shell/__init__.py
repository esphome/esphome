import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
)
from esphome.components.nrf52 import add_zephyr_prj_conf_option

shell_ns = cg.esphome_ns.namespace("shell")
Shell = shell_ns.class_("Shell", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Shell),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_zephyr,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    add_zephyr_prj_conf_option("CONFIG_SHELL", True)
    add_zephyr_prj_conf_option("CONFIG_SHELL_BACKENDS", True)
    add_zephyr_prj_conf_option("CONFIG_SHELL_BACKEND_SERIAL", True)
    add_zephyr_prj_conf_option("CONFIG_NET_SHELL_DYN_CMD_COMPLETION", True)
    add_zephyr_prj_conf_option("CONFIG_NET_SHELL", True)
    add_zephyr_prj_conf_option("CONFIG_GPIO_SHELL", True)
    # add_zephyr_prj_conf_option("CONFIG_ADC_SHELL", True)
    add_zephyr_prj_conf_option("CONFIG_NET_L2_BT_SHELL", True)
