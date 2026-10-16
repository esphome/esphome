import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import factory_reset_ns

FactoryResetButton = factory_reset_ns.class_(
    "FactoryResetButton", button.Button, cg.Component
)

CONFIG_SCHEMA = button.factory_reset_button_schema(FactoryResetButton).extend(
    cv.COMPONENT_SCHEMA
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await button.register_button(var, config)
