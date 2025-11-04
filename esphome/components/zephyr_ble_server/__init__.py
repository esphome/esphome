import esphome.codegen as cg
from esphome.components.zephyr import zephyr_add_prj_conf
import esphome.config_validation as cv
from esphome.const import CONF_ESPHOME, CONF_ID, CONF_NAME, Framework
import esphome.final_validate as fv

zephyr_ble_server_ns = cg.esphome_ns.namespace("zephyr_ble_server")
BLEServer = zephyr_ble_server_ns.class_("BLEServer", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BLEServer),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_framework(Framework.ZEPHYR),
)


def _final_validate(_):
    full_config = fv.full_config.get()
    zephyr_add_prj_conf("BT_DEVICE_NAME", full_config[CONF_ESPHOME][CONF_NAME])


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    zephyr_add_prj_conf("BT", True)
    zephyr_add_prj_conf("BT_PERIPHERAL", True)
    zephyr_add_prj_conf("BT_RX_STACK_SIZE", 1536)
    # zephyr_add_prj_conf("BT_LL_SW_SPLIT", True)
    await cg.register_component(var, config)
