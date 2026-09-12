import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import (
    ECOCOMFORT2_CLIENT_SCHEMA,
    Ecocomfort2FreeCoolingSelect,
    Ecocomfort2SeasonSelect,
    register_ecocomfort2_child,
)
from .const import (
    CONF_FREE_COOLING,
    CONF_SEASON,
    FREE_COOLING_HIGH,
    FREE_COOLING_LOW,
    FREE_COOLING_MEDIUM,
    FREE_COOLING_OFF,
    SEASON_SUMMER,
    SEASON_WINTER,
)

CODEOWNERS = ["@gledian"]
DEPENDENCIES = ["ecocomfort2"]

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_SEASON): select.select_schema(
                Ecocomfort2SeasonSelect,
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
            cv.Optional(CONF_FREE_COOLING): select.select_schema(
                Ecocomfort2FreeCoolingSelect,
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
        }
    ).extend(ECOCOMFORT2_CLIENT_SCHEMA),
    cv.has_at_least_one_key(CONF_SEASON, CONF_FREE_COOLING),
)


async def to_code(config):
    if conf := config.get(CONF_SEASON):
        var = await select.new_select(conf, options=[SEASON_WINTER, SEASON_SUMMER])
        await cg.register_component(var, conf)
        await register_ecocomfort2_child(var, config)

    if conf := config.get(CONF_FREE_COOLING):
        var = await select.new_select(
            conf,
            options=[
                FREE_COOLING_OFF,
                FREE_COOLING_LOW,
                FREE_COOLING_MEDIUM,
                FREE_COOLING_HIGH,
            ],
        )
        await cg.register_component(var, conf)
        await register_ecocomfort2_child(var, config)
