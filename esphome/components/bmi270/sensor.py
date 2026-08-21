#  YAML config keys
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_TEMPERATURE,
    CONF_TYPE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_MAGNET,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_MICROTESLA,
)
from esphome.cpp_generator import MockObj
import esphome.final_validate as fv

from . import CONF_BMI270_ID, BMI270Component, bmi270_ns
from .motion import CONF_AUX_DEVICE

AUTO_LOAD = ["bmi270"]

CONF_MAGNETIC_FIELD_X = "magnetic_field_x"
CONF_MAGNETIC_FIELD_Y = "magnetic_field_y"
CONF_MAGNETIC_FIELD_Z = "magnetic_field_z"

BMM150Data = bmi270_ns.namespace("bmm150").class_("BMM150Data")

_MAGNETIC_FIELDS = (CONF_MAGNETIC_FIELD_X, CONF_MAGNETIC_FIELD_Y, CONF_MAGNETIC_FIELD_Z)


def _parent_schema():
    return cv.Schema({cv.GenerateID(CONF_BMI270_ID): cv.use_id(BMI270Component)})


def _temperature_sensor_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        icon=ICON_THERMOMETER,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ).extend(_parent_schema())


def _magnetic_field_sensor_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_MICROTESLA,
        icon=ICON_MAGNET,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend(_parent_schema())


# `type` defaults to temperature so existing configs that omit it keep working unchanged.
CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_TEMPERATURE: _temperature_sensor_schema(),
        **{x: _magnetic_field_sensor_schema() for x in _MAGNETIC_FIELDS},
    },
    default_type=CONF_TEMPERATURE,
)


def _final_validate(config: dict) -> None:
    if config[CONF_TYPE] not in _MAGNETIC_FIELDS:
        return
    full_config = fv.full_config.get()
    bmi270_path = full_config.get_path_for_id(config[CONF_BMI270_ID])[:-1]
    bmi270_config = full_config.get_config_for_path(bmi270_path)
    if bmi270_config.get(CONF_AUX_DEVICE) != "BMM150":
        raise cv.Invalid(
            "This sensor requires the parent bmi270's 'aux_device' to be set to 'BMM150'",
            path=[CONF_TYPE],
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    sensor_type = config[CONF_TYPE]
    var = await sensor.new_sensor(config)
    parent = await cg.get_variable(config[CONF_BMI270_ID])

    if sensor_type == CONF_TEMPERATURE:
        data = MockObj("data")
        value_lambda = await cg.process_lambda(
            var.publish_state(data),
            [(cg.float_, str(data))],
        )
        cg.add(parent.add_temperature_listener(value_lambda))
        return

    axis = sensor_type[-1:]
    data = MockObj("data")
    value_lambda = await cg.process_lambda(
        var.publish_state(getattr(data, axis)),
        [(BMM150Data.operator("ref"), str(data))],
    )
    cg.add(parent.add_magnetometer_listener(value_lambda))
