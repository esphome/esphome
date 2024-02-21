import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
)
from esphome.components.zephyr import zephyr_add_prj_conf

zephyr_ble_server_ns = cg.esphome_ns.namespace("zephyr_ble_server")
BLEServer = zephyr_ble_server_ns.class_("BLEServer", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BLEServer),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_zephyr,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    zephyr_add_prj_conf("BT", True)
    zephyr_add_prj_conf("BT_PERIPHERAL", True)
    zephyr_add_prj_conf("BT_RX_STACK_SIZE", 1536)
    # zephyr_add_prj_conf("BT_LL_SW_SPLIT", True)
    await cg.register_component(var, config)
