import esphome.codegen as cg
from esphome.components import uart
from esphome.components.light.effects import register_addressable_effect
from esphome.components.light.types import AddressableLightEffect
import esphome.config_validation as cv
from esphome.const import CONF_NAME, CONF_UART_ID
from esphome.core import ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

DEPENDENCIES = ["uart"]

adalight_ns = cg.esphome_ns.namespace("adalight")
AdalightLightEffect = adalight_ns.class_(
    "AdalightLightEffect", uart.UARTDevice, AddressableLightEffect
)

CONFIG_SCHEMA = cv.Schema({})


@register_addressable_effect(
    "adalight",
    AdalightLightEffect,
    "Adalight",
    {cv.GenerateID(CONF_UART_ID): cv.use_id(uart.UARTComponent)},
)
async def adalight_light_effect_to_code(config: ConfigType, effect_id: ID) -> MockObj:
    effect = cg.new_Pvariable(effect_id, config[CONF_NAME])
    await uart.register_uart_device(effect, config)
    return effect
