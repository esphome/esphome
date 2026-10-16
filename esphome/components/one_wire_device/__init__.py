import math

from esphome import automation, final_validate as fv, pins
import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_CHANNEL,
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PIN,
    CONF_PULLDOWN,
    CONF_TYPE,
    CONF_VALUE,
)
from esphome.core import CORE, Lambda
from esphome.types import ConfigType

CODEOWNERS = ["@tomtom13"]
DEPENDENCIES = ["esp32"]
DOMAIN = "one_wire_device"
CONF_INTERRUPT_MODE = "interrupt_mode"
CONF_TIMING_SCALE = "timing_scale"
CONF_VALUES = "values"
FAMILIES = {
    "DS18B20": 0x28,
    "DS1822": 0x22,
    "DS18S20": 0x10,
    "DS2401": 0x01,
    "DS2413": 0x3A,
}
# Names are quantities/channels, not virtual endpoints. The model owns their meaning and units.
CHANNELS = {kind: {"temperature": 0} for kind in ("DS18B20", "DS1822", "DS18S20")}
CHANNELS.update({"DS2401": {}, "DS2413": {"pio_a": 1, "pio_b": 2}})
ns = cg.esphome_ns.namespace(DOMAIN)
OneWireDeviceComponent = ns.class_("OneWireDeviceComponent", cg.PollingComponent)
SetValueAction = ns.class_("SetValueAction", automation.Action)


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


def validate_pin(config):
    mode = config[CONF_MODE]
    if config[CONF_INVERTED] or mode[CONF_PULLDOWN]:
        raise cv.Invalid("1-Wire DQ must not be inverted or use pulldown")
    if not all(mode[key] for key in (CONF_INPUT, CONF_OUTPUT, CONF_OPEN_DRAIN)):
        raise cv.Invalid("1-Wire DQ requires input, output and open_drain mode")
    return config


def channel_name(kind, supplied=None):
    channels = CHANNELS[kind]
    if not channels:
        raise cv.Invalid(f"{kind} has no value channels")
    if supplied is None:
        if len(channels) != 1:
            raise cv.Invalid(
                f"channel is required for {kind}: choose {', '.join(channels)}"
            )
        return next(iter(channels))
    if supplied not in channels:
        raise cv.Invalid(
            f"{kind} does not support value channel '{supplied}'; choose {', '.join(channels)}"
        )
    return supplied


def validate_value(kind, channel, value):
    channel_name(kind, channel)
    if isinstance(value, Lambda):
        return value
    if not math.isfinite(value):
        raise cv.Invalid("value must be finite")
    if kind == "DS2413":
        if value not in (0, 1):
            raise cv.Invalid("digital value must be 0 or 1")
    elif not -55 <= value <= 125:
        raise cv.Invalid("temperature value must be between -55 and 125 C")
    return value


def validate_config(config):
    kind = config[CONF_TYPE]
    try:
        validate_rom_address(config[CONF_ADDRESS], FAMILIES[kind])
    except ValueError as error:
        raise cv.Invalid(str(error), [CONF_ADDRESS]) from error
    if CONF_VALUE in config:
        name = channel_name(kind)
        config[CONF_VALUES] = {name: config.pop(CONF_VALUE)}
    values = config.get(CONF_VALUES, {})
    for name, value in values.items():
        validate_value(kind, name, value)
    # Explicit model defaults are applied once. A configured lambda instead starts unavailable.
    defaults = dict.fromkeys(CHANNELS[kind], 1.0 if kind == "DS2413" else 23.5)
    config[CONF_VALUES] = {**defaults, **values}
    return config


def validate_action(config, kind):
    name = channel_name(kind, config.get(CONF_CHANNEL))
    validate_value(kind, name, config[CONF_VALUE])
    return CHANNELS[kind][name]


def validate_all_actions(config):
    def walk(node):
        if isinstance(node, dict):
            for key, value in node.items():
                if key == "one_wire_device.set_value" and str(value[CONF_ID]) == str(
                    config[CONF_ID]
                ):
                    validate_action(value, config[CONF_TYPE])
                else:
                    walk(value)
        elif isinstance(node, list):
            for item in node:
                walk(item)

    walk(fv.full_config.get())
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
            cv.Optional(CONF_VALUE): cv.templatable(cv.float_),
            cv.Optional(CONF_VALUES): cv.Schema(
                {cv.string_strict: cv.templatable(cv.float_)}
            ),
            cv.Optional(CONF_TIMING_SCALE): cv.All(cv.int_, cv.Range(min=1, max=1)),
            cv.Optional(CONF_INTERRUPT_MODE): cv.All(cv.boolean, cv.one_of(True)),
        }
    ).extend(cv.polling_component_schema("1s")),
    esp32.only_on_variant(supported=[esp32.VARIANT_ESP32]),
    cv.has_at_most_one_key(CONF_VALUE, CONF_VALUES),
    validate_config,
)
FINAL_VALIDATE_SCHEMA = validate_all_actions


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_pin(await cg.gpio_pin_expression(config[CONF_PIN])))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_family(FAMILIES[config[CONF_TYPE]]))
    for name, value in config[CONF_VALUES].items():
        channel = CHANNELS[config[CONF_TYPE]][name]
        if isinstance(value, Lambda):
            function = await cg.process_lambda(value, [], return_type=cg.float_)
            cg.add(var.set_value_function(channel, function))
        else:
            cg.add(var.set_value(channel, value))


@automation.register_action(
    "one_wire_device.set_value",
    SetValueAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(OneWireDeviceComponent),
            cv.Optional(CONF_CHANNEL): cv.string_strict,
            cv.Required(CONF_VALUE): cv.templatable(cv.float_),
        }
    ),
    synchronous=True,
)
async def set_value_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    channel = validate_action(config, CORE.config[DOMAIN][CONF_TYPE])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    cg.add(var.set_channel(channel))
    cg.add(var.set_value(await cg.templatable(config[CONF_VALUE], args, cg.float_)))
    return var
