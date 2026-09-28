from collections.abc import Callable

import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.types import ConfigFragmentType, ConfigPathType, ConfigType

from . import CONF_LD6004_ID, LD6004Component

CONF_OUT_PIN: str = "out_pin"
CONF_PRESENCE: str = "presence"
CONF_OUT: str = "out"
CONF_TARGET_1: str = "target_1"
CONF_TARGET_2: str = "target_2"
CONF_TARGET_3: str = "target_3"
CONF_TARGET_4: str = "target_4"
CONF_TARGET_5: str = "target_5"
CONF_TARGET_6: str = "target_6"
CONF_TARGET_7: str = "target_7"
CONF_TARGET_8: str = "target_8"
CONF_TARGET_9: str = "target_9"
CONF_TARGET_10: str = "target_10"
CONF_DETECTION_AREA_0: str = "detection_area_0"
CONF_DETECTION_AREA_1: str = "detection_area_1"
CONF_DETECTION_AREA_2: str = "detection_area_2"
CONF_DETECTION_AREA_3: str = "detection_area_3"

DEPENDENCIES: list[str] = ["ld6004"]

FIELDS: tuple[str, ...] = (
    CONF_PRESENCE,
    CONF_TARGET_1,
    CONF_TARGET_2,
    CONF_TARGET_3,
    CONF_TARGET_4,
    CONF_TARGET_5,
    CONF_TARGET_6,
    CONF_TARGET_7,
    CONF_TARGET_8,
    CONF_TARGET_9,
    CONF_TARGET_10,
    CONF_DETECTION_AREA_0,
    CONF_DETECTION_AREA_1,
    CONF_DETECTION_AREA_2,
    CONF_DETECTION_AREA_3,
    CONF_OUT,
)
CONFIG_SCHEMA: cv.Schema = cv.Schema(
    {
        cv.GenerateID(CONF_LD6004_ID): cv.use_id(LD6004Component),
        **{
            cv.Optional(key): binary_sensor.binary_sensor_schema(
                device_class="occupancy" if key != CONF_OUT else ""
            )
            for key in FIELDS
        },
    }
)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_LD6004_BINARY_SENSOR")
    conf: ConfigType | None
    hub: cg.MockObj = await cg.get_variable(config[CONF_LD6004_ID])
    for index, key in enumerate(FIELDS):
        if conf := config.get(key):
            entity: cg.MockObj = await binary_sensor.new_binary_sensor(conf)
            cg.add(hub.set_binary_sensor(index, entity))


def final_validate(config: ConfigType) -> None:
    if CONF_OUT not in config:
        return
    import esphome.final_validate as fv

    full: fv.FinalValidateConfig = fv.full_config.get()
    path: ConfigPathType = full.get_path_for_id(config[CONF_LD6004_ID])[:-1]
    hub: ConfigFragmentType = full.get_config_for_path(path)
    if CONF_OUT_PIN not in hub:
        raise cv.Invalid("out requires out_pin on the same ld6004 instance")


FINAL_VALIDATE_SCHEMA: Callable[[ConfigType], None] = final_validate
