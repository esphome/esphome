import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_CURRENT,
    CONF_ENERGY,
    CONF_EXTERNAL_TEMPERATURE,
    CONF_ID,
    CONF_INTERNAL_TEMPERATURE,
    CONF_POWER,
    CONF_REFERENCE_VOLTAGE,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_WATT,
)

from . import bl0940_ns

DEPENDENCIES = ["uart"]

BL0940 = bl0940_ns.class_("BL0940", cg.PollingComponent, uart.UARTDevice)

CONF_LEGACY_MODE = "legacy_mode"

CONF_READ_COMMAND = "read_command"
CONF_WRITE_COMMAND = "write_command"

CONF_RESISTOR_SHUNT = "resistor_shunt"
CONF_RESISTOR_ONE = "resistor_one"
CONF_RESISTOR_TWO = "resistor_two"

CONF_CURRENT_REFERENCE = "current_reference"
CONF_ENERGY_REFERENCE = "energy_reference"
CONF_POWER_REFERENCE = "power_reference"
CONF_VOLTAGE_REFERENCE = "voltage_reference"

DEFAULT_BL0940_READ_COMMAND = 0x58
DEFAULT_BL0940_WRITE_COMMAND = 0xA1

# Values according to BL0940 application note:
# https://www.belling.com.cn/media/file_object/bel_product/BL0940/guide/BL0940_APPNote_TSSOP14_V1.04_EN.pdf
DEFAULT_BL0940_VREF = 1.218  # Vref = 1.218
DEFAULT_BL0940_RL = 1  # RL = 1 mΩ
DEFAULT_BL0940_R1 = 0.51  # R1 = 0.51 kΩ
DEFAULT_BL0940_R2 = 1950  # R2 = 5 x 390 kΩ -> 1950 kΩ

# ----------------------------------------------------
# values from initial implementation
DEFAULT_BL0940_LEGACY_READ_COMMAND = 0x50
DEFAULT_BL0940_LEGACY_WRITE_COMMAND = 0xA0

DEFAULT_BL0940_LEGACY_UREF = 33000
DEFAULT_BL0940_LEGACY_IREF = 275000
DEFAULT_BL0940_LEGACY_PREF = 1430
# Measured to 297J  per click according to power consumption of 5 minutes
# Converted to kWh (3.6MJ per kwH). Used to be 256 * 1638.4
DEFAULT_BL0940_LEGACY_EREF = 3.6e6 / 297
# ----------------------------------------------------


# methods to calculate voltage and current reference values
def calculate_voltage_reference(vref, r_one, r_two):
    # formula: 79931 / Vref * (R1 * 1000) / (R1 + R2)
    return 79931 / vref * (r_one * 1000) / (r_one + r_two)


def calculate_current_reference(vref, r_shunt):
    # formula: 324004 * RL / Vref
    return 324004 * r_shunt / vref


def calculate_power_reference(voltage_reference, current_reference):
    # calculate power reference based on voltage and current reference
    return voltage_reference * current_reference * 4046 / 324004 / 79931


def calculate_energy_reference(power_reference):
    # formula: power_reference * 3600000 / (1638.4 * 256)
    return power_reference * 3600000 / (1638.4 * 256)


def validate_legacy_mode(config):
    # Only allow schematic calibration options if legacy_mode is False
    if config.get(CONF_LEGACY_MODE, True):
        forbidden = [
            CONF_REFERENCE_VOLTAGE,
            CONF_RESISTOR_SHUNT,
            CONF_RESISTOR_ONE,
            CONF_RESISTOR_TWO,
        ]
        for key in forbidden:
            if key in config:
                raise cv.Invalid(
                    f"Option '{key}' is only allowed when legacy_mode: false"
                )
    return config


def set_command_defaults(config):
    # Set defaults for read_command and write_command based on legacy_mode
    legacy = config.get(CONF_LEGACY_MODE, True)
    if legacy:
        config.setdefault(CONF_READ_COMMAND, DEFAULT_BL0940_LEGACY_READ_COMMAND)
        config.setdefault(CONF_WRITE_COMMAND, DEFAULT_BL0940_LEGACY_WRITE_COMMAND)
    else:
        config.setdefault(CONF_READ_COMMAND, DEFAULT_BL0940_READ_COMMAND)
        config.setdefault(CONF_WRITE_COMMAND, DEFAULT_BL0940_WRITE_COMMAND)
    return config


def set_reference_values(config):
    # Set default reference values based on legacy_mode
    if config.get(CONF_LEGACY_MODE, True):
        config.setdefault(CONF_VOLTAGE_REFERENCE, DEFAULT_BL0940_LEGACY_UREF)
        config.setdefault(CONF_CURRENT_REFERENCE, DEFAULT_BL0940_LEGACY_IREF)
        config.setdefault(CONF_POWER_REFERENCE, DEFAULT_BL0940_LEGACY_PREF)
        config.setdefault(CONF_ENERGY_REFERENCE, DEFAULT_BL0940_LEGACY_PREF)
    else:
        vref = config.get(CONF_VOLTAGE_REFERENCE, DEFAULT_BL0940_VREF)
        r_one = config.get(CONF_RESISTOR_ONE, DEFAULT_BL0940_R1)
        r_two = config.get(CONF_RESISTOR_TWO, DEFAULT_BL0940_R2)
        r_shunt = config.get(CONF_RESISTOR_SHUNT, DEFAULT_BL0940_RL)

        config.setdefault(
            CONF_VOLTAGE_REFERENCE, calculate_voltage_reference(vref, r_one, r_two)
        )
        config.setdefault(
            CONF_CURRENT_REFERENCE, calculate_current_reference(vref, r_shunt)
        )
        config.setdefault(
            CONF_POWER_REFERENCE,
            calculate_power_reference(
                config.get(CONF_VOLTAGE_REFERENCE), config.get(CONF_CURRENT_REFERENCE)
            ),
        )
        config.setdefault(
            CONF_ENERGY_REFERENCE,
            calculate_energy_reference(config.get(CONF_POWER_REFERENCE)),
        )

    return config


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BL0940),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_INTERNAL_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_EXTERNAL_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_LEGACY_MODE, default=True): cv.boolean,
            cv.Optional(CONF_READ_COMMAND): cv.hex_uint8_t,
            cv.Optional(CONF_WRITE_COMMAND): cv.hex_uint8_t,
            cv.Optional(CONF_REFERENCE_VOLTAGE): cv.float_,
            cv.Optional(CONF_RESISTOR_SHUNT): cv.float_,
            cv.Optional(CONF_RESISTOR_ONE): cv.float_,
            cv.Optional(CONF_RESISTOR_TWO): cv.float_,
            cv.Optional(CONF_CURRENT_REFERENCE): cv.float_,
            cv.Optional(CONF_ENERGY_REFERENCE): cv.float_,
            cv.Optional(CONF_POWER_REFERENCE): cv.float_,
            cv.Optional(CONF_VOLTAGE_REFERENCE): cv.float_,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
    .add_extra(validate_legacy_mode)
    .add_extra(set_command_defaults)
    .add_extra(set_reference_values)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if voltage_config := config.get(CONF_VOLTAGE):
        sens = await sensor.new_sensor(voltage_config)
        cg.add(var.set_voltage_sensor(sens))
    if current_config := config.get(CONF_CURRENT):
        sens = await sensor.new_sensor(current_config)
        cg.add(var.set_current_sensor(sens))
    if power_config := config.get(CONF_POWER):
        sens = await sensor.new_sensor(power_config)
        cg.add(var.set_power_sensor(sens))
    if energy_config := config.get(CONF_ENERGY):
        sens = await sensor.new_sensor(energy_config)
        cg.add(var.set_energy_sensor(sens))
    if internal_temperature_config := config.get(CONF_INTERNAL_TEMPERATURE):
        sens = await sensor.new_sensor(internal_temperature_config)
        cg.add(var.set_internal_temperature_sensor(sens))
    if external_temperature_config := config.get(CONF_EXTERNAL_TEMPERATURE):
        sens = await sensor.new_sensor(external_temperature_config)
        cg.add(var.set_external_temperature_sensor(sens))

    # enable legacy mode
    cg.add(var.set_legacy_mode(config.get(CONF_LEGACY_MODE)))

    # Set bl0940 commands after validator has determined which defaults to use if not set
    cg.add(var.set_read_command(config.get(CONF_READ_COMMAND)))
    cg.add(var.set_write_command(config.get(CONF_WRITE_COMMAND)))

    # Set reference values after validator has set the values either from defaults or calculated
    cg.add(var.set_current_reference(config.get(CONF_CURRENT_REFERENCE)))
    cg.add(var.set_voltage_reference(config.get(CONF_VOLTAGE_REFERENCE)))
    cg.add(var.set_power_reference(config.get(CONF_POWER_REFERENCE)))
    cg.add(var.set_energy_reference(config.get(CONF_ENERGY_REFERENCE)))
