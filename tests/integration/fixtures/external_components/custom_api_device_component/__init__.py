import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

custom_api_device_component_ns = cg.esphome_ns.namespace("custom_api_device_component")
CustomAPIDeviceComponent = custom_api_device_component_ns.class_(
    "CustomAPIDeviceComponent", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CustomAPIDeviceComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
