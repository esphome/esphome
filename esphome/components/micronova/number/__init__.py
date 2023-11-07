import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    UNIT_CELSIUS,
    CONF_STEP,
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

ICON_FLASH = "mdi:flash"

CONF_THERMOSTAT_TEMPERATURE = "thermostat_temperature"
CONF_POWER_LEVEL = "power_level"
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
        .extend(
            MICRONOVA_LISTENER_SCHEMA(
                default_memory_location=0x20, default_memory_address=0x7D
            )
        )
        .extend(
            {
                cv.Optional(
                    CONF_MEMORY_WRITE_LOCATION, default=0xA0
                ): cv.hex_int_range(),
                cv.Optional(CONF_STEP, default=1.0): cv.float_range(min=0.1, max=10.0),
            }
        ),
        cv.Optional(CONF_POWER_LEVEL): number.number_schema(
            MicroNovaNumber,
            icon=ICON_FLASH,
        )
        .extend(
            MICRONOVA_LISTENER_SCHEMA(
                default_memory_location=0x20, default_memory_address=0x7F
            )
        )
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
            min_value=0,
            max_value=40,
            step=thermostat_temperature_config.get(CONF_STEP),
        )
        cg.add(numb.set_micronova_object(mv))
        cg.add(mv.register_micronova_listener(numb))
        cg.add(
            numb.set_memory_location(
                thermostat_temperature_config[CONF_MEMORY_LOCATION]
            )
        )
        cg.add(
            numb.set_memory_address(thermostat_temperature_config[CONF_MEMORY_ADDRESS])
        )
        cg.add(
            numb.set_memory_write_location(
                thermostat_temperature_config.get(CONF_MEMORY_WRITE_LOCATION)
            )
        )
        cg.add(
            numb.set_function(MicroNovaFunctions.STOVE_FUNCTION_THERMOSTAT_TEMPERATURE)
        )

    if power_level_config := config.get(CONF_POWER_LEVEL):
        numb = await number.new_number(
            power_level_config,
            min_value=1,
            max_value=5,
            step=1,
        )
        cg.add(numb.set_micronova_object(mv))
        cg.add(mv.register_micronova_listener(numb))
        cg.add(numb.set_memory_location(power_level_config[CONF_MEMORY_LOCATION]))
        cg.add(numb.set_memory_address(power_level_config[CONF_MEMORY_ADDRESS]))
        cg.add(
            numb.set_memory_write_location(
                power_level_config.get(CONF_MEMORY_WRITE_LOCATION)
            )
        )
        cg.add(numb.set_function(MicroNovaFunctions.STOVE_FUNCTION_POWER_LEVEL))
