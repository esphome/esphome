import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_LOG,
)
from esphome.components.zephyr import zephyr_add_prj_conf

DEPENDENCIES = ["zephyr_ble_server"]

zephyr_ble_nus_ns = cg.esphome_ns.namespace("zephyr_ble_nus")
BLENUS = zephyr_ble_nus_ns.class_("BLENUS", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BLENUS),
            cv.Optional(CONF_LOG, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_zephyr,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    zephyr_add_prj_conf("BT_NUS", True)
    cg.add(var.set_expose_log(config[CONF_LOG]))
    await cg.register_component(var, config)
