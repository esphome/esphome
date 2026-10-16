from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_PIN, CONF_TEMPERATURE
from esphome.types import ConfigType

CODEOWNERS = ["@tomtom13"]
DEPENDENCIES = ["esp32"]
DOMAIN = "one_wire_device"

CONF_TIMING_SCALE = "timing_scale"
CONF_INTERRUPT_MODE = "interrupt_mode"

one_wire_device_ns = cg.esphome_ns.namespace("one_wire_device")
OneWireDeviceComponent = one_wire_device_ns.class_(
    "OneWireDeviceComponent", cg.Component
)


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


def validate_rom_address(value: int) -> int:
    """Validate a little-endian 64-bit DS18B20 ROM address.

    Zero is reserved for the runtime-generated address derived from the ESP32
    station MAC address.
    """
    value = int(value)
    if value == 0:
        return value
    raw = value.to_bytes(8, "little")
    if raw[0] != 0x28:
        raise ValueError("1-Wire device ROM must use DS18B20 family code 0x28")
    if _crc8(raw[:7]) != raw[7]:
        raise ValueError("1-Wire device ROM CRC is invalid")
    return value


def validate_timing_scale(value: int) -> int:
    value = int(value)
    if not 1 <= value <= 100:
        raise ValueError("timing_scale must be between 1 and 100")
    return value


def _validate_rom_for_schema(value: int) -> int:
    try:
        return validate_rom_address(value)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err


def encode_scratchpad(
    temperature: float, resolution: int, high_alarm: int = 75, low_alarm: int = 70
) -> bytes:
    """Encode a DS18B20 scratchpad for protocol-model tests and tooling."""
    raw = int(round(max(-55.0, min(125.0, temperature)) * 16.0))
    raw &= ~((1 << (12 - resolution)) - 1)
    config = {9: 0x1F, 10: 0x3F, 11: 0x5F, 12: 0x7F}[resolution]
    scratchpad = bytes(
        (
            raw & 0xFF,
            (raw >> 8) & 0xFF,
            high_alarm,
            low_alarm,
            config,
            0xFF,
            0x0C,
            0x10,
        )
    )
    return scratchpad + bytes((_crc8(scratchpad),))


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OneWireDeviceComponent),
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_ADDRESS, default=0): cv.All(
                cv.hex_uint64_t, _validate_rom_for_schema
            ),
            cv.Optional(CONF_TEMPERATURE, default=23.5): cv.float_range(
                min=-55.0, max=125.0
            ),
            cv.Optional(CONF_TIMING_SCALE, default=1): validate_timing_scale,
            cv.Optional(CONF_INTERRUPT_MODE, default=True): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_temperature(config[CONF_TEMPERATURE]))
    cg.add(var.set_timing_scale(config[CONF_TIMING_SCALE]))
    cg.add(var.set_interrupt_mode(config[CONF_INTERRUPT_MODE]))
