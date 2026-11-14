from functools import partial

import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from . import CONF_ECOCOMFORT2_ID, ECOCOMFORT2_CLIENT_SCHEMA, ecocomfort2_ns

CODEOWNERS = ["@gledian"]
DEPENDENCIES = ["ecocomfort2"]

CONF_FREE_COOLING = "free_cooling"
CONF_SEASON = "season"

SelectSetting = ecocomfort2_ns.enum("SelectSetting", is_class=True)

# Option order follows the values the unit stores
# key: (setting, icon, options)
_SELECTS = {
    CONF_SEASON: (
        SelectSetting.SELECT_SETTING_SEASON,
        "mdi:sun-snowflake-variant",
        ["Winter", "Summer"],
    ),
    CONF_FREE_COOLING: (
        SelectSetting.SELECT_SETTING_FREE_COOLING,
        "mdi:snowflake",
        ["Off", "Low", "Medium", "High"],
    ),
}

CONFIG_SCHEMA = cv.All(
    ECOCOMFORT2_CLIENT_SCHEMA.extend(
        {
            cv.Optional(key): select.select_schema(
                select.ApplySelect,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon=icon,
            )
            for key, (_, icon, _) in _SELECTS.items()
        }
    ),
    cv.has_at_least_one_key(*_SELECTS),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ECOCOMFORT2_ID])
    selects = select.sub_selects(config, parent=hub)
    cg.add_define("USE_ECOCOMFORT2_SETTINGS")
    # The hub publishes the written option and corrects it from the read-back
    for key, (setting, _, options) in _SELECTS.items():
        await selects(
            key,
            partial(hub.set_select, setting),
            apply=f"write_select({setting}, index)",
            options=options,
        )
