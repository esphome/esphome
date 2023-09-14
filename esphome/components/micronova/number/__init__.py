import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    UNIT_CELSIUS,
    CONF_MIN_VALUE,
    CONF_MAX_VALUE,
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

CONF_THERMOSTAT_TEMPERATURE = "thermostat_temperature"
CONF_MEMORY_WRITE_LOCATION = "memory_write_location"

MicroNovaNumber = micronova_ns.class_("MicroNovaNumber", number.Number, cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MICRONOVA_ID): cv.use_id(MicroNova),
        cv.Optional(CONF_THERMOSTAT_TEMPERATURE): number.number_schema(
            MicroNovaNumber,
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
        )
        .extend(MICRONOVA_LISTENER_SCHEMA)
        .extend(
            {cv.Optional(CONF_MEMORY_WRITE_LOCATION, default=0xA0): cv.hex_int_range()}
        ),
    }
)


async def to_code(config):
    mv = await cg.get_variable(config[CONF_MICRONOVA_ID])

    if thermostat_temperature_config := config.get(CONF_THERMOSTAT_TEMPERATURE):
        numb = await number.new_number(
            thermostat_temperature_config,
            min_value=thermostat_temperature_config.get(CONF_MIN_VALUE, 0),
            max_value=thermostat_temperature_config.get(CONF_MAX_VALUE, 40),
            step=1,
        )
        cg.add(numb.set_micronova_object(mv))
        cg.add(mv.register_micronova_listener(numb))
        cg.add(
            numb.set_memory_location(
                thermostat_temperature_config.get(CONF_MEMORY_LOCATION, 0x20)
            )
        )
        cg.add(
            numb.set_memory_write_location(
                thermostat_temperature_config.get(CONF_MEMORY_WRITE_LOCATION, 0xA0)
            )
        )
        cg.add(
            numb.set_memory_address(
                thermostat_temperature_config.get(CONF_MEMORY_ADDRESS, 0x7D)
            )
        )
        cg.add(
            numb.set_function(MicroNovaFunctions.STOVE_FUNCTION_THERMOSTAT_TEMPERATURE)
        )
