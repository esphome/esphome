import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import (
    ECOCOMFORT2_CLIENT_SCHEMA,
    Ecocomfort2PairButton,
    register_ecocomfort2_child,
)
from .const import CONF_PAIR

CODEOWNERS = ["@gledian"]
DEPENDENCIES = ["ecocomfort2"]

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Optional(CONF_PAIR): button.button_schema(
                Ecocomfort2PairButton,
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
        }
    )
    .extend(ECOCOMFORT2_CLIENT_SCHEMA)
    .add_extra(cv.has_at_least_one_key(CONF_PAIR))
)


async def to_code(config):
    if conf := config.get(CONF_PAIR):
        var = await button.new_button(conf)
        await cg.register_component(var, conf)
        await register_ecocomfort2_child(var, config)
