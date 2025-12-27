import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_STEP, DEVICE_CLASS_TEMPERATURE, UNIT_CELSIUS

from .. import (
    CONF_MICRONOVA_ID,
    MICRONOVA_ADDRESS_SCHEMA,
    MicroNova,
    MicroNovaListener,
    micronova_ns,
    to_code_micronova_listener,
)

ICON_FLASH = "mdi:flash"

CONF_THERMOSTAT_TEMPERATURE = "thermostat_temperature"
CONF_POWER_LEVEL = "power_level"

MicroNovaNumber = micronova_ns.class_(
    "MicroNovaNumber", number.Number, MicroNovaListener
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MICRONOVA_ID): cv.use_id(MicroNova),
        cv.Optional(CONF_THERMOSTAT_TEMPERATURE): number.number_schema(
            MicroNovaNumber,
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
        )
        .extend(
            MICRONOVA_ADDRESS_SCHEMA(
                default_memory_location=0x20,
                default_memory_address=0x7D,
                is_polling_component=True,
            )
        )
        .extend(
            {
                cv.Optional(CONF_STEP, default=1.0): cv.float_range(min=0.1, max=10.0),
            }
        ),
        cv.Optional(CONF_POWER_LEVEL): number.number_schema(
            MicroNovaNumber,
            icon=ICON_FLASH,
        ).extend(
            MICRONOVA_ADDRESS_SCHEMA(
                default_memory_location=0x20,
                default_memory_address=0x7F,
                is_polling_component=True,
            )
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
        await to_code_micronova_listener(mv, numb, thermostat_temperature_config)
        cg.add(numb.set_micronova_object(mv))
        cg.add(numb.set_use_step_scaling(True))

    if power_level_config := config.get(CONF_POWER_LEVEL):
        numb = await number.new_number(
            power_level_config,
            min_value=1,
            max_value=5,
            step=1,
        )
        await to_code_micronova_listener(mv, numb, power_level_config)
        cg.add(numb.set_micronova_object(mv))
