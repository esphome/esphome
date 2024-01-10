import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
)

dfu_ns = cg.esphome_ns.namespace("dfu")
DeviceFirmwareUpdate = dfu_ns.class_("DeviceFirmwareUpdate", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DeviceFirmwareUpdate),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_nrf52,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    # week symbol do not work for some reason so use wrap instaed
    cg.add_build_flag("-Wl,--wrap=tud_cdc_line_state_cb")
    await cg.register_component(var, config)
