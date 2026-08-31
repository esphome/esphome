import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import (
    ECOCOMFORT2_CLIENT_SCHEMA,
    Ecocomfort2AdvancedSwitch,
    register_ecocomfort2_child,
)
from .const import CONF_HUMIDITY_ADVANCED, CONF_VOC_ADVANCED

CODEOWNERS = ["@gledian"]
DEPENDENCIES = ["ecocomfort2"]

SWITCH_TYPES = {
    CONF_HUMIDITY_ADVANCED: "humidity",
    CONF_VOC_ADVANCED: "voc",
}

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(key): switch.switch_schema(
                Ecocomfort2AdvancedSwitch,
                entity_category=ENTITY_CATEGORY_CONFIG,
            )
            for key in SWITCH_TYPES
        }
    ).extend(ECOCOMFORT2_CLIENT_SCHEMA),
    cv.has_at_least_one_key(*SWITCH_TYPES),
)


async def to_code(config):
    for key, kind in SWITCH_TYPES.items():
        if conf := config.get(key):
            var = await switch.new_switch(conf)
            await cg.register_component(var, conf)
            cg.add(var.set_advanced_type(kind))
            await register_ecocomfort2_child(var, config)
