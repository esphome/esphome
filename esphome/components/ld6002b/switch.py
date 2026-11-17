import esphome.codegen as cg
from esphome.components import switch
from esphome.components.ld600x import entities
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import LD6002BComponent
from .const import CONF_LD6002B_ID, CONF_LOW_POWER

DEPENDENCIES = ["ld6002b"]

CONFIG_SCHEMA = entities.switch_schema(
    LD6002BComponent,
    CONF_LD6002B_ID,
    extra={cv.Optional(CONF_LOW_POWER): entities.CONFIG_SWITCH_SCHEMA},
)
FINAL_VALIDATE_SCHEMA = entities.unique_settings_final_validate(
    switch.DOMAIN, CONF_LD6002B_ID, "ld6002b"
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD6002B_ID])
    switches = switch.sub_switches(config, parent=hub)
    await switches(CONF_LOW_POWER, hub.set_low_power_switch, **entities.SWITCH_CONTROL)
    await entities.switch_to_code(config, CONF_LD6002B_ID)
