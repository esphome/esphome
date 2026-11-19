import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_STEP, DEVICE_CLASS_TEMPERATURE, UNIT_CELSIUS
from esphome.types import ConfigType

from .. import (
    CONF_MICRONOVA_ID,
    MICRONOVA_ADDRESS_SCHEMA,
    MicroNova,
    MicroNovaListener,
    micronova_ns,
    register_micronova_writer,
    to_code_micronova_listener,
)

ICON_FLASH = "mdi:flash"

CONF_THERMOSTAT_TEMPERATURE = "thermostat_temperature"
CONF_POWER_LEVEL = "power_level"
CONF_POWER_LEVEL_OFFSET = "power_level_offset"
CONF_RAM_WRITE_LOCATION = "ram_write_location"
CONF_RAM_WRITE_ADDRESS = "ram_write_address"

POWER_LEVEL_MAX = 5

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
        )
        .extend(
            MICRONOVA_ADDRESS_SCHEMA(
                default_memory_location=0x20,
                default_memory_address=0x7F,
                is_polling_component=True,
            )
        )
        .extend(
            {
                # Some boards store the power level with a fixed offset
                # (e.g. 136 -> raw 137..141 for levels 1..5)
                cv.Optional(CONF_POWER_LEVEL_OFFSET, default=0): cv.int_range(
                    min=0, max=0xFF - POWER_LEVEL_MAX
                ),
                # Optional second write of the same raw value, e.g. to the RAM
                # copy of the EEPROM setting so the stove applies it immediately
                cv.Optional(CONF_RAM_WRITE_LOCATION, default=0x00): cv.hex_int_range(
                    min=0x00, max=0x79
                ),
                cv.Optional(CONF_RAM_WRITE_ADDRESS): cv.hex_int_range(
                    min=0x00, max=0xFF
                ),
            }
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    mv = await cg.get_variable(config[CONF_MICRONOVA_ID])

    if thermostat_temperature_config := config.get(CONF_THERMOSTAT_TEMPERATURE):
        register_micronova_writer()
        numb = await number.new_number(
            thermostat_temperature_config,
            mv,
            min_value=0,
            max_value=40,
            step=thermostat_temperature_config.get(CONF_STEP),
        )
        await to_code_micronova_listener(mv, numb, thermostat_temperature_config)
        cg.add(numb.set_use_step_scaling(True))

    if power_level_config := config.get(CONF_POWER_LEVEL):
        register_micronova_writer()
        numb = await number.new_number(
            power_level_config,
            mv,
            min_value=1,
            max_value=POWER_LEVEL_MAX,
            step=1,
        )
        await to_code_micronova_listener(mv, numb, power_level_config)
        if offset := power_level_config[CONF_POWER_LEVEL_OFFSET]:
            cg.add(numb.set_value_offset(offset))
        if (ram_address := power_level_config.get(CONF_RAM_WRITE_ADDRESS)) is not None:
            cg.add(
                numb.set_mirror_write(
                    power_level_config[CONF_RAM_WRITE_LOCATION], ram_address
                )
            )
