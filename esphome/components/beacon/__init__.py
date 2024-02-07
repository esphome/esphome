import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
)

dfu_ns = cg.esphome_ns.namespace("ble")
Beacon = dfu_ns.class_("Beacon", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Beacon),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_nrf52,
    # TODO implement
    cv.only_with_arduino,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
