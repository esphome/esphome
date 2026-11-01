#  YAML config keys
from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.motion import (
    temperature_sensor_schema,
    temperature_sensor_to_code,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_TEMPERATURE,
    CONF_TYPE,
    ICON_MAGNET,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROTESLA,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import CONF_BMI270_ID, MAGNETOMETER_DATA, MAGNETOMETER_PUBLISHER, BMI270Component
from .motion import CONF_AUX_DEVICE

AUTO_LOAD = ["bmi270"]

CONF_MAGNETIC_FIELD_X = "magnetic_field_x"
CONF_MAGNETIC_FIELD_Y = "magnetic_field_y"
CONF_MAGNETIC_FIELD_Z = "magnetic_field_z"

_MAGNETIC_FIELDS = (CONF_MAGNETIC_FIELD_X, CONF_MAGNETIC_FIELD_Y, CONF_MAGNETIC_FIELD_Z)


def _parent_schema() -> cv.Schema:
    return cv.Schema({cv.GenerateID(CONF_BMI270_ID): cv.use_id(BMI270Component)})


def _magnetic_field_sensor_schema() -> cv.Schema:
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_MICROTESLA,
        icon=ICON_MAGNET,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend(_parent_schema())


# `type` defaults to temperature so existing configs that omit it keep working unchanged.
CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_TEMPERATURE: temperature_sensor_schema(
            CONF_BMI270_ID, BMI270Component, type_key=False
        ),
        **{x: _magnetic_field_sensor_schema() for x in _MAGNETIC_FIELDS},
    },
    default_type=CONF_TEMPERATURE,
)


def _final_validate(config: ConfigType) -> None:
    if config[CONF_TYPE] not in _MAGNETIC_FIELDS:
        return
    bmi270_config = fv.get_declaring_config(config[CONF_BMI270_ID])
    if bmi270_config.get(CONF_AUX_DEVICE) != "BMM150":
        raise cv.Invalid(
            "This sensor requires the parent bmi270's 'aux_device' to be set to 'BMM150'",
            path=[CONF_TYPE],
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    sensor_type = config[CONF_TYPE]
    if sensor_type == CONF_TEMPERATURE:
        await temperature_sensor_to_code(config, CONF_BMI270_ID)
        return

    var = await sensor.new_sensor(config)
    parent = await cg.get_variable(config[CONF_BMI270_ID])
    axis = sensor_type[-1:]
    MAGNETOMETER_PUBLISHER.add(
        parent, parent_ref(var).publish_state(getattr(MAGNETOMETER_DATA, axis))
    )
