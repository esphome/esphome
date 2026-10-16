import math

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import binary_sensor, esp32, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PIN,
    CONF_PULLDOWN,
    CONF_SENSOR_ID,
    CONF_TEMPERATURE,
    CONF_TYPE,
)
from esphome.types import ConfigType

CODEOWNERS = ["@tomtom13"]
DEPENDENCIES = ["esp32"]
DOMAIN = "one_wire_device"
CONF_INTERRUPT_MODE = "interrupt_mode"
CONF_PIO_A = "pio_a"
CONF_PIO_B = "pio_b"
CONF_TIMING_SCALE = "timing_scale"
FAMILIES = {
    "DS18B20": 0x28,
    "DS1822": 0x22,
    "DS18S20": 0x10,
    "DS2401": 0x01,
    "DS2413": 0x3A,
}
ns = cg.esphome_ns.namespace(DOMAIN)
OneWireDeviceComponent = ns.class_("OneWireDeviceComponent", cg.Component)
SetTemperatureAction = ns.class_("SetTemperatureAction", automation.Action)


def _crc8(data: bytes) -> int:
    crc = 0
    for value in data:
        for _ in range(8):
            mix = (crc ^ value) & 1
            crc >>= 1
            if mix:
                crc ^= 0x8C
            value >>= 1
    return crc


def validate_rom_address(value: int, family: int = 0x28) -> int:
    value = int(value)
    if value == 0:
        return value
    raw = value.to_bytes(8, "little")
    if raw[0] != family:
        raise ValueError(f"ROM family code must match selected type (0x{family:02x})")
    if _crc8(raw[:7]) != raw[7]:
        raise ValueError("1-Wire device ROM CRC is invalid")
    return value


def validate_temperature(value):
    value = cv.float_(value)
    if not math.isfinite(value):
        raise cv.Invalid("temperature must be finite")
    return cv.float_range(min=-55.0, max=125.0)(value)


def validate_pin(config):
    mode = config[CONF_MODE]
    if config[CONF_INVERTED] or mode[CONF_PULLDOWN]:
        raise cv.Invalid("1-Wire DQ must not be inverted or use pulldown")
    if not all(mode[key] for key in (CONF_INPUT, CONF_OUTPUT, CONF_OPEN_DRAIN)):
        raise cv.Invalid("1-Wire DQ requires input, output and open_drain mode")
    return config


def validate_config(config):
    try:
        validate_rom_address(config[CONF_ADDRESS], FAMILIES[config[CONF_TYPE]])
    except ValueError as error:
        raise cv.Invalid(str(error), [CONF_ADDRESS]) from error
    thermometer = config[CONF_TYPE] in ("DS18B20", "DS1822", "DS18S20")
    if not thermometer and any(
        key in config for key in (CONF_TEMPERATURE, CONF_SENSOR_ID)
    ):
        raise cv.Invalid("temperature and sensor_id require a thermometer type")
    if config[CONF_TYPE] != "DS2413" and any(
        key in config for key in (CONF_PIO_A, CONF_PIO_B)
    ):
        raise cv.Invalid("pio_a and pio_b require type: DS2413")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OneWireDeviceComponent),
            cv.Required(CONF_PIN): cv.All(
                pins.gpio_pin_schema(
                    {CONF_INPUT: True, CONF_OUTPUT: True, CONF_OPEN_DRAIN: True},
                    internal=True,
                ),
                validate_pin,
            ),
            cv.Optional(CONF_TYPE, default="DS18B20"): cv.one_of(*FAMILIES, upper=True),
            cv.Optional(CONF_ADDRESS, default=0): cv.hex_uint64_t,
            cv.Optional(CONF_TEMPERATURE): validate_temperature,
            cv.Optional(CONF_SENSOR_ID): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_PIO_A): cv.use_id(binary_sensor.BinarySensor),
            cv.Optional(CONF_PIO_B): cv.use_id(binary_sensor.BinarySensor),
            cv.Optional(CONF_TIMING_SCALE): cv.All(cv.int_, cv.Range(min=1, max=1)),
            cv.Optional(CONF_INTERRUPT_MODE): cv.All(cv.boolean, cv.one_of(True)),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    esp32.only_on_variant(supported=[esp32.VARIANT_ESP32]),
    cv.has_at_most_one_key(CONF_TEMPERATURE, CONF_SENSOR_ID),
    validate_config,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_pin(await cg.gpio_pin_expression(config[CONF_PIN])))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_family(FAMILIES[config[CONF_TYPE]]))
    if CONF_SENSOR_ID in config:
        cg.add(var.set_source_sensor(await cg.get_variable(config[CONF_SENSOR_ID])))
    elif config[CONF_TYPE] in ("DS18B20", "DS1822", "DS18S20"):
        cg.add(var.set_temperature(config.get(CONF_TEMPERATURE, 23.5)))
    for index, key in enumerate((CONF_PIO_A, CONF_PIO_B)):
        if key in config:
            cg.add(var.set_pio_source(index, await cg.get_variable(config[key])))


@automation.register_action(
    "one_wire_device.set_temperature",
    SetTemperatureAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(OneWireDeviceComponent),
            cv.Required(CONF_TEMPERATURE): cv.templatable(validate_temperature),
        }
    ),
    synchronous=True,
)
async def set_temperature_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    cg.add(
        var.set_temperature(
            await cg.templatable(config[CONF_TEMPERATURE], args, cg.float_)
        )
    )
    return var
