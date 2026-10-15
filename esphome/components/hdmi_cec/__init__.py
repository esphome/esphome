from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import uart
from esphome.components.esp32 import include_builtin_idf_component
from esphome.components.logger import UART1
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_DATA,
    CONF_HARDWARE_UART,
    CONF_ID,
    CONF_LOGGER,
    CONF_NUMBER,
    CONF_ON_MESSAGE,
    CONF_PIN,
    CONF_RX_PIN,
    CONF_SOURCE,
    CONF_TX_PIN,
    CONF_UART_ID,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_LN882X,
    PLATFORM_RP2,
    PLATFORM_RTL87XX,
)
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@JosVanEijndhoven"]
DOMAIN = "hdmi_cec"

MAX_LOGICAL_ADDRESS = 15
MAX_PAYLOAD_LENGTH = 15
MAX_OSD_NAME_LENGTH = MAX_PAYLOAD_LENGTH - 1  # the opcode takes one byte
UART_BAUD_RATE = 2083  # five UART bits per 2.4 ms CEC bit
CONF_PROMISCUOUS_MODE = "promiscuous_mode"
CONF_MONITOR_MODE = "monitor_mode"
CONF_DESTINATION = "destination"
CONF_OPCODE = "opcode"
CONF_PHYSICAL_ADDRESS = "physical_address"
CONF_OSD_NAME = "osd_name"

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
payload_prefix = cv.All(payload, cv.Length(min=1))


def _osd_name(value: object) -> str:
    # The name goes on the bus as plain ASCII, one byte per character
    value = cv.All(cv.string_strict, cv.Length(min=1, max=MAX_OSD_NAME_LENGTH))(value)
    if not all(0x20 <= ord(char) <= 0x7E for char in value):
        raise cv.Invalid("osd_name may only contain printable ASCII characters")
    return value


def _validate_message_filter(config: ConfigType) -> ConfigType:
    # The opcode is the first payload byte: fold it into the data prefix
    if (opcode := config.pop(CONF_OPCODE, None)) is not None:
        data = config.setdefault(CONF_DATA, [opcode])
        if data[0] != opcode:
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
    if data := conf.get(CONF_DATA):
        checks.append(f"data.size() >= {len(data)}")
        checks.extend(f"data[{i}] == 0x{byte:02X}" for i, byte in enumerate(data))
    return " && ".join(checks) or None


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HDMICEC),
            cv.Required(CONF_PIN): pins.internal_gpio_input_pullup_pin_schema,
            cv.Required(CONF_ADDRESS): logical_address,
            cv.Optional(CONF_PHYSICAL_ADDRESS): cv.hex_uint16_t,
            cv.Optional(CONF_OSD_NAME): _osd_name,
            cv.Optional(CONF_UART_ID): cv.use_id(uart.UARTComponent),
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


# On the ESP8266 only UART1, transmit only on GPIO2, is free of the logger; UART0 carries the log
# and any other pin gets a software UART that sends with interrupts off, which holds the main loop
# and hides our own frame from the receive interrupt
ESP8266_UART1_TX_PIN = 2


def _esp8266_uart1(hub_config: ConfigType) -> ConfigType:
    if (
        hub_config[CONF_TX_PIN][CONF_NUMBER] != ESP8266_UART1_TX_PIN
        or CONF_RX_PIN in hub_config
    ):
        raise cv.Invalid(
            f"{DOMAIN} needs the hardware UART1 on the ESP8266: tx_pin GPIO2 and no rx_pin"
        )
    logger = fv.full_config.get().get(CONF_LOGGER, {})
    if logger.get(CONF_HARDWARE_UART) == UART1:
        raise cv.Invalid(f"{DOMAIN} needs UART1 on the ESP8266, which the logger uses")
    return hub_config


def _final_validate(config: ConfigType) -> ConfigType:
    if CONF_UART_ID in config:
        # The UART only drives the line, through a diode; its settings make one CEC bit of five UART bits
        uart.final_validate_device_schema(
            DOMAIN,
            baud_rate=UART_BAUD_RATE,
            require_tx=True,
            data_bits=8,
            parity="NONE",
            stop_bits=1,
        )(config)
        if CORE.is_esp8266:
            cv.Schema(
                {
                    cv.Required(CONF_UART_ID): fv.id_declaration_match_schema(
                        _esp8266_uart1
                    )
                },
                extra=cv.ALLOW_EXTRA,
            )(config)
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    if CORE.is_esp32:
        # The acknowledge bit is released from a general purpose timer, left out of the build by default
        include_builtin_idf_component("esp_driver_gptimer")
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    var = cg.new_Pvariable(config[CONF_ID], pin, config[CONF_ADDRESS])
    await cg.register_component(var, config)
    if config[CONF_PROMISCUOUS_MODE]:
        cg.add(var.set_promiscuous_mode(True))
    if config[CONF_MONITOR_MODE]:
        cg.add(var.set_monitor_mode(True))
    if (physical_address := config.get(CONF_PHYSICAL_ADDRESS)) is not None:
        cg.add(var.set_physical_address(physical_address))
    if (osd_name := config.get(CONF_OSD_NAME)) is not None:
        cg.add(var.set_osd_name(cg.shared_progmem_string(osd_name), len(osd_name)))
    if (uart_id := config.get(CONF_UART_ID)) is not None:
        cg.add_define("USE_HDMI_CEC_UART")
        cg.add(var.set_uart(await cg.get_variable(uart_id)))

    on_message = config.get(CONF_ON_MESSAGE, [])
    # An automation for frames addressed to us with a data prefix claims its opcode from every
    # initiator, whatever its source filter or the rest of its prefix, so the built-in replies never
    # contradict an answer it sends. One without a prefix sees every such frame, so nothing is
    # unhandled and no Feature Abort is sent
    address = config[CONF_ADDRESS]
    for_us = [
        conf for conf in on_message if conf.get(CONF_DESTINATION, address) == address
    ]
    claimed_opcodes = sorted(
        {conf[CONF_DATA][0] for conf in for_us if CONF_DATA in conf}
    )
    if claimed_opcodes:
        table = cg.shared_progmem_array(
            "hdmi_cec_claimed_opcodes", cg.uint8, claimed_opcodes
        )
        cg.add(var.set_claimed_opcodes(table, len(claimed_opcodes)))
    if all(CONF_DATA in conf for conf in for_us):
        cg.add_define("USE_HDMI_CEC_FEATURE_ABORT")

    for conf in on_message:
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

# A frame under another logical address, for a device that stands in for several
automation.register_apply_action(
    "hdmi_cec.send_as",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HDMICEC),
            cv.Required(CONF_SOURCE): cv.templatable(logical_address),
            cv.Required(CONF_DESTINATION): cv.templatable(logical_address),
            cv.Required(CONF_DATA): cv.templatable(payload),
        }
    ),
    automation.ApplyCall(
        "send({}, {}, {})",
        (
            (CONF_SOURCE, cg.uint8),
            (CONF_DESTINATION, cg.uint8),
            (CONF_DATA, _PAYLOAD_VECTOR),
        ),
    ),
)
