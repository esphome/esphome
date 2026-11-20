"""Waits for the id under ``after`` between creating its event and register_event."""

import esphome.codegen as cg
from esphome.components import event
import esphome.config_validation as cv
from esphome.const import CONF_AFTER, CONF_ID
from esphome.types import ConfigType

CONFIG_SCHEMA = event.event_schema(event.Event).extend(
    {cv.Required(CONF_AFTER): cv.use_id(cg.EntityBase)}
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.get_variable(config[CONF_AFTER])
    await event.register_event(
        var, config, event_types=["press"], variables={"steps": "int"}
    )
