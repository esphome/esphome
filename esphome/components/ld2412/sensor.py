import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_LIGHT,
    CONF_MOVING_DISTANCE,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_ILLUMINANCE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_FLASH,
    ICON_LIGHTBULB,
    ICON_MOTION_SENSOR,
    ICON_SIGNAL,
    UNIT_CENTIMETER,
    UNIT_EMPTY,
    UNIT_PERCENT,
)

from . import CONF_LD2412_ID, LD2412Component

DEPENDENCIES = ["ld2412"]

CONF_DETECTION_DISTANCE = "detection_distance"
CONF_MOVE_ENERGY = "move_energy"
CONF_MOVING_ENERGY = "moving_energy"
CONF_STILL_DISTANCE = "still_distance"
CONF_STILL_ENERGY = "still_energy"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2412_ID): cv.use_id(LD2412Component),
        cv.Optional(CONF_DETECTION_DISTANCE): sensor.sensor_schema(
            device_class=DEVICE_CLASS_DISTANCE,
            filters=[
                {
                    "timeout": {
                        "timeout": cv.TimePeriod(milliseconds=1000),
                        "value": "last",
                    }
                },
                {"throttle_with_priority": cv.TimePeriod(milliseconds=1000)},
            ],
            icon=ICON_SIGNAL,
            unit_of_measurement=UNIT_CENTIMETER,
        ),
        cv.Optional(CONF_LIGHT): sensor.sensor_schema(
            device_class=DEVICE_CLASS_ILLUMINANCE,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            filters=[
                {
                    "timeout": {
                        "timeout": cv.TimePeriod(milliseconds=1000),
                        "value": "last",
                    }
                },
                {"throttle_with_priority": cv.TimePeriod(milliseconds=1000)},
            ],
            icon=ICON_LIGHTBULB,
            unit_of_measurement=UNIT_EMPTY,  # No standard unit for this light sensor
        ),
        cv.Optional(CONF_MOVING_DISTANCE): sensor.sensor_schema(
            device_class=DEVICE_CLASS_DISTANCE,
            filters=[
                {
                    "timeout": {
                        "timeout": cv.TimePeriod(milliseconds=1000),
                        "value": "last",
                    }
                },
                {"throttle_with_priority": cv.TimePeriod(milliseconds=1000)},
            ],
            icon=ICON_SIGNAL,
            unit_of_measurement=UNIT_CENTIMETER,
        ),
        cv.Optional(CONF_MOVING_ENERGY): sensor.sensor_schema(
            filters=[
                {
                    "timeout": {
                        "timeout": cv.TimePeriod(milliseconds=1000),
                        "value": "last",
                    }
                },
                {"throttle_with_priority": cv.TimePeriod(milliseconds=1000)},
            ],
            icon=ICON_MOTION_SENSOR,
            unit_of_measurement=UNIT_PERCENT,
        ),
        cv.Optional(CONF_STILL_DISTANCE): sensor.sensor_schema(
            device_class=DEVICE_CLASS_DISTANCE,
            filters=[
                {
                    "timeout": {
                        "timeout": cv.TimePeriod(milliseconds=1000),
                        "value": "last",
                    }
                },
                {"throttle_with_priority": cv.TimePeriod(milliseconds=1000)},
            ],
            icon=ICON_SIGNAL,
            unit_of_measurement=UNIT_CENTIMETER,
        ),
        cv.Optional(CONF_STILL_ENERGY): sensor.sensor_schema(
            filters=[
                {
                    "timeout": {
                        "timeout": cv.TimePeriod(milliseconds=1000),
                        "value": "last",
                    }
                },
                {"throttle_with_priority": cv.TimePeriod(milliseconds=1000)},
            ],
            icon=ICON_FLASH,
            unit_of_measurement=UNIT_PERCENT,
        ),
    }
)

CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
    {
        cv.Optional(f"gate_{x}"): (
            {
                cv.Optional(CONF_MOVE_ENERGY): sensor.sensor_schema(
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                    filters=[
                        {
                            "timeout": {
                                "timeout": cv.TimePeriod(milliseconds=1000),
                                "value": "last",
                            }
                        },
                        {"throttle_with_priority": cv.TimePeriod(milliseconds=1000)},
                    ],
                    icon=ICON_MOTION_SENSOR,
                    unit_of_measurement=UNIT_PERCENT,
                ),
                cv.Optional(CONF_STILL_ENERGY): sensor.sensor_schema(
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                    filters=[
                        {
                            "timeout": {
                                "timeout": cv.TimePeriod(milliseconds=1000),
                                "value": "last",
                            }
                        },
                        {"throttle_with_priority": cv.TimePeriod(milliseconds=1000)},
                    ],
                    icon=ICON_FLASH,
                    unit_of_measurement=UNIT_PERCENT,
                ),
            }
        )
        for x in range(14)
    }
)


async def to_code(config):
    LD2412_component = await cg.get_variable(config[CONF_LD2412_ID])
    if detection_distance_config := config.get(CONF_DETECTION_DISTANCE):
        sens = await sensor.new_sensor(detection_distance_config)
        cg.add(LD2412_component.set_detection_distance_sensor(sens))
    if light_config := config.get(CONF_LIGHT):
        sens = await sensor.new_sensor(light_config)
        cg.add(LD2412_component.set_light_sensor(sens))
    if moving_distance_config := config.get(CONF_MOVING_DISTANCE):
        sens = await sensor.new_sensor(moving_distance_config)
        cg.add(LD2412_component.set_moving_target_distance_sensor(sens))
    if moving_energy_config := config.get(CONF_MOVING_ENERGY):
        sens = await sensor.new_sensor(moving_energy_config)
        cg.add(LD2412_component.set_moving_target_energy_sensor(sens))
    if still_distance_config := config.get(CONF_STILL_DISTANCE):
        sens = await sensor.new_sensor(still_distance_config)
        cg.add(LD2412_component.set_still_target_distance_sensor(sens))
    if still_energy_config := config.get(CONF_STILL_ENERGY):
        sens = await sensor.new_sensor(still_energy_config)
        cg.add(LD2412_component.set_still_target_energy_sensor(sens))
    for x in range(14):
        if gate_conf := config.get(f"gate_{x}"):
            if move_config := gate_conf.get(CONF_MOVE_ENERGY):
                sens = await sensor.new_sensor(move_config)
                cg.add(LD2412_component.set_gate_move_sensor(x, sens))
            if still_config := gate_conf.get(CONF_STILL_ENERGY):
                sens = await sensor.new_sensor(still_config)
                cg.add(LD2412_component.set_gate_still_sensor(x, sens))
