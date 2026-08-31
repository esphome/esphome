import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import (
    ECOCOMFORT2_CLIENT_SCHEMA,
    Ecocomfort2OffsetNumber,
    Ecocomfort2ThresholdNumber,
    register_ecocomfort2_child,
)
from .const import (
    CONF_HUMIDITY_OFFSET_NUMBER,
    CONF_HUMIDITY_THRESHOLD,
    CONF_LUMINOSITY_THRESHOLD,
    CONF_TEMP_OFFSET_NUMBER,
    CONF_VOC_THRESHOLD,
)

CODEOWNERS = ["@gledian"]
DEPENDENCIES = ["ecocomfort2"]

NUMBER_TYPES = {
    CONF_HUMIDITY_THRESHOLD: {
        "class": Ecocomfort2ThresholdNumber,
        "min_value": 0,
        "max_value": 3,
        "step": 1,
        "kind": "humidity",
        "setter": "set_threshold_type",
    },
    CONF_LUMINOSITY_THRESHOLD: {
        "class": Ecocomfort2ThresholdNumber,
        "min_value": 0,
        "max_value": 3,
        "step": 1,
        "kind": "luminosity",
        "setter": "set_threshold_type",
    },
    CONF_VOC_THRESHOLD: {
        "class": Ecocomfort2ThresholdNumber,
        "min_value": 0,
        "max_value": 3,
        "step": 1,
        "kind": "voc",
        "setter": "set_threshold_type",
    },
    CONF_TEMP_OFFSET_NUMBER: {
        "class": Ecocomfort2OffsetNumber,
        "min_value": -5.0,
        "max_value": 5.0,
        "step": 0.01,
        "kind": "temperature",
        "setter": "set_offset_type",
    },
    CONF_HUMIDITY_OFFSET_NUMBER: {
        "class": Ecocomfort2OffsetNumber,
        "min_value": -10.0,
        "max_value": 10.0,
        "step": 0.01,
        "kind": "humidity",
        "setter": "set_offset_type",
    },
}

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(key): number.number_schema(
                spec["class"],
                entity_category=ENTITY_CATEGORY_CONFIG,
            )
            for key, spec in NUMBER_TYPES.items()
        }
    ).extend(ECOCOMFORT2_CLIENT_SCHEMA),
    cv.has_at_least_one_key(*NUMBER_TYPES),
)


async def to_code(config):
    for key, spec in NUMBER_TYPES.items():
        if conf := config.get(key):
            var = await number.new_number(
                conf,
                min_value=spec["min_value"],
                max_value=spec["max_value"],
                step=spec["step"],
            )
            await cg.register_component(var, conf)
            cg.add(getattr(var, spec["setter"])(spec["kind"]))
            await register_ecocomfort2_child(var, config)
