import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DISTANCE,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_PRECIPITATION,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_WIND_SPEED,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_DEGREES,
    UNIT_EMPTY,
    UNIT_KILOMETER,
    UNIT_KILOMETER_PER_HOUR,
    UNIT_LUX,
    UNIT_PERCENT,
)

from .. import AcuRiteComponent, acurite_ns

DEPENDENCIES = ["acurite"]

CONF_ACURITE_ID = "acurite_id"
CONF_DEVICES = "devices"
CONF_DEVICE = "device"
CONF_RAIN = "rain"
CONF_SPEED = "speed"
CONF_DIRECTION = "direction"
CONF_LIGHTNING = "lightning"
CONF_UV = "uv"
CONF_LUX = "lux"
UNIT_MILLIMETER = "mm"

AcuRiteSensor = acurite_ns.class_("AcuRiteSensor", cg.Component)

DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AcuRiteSensor),
        cv.Required(CONF_DEVICE): cv.hex_int_range(max=0x3FFF),
        cv.Optional(CONF_SPEED): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOMETER_PER_HOUR,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_WIND_SPEED,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_DIRECTION): sensor.sensor_schema(
            unit_of_measurement=UNIT_DEGREES,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_RAIN): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILLIMETER,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_PRECIPITATION,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOMETER,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_LIGHTNING): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_TOTAL,
        ),
        cv.Optional(CONF_UV): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_LUX): sensor.sensor_schema(
            unit_of_measurement=UNIT_LUX,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_ILLUMINANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ACURITE_ID): cv.use_id(AcuRiteComponent),
        cv.Required(CONF_DEVICES): cv.ensure_list(DEVICE_SCHEMA),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ACURITE_ID])
    if devices_cfg := config.get(CONF_DEVICES):
        for device_cfg in devices_cfg:
            var = cg.new_Pvariable(device_cfg[CONF_ID])
            await cg.register_component(var, device_cfg)
            cg.add(var.set_id(device_cfg[CONF_DEVICE]))
            if CONF_SPEED in device_cfg:
                sens = await sensor.new_sensor(device_cfg[CONF_SPEED])
                cg.add(var.set_speed_sensor(sens))
            if CONF_DIRECTION in device_cfg:
                sens = await sensor.new_sensor(device_cfg[CONF_DIRECTION])
                cg.add(var.set_direction_sensor(sens))
            if CONF_TEMPERATURE in device_cfg:
                sens = await sensor.new_sensor(device_cfg[CONF_TEMPERATURE])
                cg.add(var.set_temperature_sensor(sens))
            if CONF_HUMIDITY in device_cfg:
                sens = await sensor.new_sensor(device_cfg[CONF_HUMIDITY])
                cg.add(var.set_humidity_sensor(sens))
            if CONF_RAIN in device_cfg:
                sens = await sensor.new_sensor(device_cfg[CONF_RAIN])
                cg.add(var.set_rainfall_sensor(sens))
            if CONF_LIGHTNING in device_cfg:
                sens = await sensor.new_sensor(device_cfg[CONF_LIGHTNING])
                cg.add(var.set_lightning_sensor(sens))
            if CONF_DISTANCE in device_cfg:
                sens = await sensor.new_sensor(device_cfg[CONF_DISTANCE])
                cg.add(var.set_distance_sensor(sens))
            if CONF_UV in device_cfg:
                sens = await sensor.new_sensor(device_cfg[CONF_UV])
                cg.add(var.set_uv_sensor(sens))
            if CONF_LUX in device_cfg:
                sens = await sensor.new_sensor(device_cfg[CONF_LUX])
                cg.add(var.set_lux_sensor(sens))
            cg.add(parent.add_device(var, device_cfg[CONF_DEVICE]))
