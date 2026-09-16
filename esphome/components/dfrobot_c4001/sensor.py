import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv

# from esphome import core
from esphome.const import (
    CONF_MODE,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_SPEED,
    STATE_CLASS_MEASUREMENT,
    UNIT_EMPTY,
    UNIT_METER,
)
import esphome.final_validate as fv

from . import CONF_DFROBOT_C4001_ID, HUB_CHILD_SCHEMA

CONF_TARGET_DISTANCE = "target_distance"
CONF_TARGET_SPEED = "target_speed"
CONF_TARGET_ENERGY = "target_energy"
UNIT_METERS_PER_SECOND = "m/s"

ICON_USBC = "mdi:usb-c-port"

DEPENDENCIES = ["dfrobot_c4001"]

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Optional(CONF_TARGET_DISTANCE): sensor.sensor_schema(
                icon=ICON_USBC,
                accuracy_decimals=2,
                unit_of_measurement=UNIT_METER,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_DISTANCE,
            ),
            cv.Optional(CONF_TARGET_SPEED): sensor.sensor_schema(
                icon=ICON_USBC,
                accuracy_decimals=2,
                unit_of_measurement=UNIT_METERS_PER_SECOND,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_SPEED,
            ),
            cv.Optional(CONF_TARGET_ENERGY): sensor.sensor_schema(
                icon=ICON_USBC,
                accuracy_decimals=0,
                unit_of_measurement=UNIT_EMPTY,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_ENERGY,
            ),
        }
    )
    .extend(HUB_CHILD_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


def _final_validate(config):
    full_config = fv.full_config.get()
    hub_path = full_config.get_path_for_id(config[CONF_DFROBOT_C4001_ID])[:-1]
    hub_conf = full_config.get_config_for_path(hub_path)
    MODE = hub_conf.get(CONF_MODE)
    if MODE == "PRESENCE":
        if CONF_TARGET_DISTANCE in config:
            raise cv.Invalid(
                f"When 'mode' is set to {MODE}, {CONF_TARGET_DISTANCE} sensor is not allowed."
            )
        if CONF_TARGET_SPEED in config:
            raise cv.Invalid(
                f"When 'mode' is set to {MODE}, {CONF_TARGET_SPEED} sensor is not allowed."
            )
        if CONF_TARGET_ENERGY in config:
            raise cv.Invalid(
                f"When 'mode' is set to {MODE}, {CONF_TARGET_ENERGY} sensor is not allowed."
            )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    dfrobot_c4001_hub = await cg.get_variable(config[CONF_DFROBOT_C4001_ID])

    if target_distance := config.get(CONF_TARGET_DISTANCE):
        sens = await sensor.new_sensor(target_distance)
        cg.add(dfrobot_c4001_hub.set_target_distance_sensor(sens))
    if target_speed := config.get(CONF_TARGET_SPEED):
        sens = await sensor.new_sensor(target_speed)
        cg.add(dfrobot_c4001_hub.set_target_speed_sensor(sens))
    if target_energy := config.get(CONF_TARGET_ENERGY):
        sens = await sensor.new_sensor(target_energy)
        cg.add(dfrobot_c4001_hub.set_target_energy_sensor(sens))
