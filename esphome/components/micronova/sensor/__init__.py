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
    CONF_MICRONOVA_ID,
    MICRONOVA_ADDRESS_SCHEMA,
    MicroNova,
    MicroNovaListener,
    micronova_ns,
    to_code_micronova_listener,
)

UNIT_BAR = "bar"

MicroNovaSensor = micronova_ns.class_(
    "MicroNovaSensor", sensor.Sensor, MicroNovaListener
)

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
            MICRONOVA_ADDRESS_SCHEMA(
                default_memory_location=0x00,
                default_memory_address=0x01,
                is_polling_component=True,
            )
        ),
        cv.Optional(CONF_FUMES_TEMPERATURE): sensor.sensor_schema(
            MicroNovaSensor,
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ).extend(
            MICRONOVA_ADDRESS_SCHEMA(
                default_memory_location=0x00,
                default_memory_address=0x5A,
                is_polling_component=True,
            )
        ),
        cv.Optional(CONF_STOVE_POWER): sensor.sensor_schema(
            MicroNovaSensor,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
        ).extend(
            MICRONOVA_ADDRESS_SCHEMA(
                default_memory_location=0x00,
                default_memory_address=0x34,
                is_polling_component=True,
            )
        ),
        cv.Optional(CONF_FAN_SPEED): sensor.sensor_schema(
            MicroNovaSensor,
            state_class=STATE_CLASS_MEASUREMENT,
            unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
        )
        .extend(
            MICRONOVA_ADDRESS_SCHEMA(
                default_memory_location=0x00,
                default_memory_address=0x37,
                is_polling_component=True,
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
            MICRONOVA_ADDRESS_SCHEMA(
                default_memory_location=0x00,
                default_memory_address=0x3B,
                is_polling_component=True,
            )
        ),
        cv.Optional(CONF_WATER_PRESSURE): sensor.sensor_schema(
            MicroNovaSensor,
            unit_of_measurement=UNIT_BAR,
            device_class=DEVICE_CLASS_PRESSURE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ).extend(
            MICRONOVA_ADDRESS_SCHEMA(
                default_memory_location=0x00,
                default_memory_address=0x3C,
                is_polling_component=True,
            )
        ),
        cv.Optional(CONF_MEMORY_ADDRESS_SENSOR): sensor.sensor_schema(
            MicroNovaSensor,
        ).extend(
            MICRONOVA_ADDRESS_SCHEMA(
                is_polling_component=True,
            )
        ),
    }
)


async def to_code(config):
    mv = await cg.get_variable(config[CONF_MICRONOVA_ID])

    for key, divisor in {
        CONF_ROOM_TEMPERATURE: 2,
        CONF_FUMES_TEMPERATURE: None,
        CONF_STOVE_POWER: None,
        CONF_MEMORY_ADDRESS_SENSOR: None,
        CONF_WATER_TEMPERATURE: 2,
        CONF_WATER_PRESSURE: 10,
    }.items():
        if sensor_config := config.get(key):
            sens = await sensor.new_sensor(sensor_config, mv)
            await to_code_micronova_listener(mv, sens, sensor_config)
            if divisor:
                cg.add(sens.set_divisor(divisor))

    if fan_speed_config := config.get(CONF_FAN_SPEED):
        sens = await sensor.new_sensor(fan_speed_config, mv)
        await to_code_micronova_listener(mv, sens, fan_speed_config)
        cg.add(sens.set_fan_speed_offset(fan_speed_config[CONF_FAN_RPM_OFFSET]))
