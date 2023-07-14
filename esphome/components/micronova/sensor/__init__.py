import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

from .. import (
    MicroNova,
    MicroNovaFunctions,
    CONF_MICRONOVA_ID,
    CONF_MEMORY_LOCATION,
    CONF_MEMORY_ADDRESS,
    MICRONOVA_LISTENER_SCHEMA,
    micronova_ns,
)

UNIT_RPM = "rpm"

MicroNovaSensor = micronova_ns.class_("MicroNovaSensor", sensor.Sensor, cg.Component)

CONF_ROOM_TEMPERATURE = "room_temperature"
CONF_THERMOSTAT_TEMPERATURE = "thermostat_temperature"
CONF_FUMES_TEMPERATURE = "fumes_temperature"
CONF_STOVE_POWER = "stove_power"
CONF_FAN_SPEED = "fan_speed"
CONF_MEMORY_ADDRESS_SENSOR = "memory_address_sensor"
CONF_FAN_RPM_OFFSET = "fan_rpm_offset"

TYPES = [
    CONF_ROOM_TEMPERATURE,
    CONF_THERMOSTAT_TEMPERATURE,
    CONF_FUMES_TEMPERATURE,
    CONF_STOVE_POWER,
    CONF_FAN_SPEED,
    CONF_MEMORY_ADDRESS_SENSOR,
    CONF_FAN_RPM_OFFSET,
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MICRONOVA_ID): cv.use_id(MicroNova),
        cv.Optional(CONF_ROOM_TEMPERATURE): sensor.sensor_schema(
            MicroNovaSensor,
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ).extend(MICRONOVA_LISTENER_SCHEMA),
        cv.Optional(CONF_THERMOSTAT_TEMPERATURE): sensor.sensor_schema(
            MicroNovaSensor,
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ).extend(MICRONOVA_LISTENER_SCHEMA),
        cv.Optional(CONF_FUMES_TEMPERATURE): sensor.sensor_schema(
            MicroNovaSensor,
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ).extend(MICRONOVA_LISTENER_SCHEMA),
        cv.Optional(CONF_STOVE_POWER): sensor.sensor_schema(
            MicroNovaSensor,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
        ).extend(MICRONOVA_LISTENER_SCHEMA),
        cv.Optional(CONF_FAN_SPEED): sensor.sensor_schema(
            MicroNovaSensor,
            state_class=STATE_CLASS_MEASUREMENT,
            unit_of_measurement=UNIT_RPM,
        )
        .extend(MICRONOVA_LISTENER_SCHEMA)
        .extend(
            {cv.Optional(CONF_FAN_RPM_OFFSET, default=0): cv.int_range(min=0, max=255)}
        ),
        cv.Optional(CONF_MEMORY_ADDRESS_SENSOR): sensor.sensor_schema(
            MicroNovaSensor,
        ).extend(MICRONOVA_LISTENER_SCHEMA),
    }
)


async def to_code(config):
    mv = await cg.get_variable(config[CONF_MICRONOVA_ID])
    for key in TYPES:
        if key in config:
            conf = config[key]
            sens = await sensor.new_sensor(conf, mv)
            cg.add(mv.register_micronova_listener(sens))
            if key == CONF_ROOM_TEMPERATURE:
                cg.add(sens.set_memory_location(conf.get(CONF_MEMORY_LOCATION, 0x00)))
                cg.add(sens.set_memory_address(conf.get(CONF_MEMORY_ADDRESS, 0x01)))
                cg.add(
                    sens.set_function(
                        MicroNovaFunctions.STOVE_FUNCTION_ROOM_TEMPERATURE
                    )
                )
            if key == CONF_THERMOSTAT_TEMPERATURE:
                cg.add(sens.set_memory_location(conf.get(CONF_MEMORY_LOCATION, 0x20)))
                cg.add(sens.set_memory_address(conf.get(CONF_MEMORY_ADDRESS, 0x7D)))
                cg.add(
                    sens.set_function(
                        MicroNovaFunctions.STOVE_FUNCTION_THERMOSTAT_TEMPERATURE
                    )
                )
            if key == CONF_FUMES_TEMPERATURE:
                cg.add(sens.set_memory_location(conf.get(CONF_MEMORY_LOCATION, 0x00)))
                cg.add(sens.set_memory_address(conf.get(CONF_MEMORY_ADDRESS, 0x5A)))
                cg.add(
                    sens.set_function(
                        MicroNovaFunctions.STOVE_FUNCTION_FUMES_TEMPERATURE
                    )
                )
            if key == CONF_STOVE_POWER:
                cg.add(sens.set_memory_location(conf.get(CONF_MEMORY_LOCATION, 0x00)))
                cg.add(sens.set_memory_address(conf.get(CONF_MEMORY_ADDRESS, 0x34)))
                cg.add(sens.set_function(MicroNovaFunctions.STOVE_FUNCTION_STOVE_POWER))
            if key == CONF_FAN_SPEED:
                cg.add(sens.set_memory_location(conf.get(CONF_MEMORY_LOCATION, 0x00)))
                cg.add(sens.set_memory_address(conf.get(CONF_MEMORY_ADDRESS, 0x37)))
                cg.add(sens.set_function(MicroNovaFunctions.STOVE_FUNCTION_FAN_SPEED))
                cg.add(sens.set_fan_speed_offset(conf[CONF_FAN_RPM_OFFSET]))
            if key == CONF_MEMORY_ADDRESS_SENSOR:
                cg.add(sens.set_memory_location(conf.get(CONF_MEMORY_LOCATION, 0x00)))
                cg.add(sens.set_memory_address(conf.get(CONF_MEMORY_ADDRESS, 0x00)))
                cg.add(
                    sens.set_function(
                        MicroNovaFunctions.STOVE_FUNCTION_MEMORY_ADDRESS_SENSOR
                    )
                )
