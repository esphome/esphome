import esphome.codegen as cg
from esphome.components.zephyr import zephyr_add_prj_conf
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LOGS, CONF_TYPE

AUTO_LOAD = ["zephyr_ble_server"]
CODEOWNERS = ["@tomaszduda23"]

ble_nus_ns = cg.esphome_ns.namespace("ble_nus")
BLENUS = ble_nus_ns.class_("BLENUS", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BLENUS),
            cv.Optional(CONF_TYPE, default=CONF_LOGS): cv.one_of(
                *[CONF_LOGS], lower=True
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_framework("zephyr"),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    zephyr_add_prj_conf("BT_NUS", True)
    cg.add(var.set_expose_log(config[CONF_TYPE] == CONF_LOGS))
    await cg.register_component(var, config)
