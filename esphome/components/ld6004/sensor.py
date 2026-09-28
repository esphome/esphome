import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_X, CONF_Y
from esphome.types import ConfigType

from . import AXES, CONF_LD6004_ID, LD6004Component

CONF_Z: str = "z"
CONF_TARGET_COUNT: str = "target_count"
CONF_PARSER_ERRORS: str = "parser_errors"
CONF_DOPPLER_INDEX: str = "doppler_index"
CONF_CLUSTER_ID: str = "cluster_id"

DEPENDENCIES: list[str] = ["ld6004"]

TARGET_KEYS: tuple[str, ...] = tuple(f"target_{i + 1}" for i in range(10))
TARGET_FIELDS: tuple[str, ...] = (
    CONF_X,
    CONF_Y,
    CONF_Z,
    CONF_DOPPLER_INDEX,
    CONF_CLUSTER_ID,
)
TARGET_SCHEMA: cv.Schema = cv.Schema(
    {
        cv.Optional(key): sensor.sensor_schema(
            accuracy_decimals=2 if key in (CONF_X, CONF_Y, CONF_Z) else 0,
            unit_of_measurement="m" if key in (CONF_X, CONF_Y, CONF_Z) else "",
        )
        for key in TARGET_FIELDS
    }
)
ZONE_SENSOR_SCHEMA: cv.Schema = cv.Schema(
    {
        cv.Optional(key): sensor.sensor_schema(
            accuracy_decimals=2, unit_of_measurement="m"
        )
        for key in AXES
    }
)
CONFIG_SCHEMA: cv.Schema = cv.Schema(
    {
        cv.GenerateID(CONF_LD6004_ID): cv.use_id(LD6004Component),
        **{
            cv.Optional(key): sensor.sensor_schema(accuracy_decimals=0)
            for key in (CONF_TARGET_COUNT, CONF_PARSER_ERRORS)
        },
        **{cv.Optional(key): TARGET_SCHEMA for key in TARGET_KEYS},
        **{
            cv.Optional(f"{kind}_area_{i}"): ZONE_SENSOR_SCHEMA
            for kind in ("interference", "detection", "dwell")
            for i in range(4)
        },
    }
)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_LD6004_SENSOR")
    conf: ConfigType | None
    hub: cg.MockObj = await cg.get_variable(config[CONF_LD6004_ID])
    for key, index in ((CONF_TARGET_COUNT, 0), (CONF_PARSER_ERRORS, 123)):
        if conf := config.get(key):
            entity: cg.MockObj = await sensor.new_sensor(conf)
            cg.add(hub.set_sensor(index, entity))
    for i in range(10):
        for j, key in enumerate(TARGET_FIELDS):
            if conf := config.get(TARGET_KEYS[i], {}).get(key):
                entity: cg.MockObj = await sensor.new_sensor(conf)
                cg.add(hub.set_sensor(1 + 5 * i + j, entity))
    for kind_index, kind in enumerate(("interference", "detection", "dwell")):
        for i in range(4):
            for j, key in enumerate(AXES):
                if conf := config.get(f"{kind}_area_{i}", {}).get(key):
                    entity: cg.MockObj = await sensor.new_sensor(conf)
                    cg.add(hub.set_sensor(51 + kind_index * 24 + i * 6 + j, entity))
