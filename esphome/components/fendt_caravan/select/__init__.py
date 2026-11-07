import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_OPTIONS, CONF_TYPE

from .. import CONF_PARENT_ID, FendtCaravanHubBase

CONF_ALDE_HEATER_ELECTRICITY = "alde_heater_electricity"
CONF_FRIDGE_MODE = "fridge_mode"
CONF_FRIDGE_TEMPERATURE = "fridge_temperature"


def _select_schema(icon: str = cv.UNDEFINED) -> cv.Schema:
    return select.select_schema(select.ApplySelect).extend(
        {
            cv.Required(CONF_PARENT_ID): cv.use_id(FendtCaravanHubBase),
        }
    )


CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_ALDE_HEATER_ELECTRICITY: _select_schema().extend(
            {
                cv.Optional(
                    CONF_OPTIONS, default=["Off", "1 kW", "2 kW", "3 kW"]
                ): cv.ensure_list(cv.string_strict)
            }
        ),
        CONF_FRIDGE_MODE: _select_schema().extend(
            {
                cv.Optional(
                    CONF_OPTIONS, default=["Performance", "Quiet", "Boost"]
                ): cv.ensure_list(cv.string_strict)
            }
        ),
        CONF_FRIDGE_TEMPERATURE: _select_schema().extend(
            {
                cv.Optional(
                    CONF_OPTIONS, default=["1", "2", "3", "4", "5"]
                ): cv.ensure_list(cv.string_strict)
            }
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_PARENT_ID])
    selects = select.sub_selects(config, parent=hub)
    await selects.create(
        config,
        setter=getattr(hub, f"set_{config[CONF_TYPE]}_select"),
        options=config[CONF_OPTIONS],
        apply="select_control(entity, index)",
    )
