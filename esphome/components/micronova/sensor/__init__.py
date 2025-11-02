import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_REVOLUTIONS_PER_MINUTE,
)

from .. import (
    CONF_MEMORY_ADDRESS,
    CONF_MEMORY_LOCATION,
    CONF_MICRONOVA_ID,
    MICRONOVA_LISTENER_SCHEMA,
    MicroNova,
    MicroNovaFunctions,
    micronova_ns,
)

UNIT_BAR = "bar"

MicroNovaSensor = micronova_ns.class_("MicroNovaSensor", sensor.Sensor, cg.Component)

CONF_ROOM_TEMPERATURE = "room_temperature"
CONF_FUMES_TEMPERATURE = "fumes_temperature"
CONF_STOVE_POWER = "stove_power"
CONF_FAN_SPEED = "fan_speed"
CONF_WATER_TEMPERATURE = "water_temperature"
CONF_WATER_PRESSURE = "water_pressure"
CONF_MEMORY_ADDRESS_SENSOR = "memory_address_sensor"
CONF_FAN_RPM_OFFSET = "fan_rpm_offset"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MICRONOVA_ID): cv.use_id(MicroNova),
        cv.Optional(CONF_ROOM_TEMPERATURE): sensor.sensor_schema(
            MicroNovaSensor,
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ).extend(
            MICRONOVA_LISTENER_SCHEMA(
                default_memory_location=0x00, default_memory_address=0x01
            )
        ),
        cv.Optional(CONF_FUMES_TEMPERATURE): sensor.sensor_schema(
            MicroNovaSensor,
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ).extend(
            MICRONOVA_LISTENER_SCHEMA(
                default_memory_location=0x00, default_memory_address=0x5A
            )
        ),
        cv.Optional(CONF_STOVE_POWER): sensor.sensor_schema(
            MicroNovaSensor,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
        ).extend(
            MICRONOVA_LISTENER_SCHEMA(
                default_memory_location=0x00, default_memory_address=0x34
            )
        ),
        cv.Optional(CONF_FAN_SPEED): sensor.sensor_schema(
            MicroNovaSensor,
            state_class=STATE_CLASS_MEASUREMENT,
            unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
        )
        .extend(
            MICRONOVA_LISTENER_SCHEMA(
                default_memory_location=0x00, default_memory_address=0x37
            )
        )
        .extend(
            {cv.Optional(CONF_FAN_RPM_OFFSET, default=0): cv.int_range(min=0, max=255)}
        ),
        cv.Optional(CONF_WATER_TEMPERATURE): sensor.sensor_schema(
            MicroNovaSensor,
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ).extend(
            MICRONOVA_LISTENER_SCHEMA(
                default_memory_location=0x00, default_memory_address=0x3B
            )
        ),
        cv.Optional(CONF_WATER_PRESSURE): sensor.sensor_schema(
            MicroNovaSensor,
            unit_of_measurement=UNIT_BAR,
            device_class=DEVICE_CLASS_PRESSURE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ).extend(
            MICRONOVA_LISTENER_SCHEMA(
                default_memory_location=0x00, default_memory_address=0x3C
            )
        ),
        cv.Optional(CONF_MEMORY_ADDRESS_SENSOR): sensor.sensor_schema(
            MicroNovaSensor,
        ).extend(
            MICRONOVA_LISTENER_SCHEMA(
                default_memory_location=0x00, default_memory_address=0x00
            )
        ),
    }
)


async def to_code(config):
    mv = await cg.get_variable(config[CONF_MICRONOVA_ID])

    if room_temperature_config := config.get(CONF_ROOM_TEMPERATURE):
        sens = await sensor.new_sensor(room_temperature_config, mv)
        cg.add(mv.register_micronova_listener(sens))
        cg.add(sens.set_memory_location(room_temperature_config[CONF_MEMORY_LOCATION]))
        cg.add(sens.set_memory_address(room_temperature_config[CONF_MEMORY_ADDRESS]))
        cg.add(sens.set_function(MicroNovaFunctions.STOVE_FUNCTION_ROOM_TEMPERATURE))

    if fumes_temperature_config := config.get(CONF_FUMES_TEMPERATURE):
        sens = await sensor.new_sensor(fumes_temperature_config, mv)
        cg.add(mv.register_micronova_listener(sens))
        cg.add(sens.set_memory_location(fumes_temperature_config[CONF_MEMORY_LOCATION]))
        cg.add(sens.set_memory_address(fumes_temperature_config[CONF_MEMORY_ADDRESS]))
        cg.add(sens.set_function(MicroNovaFunctions.STOVE_FUNCTION_FUMES_TEMPERATURE))

    if stove_power_config := config.get(CONF_STOVE_POWER):
        sens = await sensor.new_sensor(stove_power_config, mv)
        cg.add(mv.register_micronova_listener(sens))
        cg.add(sens.set_memory_location(stove_power_config[CONF_MEMORY_LOCATION]))
        cg.add(sens.set_memory_address(stove_power_config[CONF_MEMORY_ADDRESS]))
        cg.add(sens.set_function(MicroNovaFunctions.STOVE_FUNCTION_STOVE_POWER))

    if fan_speed_config := config.get(CONF_FAN_SPEED):
        sens = await sensor.new_sensor(fan_speed_config, mv)
        cg.add(mv.register_micronova_listener(sens))
        cg.add(sens.set_memory_location(fan_speed_config[CONF_MEMORY_LOCATION]))
        cg.add(sens.set_memory_address(fan_speed_config[CONF_MEMORY_ADDRESS]))
        cg.add(sens.set_function(MicroNovaFunctions.STOVE_FUNCTION_FAN_SPEED))
        cg.add(sens.set_fan_speed_offset(fan_speed_config[CONF_FAN_RPM_OFFSET]))

    if memory_address_sensor_config := config.get(CONF_MEMORY_ADDRESS_SENSOR):
        sens = await sensor.new_sensor(memory_address_sensor_config, mv)
        cg.add(mv.register_micronova_listener(sens))
        cg.add(
            sens.set_memory_location(memory_address_sensor_config[CONF_MEMORY_LOCATION])
        )
        cg.add(
            sens.set_memory_address(memory_address_sensor_config[CONF_MEMORY_ADDRESS])
        )
        cg.add(
            sens.set_function(MicroNovaFunctions.STOVE_FUNCTION_MEMORY_ADDRESS_SENSOR)
        )

    if water_temperature_config := config.get(CONF_WATER_TEMPERATURE):
        sens = await sensor.new_sensor(water_temperature_config, mv)
        cg.add(mv.register_micronova_listener(sens))
        cg.add(sens.set_memory_location(water_temperature_config[CONF_MEMORY_LOCATION]))
        cg.add(sens.set_memory_address(water_temperature_config[CONF_MEMORY_ADDRESS]))
        cg.add(sens.set_function(MicroNovaFunctions.STOVE_FUNCTION_WATER_TEMPERATURE))

    if water_pressure_config := config.get(CONF_WATER_PRESSURE):
        sens = await sensor.new_sensor(water_pressure_config, mv)
        cg.add(mv.register_micronova_listener(sens))
        cg.add(sens.set_memory_location(water_pressure_config[CONF_MEMORY_LOCATION]))
        cg.add(sens.set_memory_address(water_pressure_config[CONF_MEMORY_ADDRESS]))
        cg.add(sens.set_function(MicroNovaFunctions.STOVE_FUNCTION_WATER_PRESSURE))
