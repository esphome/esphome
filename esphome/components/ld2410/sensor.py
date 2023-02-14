import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_ENERGY,
    UNIT_CENTIMETER,
    UNIT_PERCENT,
)
from . import CONF_LD2410_ID, LD2410Component

DEPENDENCIES = ["ld2410"]
CONF_MOVING_DISTANCE = "moving_distance"
CONF_STILL_DISTANCE = "still_distance"
CONF_MOVING_ENERGY = "moving_energy"
CONF_STILL_ENERGY = "still_energy"
CONF_DETECTION_DISTANCE = "detection_distance"

CONF_STILL_ENERGIES = [f"g{x}_still_energy" for x in range(9)]

CONF_MOVE_ENERGIES = [f"g{x}_move_energy" for x in range(9)]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
        cv.Optional(CONF_MOVING_DISTANCE): sensor.sensor_schema(
            device_class=DEVICE_CLASS_DISTANCE,
            unit_of_measurement=UNIT_CENTIMETER,
            icon="mdi:signal-distance-variant",
        ),
        cv.Optional(CONF_STILL_DISTANCE): sensor.sensor_schema(
            device_class=DEVICE_CLASS_DISTANCE,
            unit_of_measurement=UNIT_CENTIMETER,
            icon="mdi:signal-distance-variant",
        ),
        cv.Optional(CONF_MOVING_ENERGY): sensor.sensor_schema(
            device_class=DEVICE_CLASS_ENERGY,
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:lightning-bolt",
        ),
        cv.Optional(CONF_STILL_ENERGY): sensor.sensor_schema(
            device_class=DEVICE_CLASS_ENERGY,
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:lightning-bolt-outline",
        ),
        cv.Optional(CONF_DETECTION_DISTANCE): sensor.sensor_schema(
            device_class=DEVICE_CLASS_DISTANCE,
            unit_of_measurement=UNIT_CENTIMETER,
            icon="mdi:signal-distance-variant",
        ),
    }
)

for i in range(9):
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
        cv.Schema(
            {
                cv.Optional(CONF_MOVE_ENERGIES[i]): sensor.sensor_schema(
                    device_class=DEVICE_CLASS_ENERGY,
                    unit_of_measurement=UNIT_PERCENT,
                    icon="mdi:lightning-bolt",
                ),
                cv.Optional(CONF_STILL_ENERGIES[i]): sensor.sensor_schema(
                    device_class=DEVICE_CLASS_ENERGY,
                    unit_of_measurement=UNIT_PERCENT,
                    icon="mdi:lightning-bolt-outline",
                ),
            }
        )
    )


async def to_code(config):
    ld2410_component = await cg.get_variable(config[CONF_LD2410_ID])
    if CONF_MOVING_DISTANCE in config:
        sens = await sensor.new_sensor(config[CONF_MOVING_DISTANCE])
        cg.add(ld2410_component.set_moving_target_distance_sensor(sens))
    if CONF_STILL_DISTANCE in config:
        sens = await sensor.new_sensor(config[CONF_STILL_DISTANCE])
        cg.add(ld2410_component.set_still_target_distance_sensor(sens))
    if CONF_MOVING_ENERGY in config:
        sens = await sensor.new_sensor(config[CONF_MOVING_ENERGY])
        cg.add(ld2410_component.set_moving_target_energy_sensor(sens))
    if CONF_STILL_ENERGY in config:
        sens = await sensor.new_sensor(config[CONF_STILL_ENERGY])
        cg.add(ld2410_component.set_still_target_energy_sensor(sens))
    if CONF_DETECTION_DISTANCE in config:
        sens = await sensor.new_sensor(config[CONF_DETECTION_DISTANCE])
        cg.add(ld2410_component.set_detection_distance_sensor(sens))
    for x in range(9):
        move = CONF_MOVE_ENERGIES[x]
        still = CONF_STILL_ENERGIES[x]
        if move in config:
            sens = await sensor.new_sensor(config[move])
            cg.add(ld2410_component.set_gate_move_sensor(x, sens))
        if still in config:
            sens = await sensor.new_sensor(config[still])
            cg.add(ld2410_component.set_gate_still_sensor(x, sens))
