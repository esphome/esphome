from esphome import automation, pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_DATA,
    CONF_ID,
    CONF_ON_MESSAGE,
    CONF_PIN,
    CONF_SOURCE,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_LN882X,
    PLATFORM_RP2,
    PLATFORM_RTL87XX,
)
from esphome.types import ConfigType

CODEOWNERS = ["@JosVanEijndhoven"]
DOMAIN = "hdmi_cec"

MAX_LOGICAL_ADDRESS = 15
MAX_PAYLOAD_LENGTH = 15
CONF_PROMISCUOUS_MODE = "promiscuous_mode"
CONF_MONITOR_MODE = "monitor_mode"
CONF_DESTINATION = "destination"
CONF_OPCODE = "opcode"

hdmi_cec_ns = cg.esphome_ns.namespace("hdmi_cec")
HDMICEC = hdmi_cec_ns.class_("HDMICEC", cg.Component)
_PAYLOAD_SPAN = cg.std_span.template(cg.uint8.operator("const"))
_PAYLOAD_VECTOR = cg.std_vector.template(cg.uint8)
_MESSAGE_ARGS = [
    (cg.uint8, "source"),
    (cg.uint8, "destination"),
    (_PAYLOAD_SPAN, "data"),
]

_request_message_callback_slot = cg.slot_counter("HDMI_CEC_MESSAGE_CALLBACK_COUNT")

logical_address = cv.int_range(min=0, max=MAX_LOGICAL_ADDRESS)
payload = cv.All(cv.ensure_list(cv.hex_uint8_t), cv.Length(max=MAX_PAYLOAD_LENGTH))
payload_prefix = cv.All(
    cv.ensure_list(cv.hex_uint8_t), cv.Length(min=1, max=MAX_PAYLOAD_LENGTH)
)


def _validate_message_filter(config: ConfigType) -> ConfigType:
    # The opcode is the first payload byte, so both filters must agree on it
    if (
        (opcode := config.get(CONF_OPCODE)) is not None
        and (data := config.get(CONF_DATA))
        and data[0] != opcode
    ):
        raise cv.Invalid(
            f"opcode 0x{opcode:02X} differs from the first data byte 0x{data[0]:02X}"
        )
    return config


def _message_filter(conf: ConfigType) -> str | None:
    """The C++ condition an on_message automation runs under, or None for every frame."""
    checks = []
    if (source := conf.get(CONF_SOURCE)) is not None:
        checks.append(f"source == {source}")
    if (destination := conf.get(CONF_DESTINATION)) is not None:
        checks.append(f"destination == {destination}")
    if (opcode := conf.get(CONF_OPCODE)) is not None:
        checks.append(f"data[0] == {opcode}")
    if data := conf.get(CONF_DATA):
        # The filter is a format template, so the braces are doubled
        prefix = ", ".join(f"0x{byte:02X}" for byte in data)
        checks.append(f"hdmi_cec::starts_with(data, {{{{{prefix}}}}})")
    return " && ".join(checks) or None


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HDMICEC),
            cv.Required(CONF_PIN): pins.internal_gpio_input_pullup_pin_schema,
            cv.Required(CONF_ADDRESS): logical_address,
            cv.Optional(CONF_PROMISCUOUS_MODE, default=False): cv.boolean,
            cv.Optional(CONF_MONITOR_MODE, default=False): cv.boolean,
            cv.Optional(CONF_ON_MESSAGE): automation.validate_automation(
                {
                    cv.Optional(CONF_SOURCE): logical_address,
                    cv.Optional(CONF_DESTINATION): logical_address,
                    cv.Optional(CONF_OPCODE): cv.hex_uint8_t,
                    cv.Optional(CONF_DATA): payload_prefix,
                },
                _validate_message_filter,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on(
        [
            PLATFORM_ESP32,
            PLATFORM_ESP8266,
            PLATFORM_RP2,
            PLATFORM_BK72XX,
            PLATFORM_RTL87XX,
            PLATFORM_LN882X,
        ]
    ),
)


async def to_code(config: ConfigType) -> None:
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    var = cg.new_Pvariable(config[CONF_ID], pin, config[CONF_ADDRESS])
    await cg.register_component(var, config)
    if config[CONF_PROMISCUOUS_MODE]:
        cg.add(var.set_promiscuous_mode(True))
    if config[CONF_MONITOR_MODE]:
        cg.add(var.set_monitor_mode(True))

    for conf in config.get(CONF_ON_MESSAGE, []):
        _request_message_callback_slot()
        await automation.build_callback_automation(
            var,
            "add_on_message_callback",
            _MESSAGE_ARGS,
            conf,
            when=_message_filter(conf),
        )


automation.register_apply_action(
    "hdmi_cec.send",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HDMICEC),
            cv.Required(CONF_DESTINATION): cv.templatable(logical_address),
            cv.Required(CONF_DATA): cv.templatable(payload),
        }
    ),
    automation.ApplyCall(
        "send({}, {})",
        ((CONF_DESTINATION, cg.uint8), (CONF_DATA, _PAYLOAD_VECTOR)),
    ),
)
