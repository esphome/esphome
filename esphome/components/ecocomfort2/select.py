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

Ecocomfort2Select = ecocomfort2_ns.class_("Ecocomfort2Select", select.Select)
SelectSetting = ecocomfort2_ns.enum("SelectSetting", is_class=True)

CONFIG_SCHEMA = cv.All(
    ECOCOMFORT2_CLIENT_SCHEMA.extend(
        {
            cv.Optional(CONF_SEASON): select.select_schema(
                Ecocomfort2Select,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon="mdi:sun-snowflake-variant",
            ),
            cv.Optional(CONF_FREE_COOLING): select.select_schema(
                Ecocomfort2Select,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon="mdi:snowflake",
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_SEASON, CONF_FREE_COOLING),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ECOCOMFORT2_ID])
    selects = select.sub_selects(config, parent=hub)
    # Option order follows the values the unit stores
    await selects(
        CONF_SEASON,
        partial(hub.set_select, SelectSetting.SEASON),
        SelectSetting.SEASON,
        options=["Winter", "Summer"],
    )
    await selects(
        CONF_FREE_COOLING,
        partial(hub.set_select, SelectSetting.FREE_COOLING),
        SelectSetting.FREE_COOLING,
        options=["Off", "Low", "Medium", "High"],
    )
