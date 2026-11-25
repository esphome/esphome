from esphome import automation, final_validate as fv, pins
import esphome.codegen as cg
from esphome.components import esp32, esp32_rmt, i2c, logger, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_DATA,
    CONF_FLOW_CONTROL_PIN,
    CONF_HARDWARE_UART,
    CONF_ID,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_RX_BUFFER_SIZE,
    CONF_RX_PIN,
    CONF_TX_PIN,
    CONF_TYPE,
    CONF_UART_ID,
)
from esphome.core import CORE, ID
from esphome.cpp_generator import MockObj, TemplateArgsType
from esphome.types import ConfigType

CODEOWNERS = ["@inventor7777"]
DOMAIN = "microchip_atsha204a"
MULTI_CONF = True

CONF_ATSHA204A_ID = "atsha204a_id"
CONF_END = "end"
CONF_EXPECT_RESPONSE = "expect_response"
CONF_ON_RESULT = "on_result"
CONF_OPCODE = "opcode"
CONF_PARAM1 = "param1"
CONF_PARAM2 = "param2"
CONF_SESSION = "session"
CONF_TRANSPORT_ID = "transport_id"
CONF_WAKE_PIN = "wake_pin"

I2C = "i2c"
SWI_RMT = "swi_rmt"
SWI_UART = "swi_uart"
SWI_BAUD_RATE = 230400
SWI_MIN_RX_BUFFER_SIZE = 8 * (1 + 84)
MAX_COMMAND_DATA = 77

microchip_atsha204a_ns = cg.esphome_ns.namespace(DOMAIN)
ATSHA204A = microchip_atsha204a_ns.class_("ATSHA204A", cg.PollingComponent)
I2CTransport = microchip_atsha204a_ns.class_("I2CTransport", i2c.I2CDevice)
UARTTransport = microchip_atsha204a_ns.class_("UARTTransport", uart.UARTDevice)
RMTTransport = microchip_atsha204a_ns.class_("RMTTransport", uart.UARTDevice)
CommandAction = microchip_atsha204a_ns.class_("CommandAction", automation.Action)
CommandResponse = microchip_atsha204a_ns.class_("CommandResponse")
Opcode = microchip_atsha204a_ns.enum("Opcode", is_class=True)
EndMode = microchip_atsha204a_ns.enum("EndMode", is_class=True)


def _validate_wake_pin(config: ConfigType) -> ConfigType:
    if config[CONF_INVERTED] or not config[CONF_MODE].get(CONF_OPEN_DRAIN):
        raise cv.Invalid("The wake GPIO must be a non-inverted open-drain output")
    return config


BASE_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(ATSHA204A)}).extend(
    cv.polling_component_schema("never")
)
CONFIG_SCHEMA = cv.typed_schema(
    {
        I2C: BASE_SCHEMA.extend(i2c.i2c_device_schema(0x64)).extend(
            {cv.GenerateID(CONF_TRANSPORT_ID): cv.declare_id(I2CTransport)}
        ),
        SWI_UART: BASE_SCHEMA.extend(uart.UART_DEVICE_SCHEMA).extend(
            {
                cv.GenerateID(CONF_TRANSPORT_ID): cv.declare_id(UARTTransport),
                cv.Required(CONF_WAKE_PIN): cv.All(
                    pins.internal_gpio_output_pin_schema, _validate_wake_pin
                ),
            }
        ),
        SWI_RMT: cv.All(
            BASE_SCHEMA.extend(uart.UART_DEVICE_SCHEMA).extend(
                {cv.GenerateID(CONF_TRANSPORT_ID): cv.declare_id(RMTTransport)}
            ),
            cv.only_on_esp32,
            esp32.only_on_variant(supported=[esp32.VARIANT_ESP32]),
            esp32.require_idf_version(cv.Version(5, 5, 4), SWI_RMT),
            esp32_rmt.request_rmt_channels([("TX", 1)], 600.0),
        ),
    },
    lower=True,
)

_VALIDATE_I2C = i2c.final_validate_device_schema(DOMAIN, max_frequency="100kHz")
_VALIDATE_RMT_UART = uart.final_validate_device_schema(
    DOMAIN,
    baud_rate=SWI_BAUD_RATE,
    data_bits=7,
    parity="NONE",
    stop_bits=1,
    require_rx=True,
)
_VALIDATE_SWI_UART = uart.final_validate_device_schema(
    DOMAIN,
    baud_rate=SWI_BAUD_RATE,
    data_bits=7,
    parity="NONE",
    stop_bits=1,
    require_tx=True,
    require_rx=True,
)
_VALIDATE_UART_BUFFER = fv.id_declaration_match_schema(
    cv.Schema(
        {cv.Required(CONF_RX_BUFFER_SIZE): cv.int_range(min=SWI_MIN_RX_BUFFER_SIZE)},
        extra=cv.ALLOW_EXTRA,
    )
)


def _validate_rmt_wiring(config: ConfigType) -> ConfigType:
    if str(config[CONF_ID].type) != str(uart.IDFUARTComponent):
        raise cv.Invalid("The RMT transport requires a hardware UART")
    for key in (CONF_TX_PIN, CONF_FLOW_CONTROL_PIN):
        if key in config:
            raise cv.Invalid("The RMT transport requires an RX-only UART", [key])
    pin = config[CONF_RX_PIN]
    if pin[CONF_INVERTED] or pin[CONF_NUMBER] >= 34:
        raise cv.Invalid("Use a non-inverted, output-capable GPIO", [CONF_RX_PIN])
    # Dedicated UART RX pins bypass the GPIO matrix when allocated to their UART.
    if pin[CONF_NUMBER] in (3, 9, 16):
        raise cv.Invalid(
            "Use an RX pin other than GPIO3, GPIO9 or GPIO16", [CONF_RX_PIN]
        )
    return config


def _validate_swi_uart_wiring(config: ConfigType) -> ConfigType:
    if str(config[CONF_ID].type) not in uart.NATIVE_UART_CLASSES:
        raise cv.Invalid("The single-wire transport requires a hardware UART")
    if CONF_FLOW_CONTROL_PIN in config:
        raise cv.Invalid(
            "Single-wire UART does not support flow control", [CONF_FLOW_CONTROL_PIN]
        )
    for key in (CONF_TX_PIN, CONF_RX_PIN):
        if config[key][CONF_INVERTED]:
            raise cv.Invalid("Single-wire UART requires non-inverted pins", [key])
    if CORE.is_esp8266:
        tx, rx = config[CONF_TX_PIN][CONF_NUMBER], config[CONF_RX_PIN][CONF_NUMBER]
        if (tx, rx) not in ((1, 3), (15, 13)):
            raise cv.Invalid(
                "On ESP8266, use hardware UART0 pins GPIO1/GPIO3 or GPIO15/GPIO13"
            )
        logger_config = fv.full_config.get().get(logger.DOMAIN)
        if (
            logger_config
            and logger_config[CONF_BAUD_RATE]
            and logger_config[CONF_HARDWARE_UART]
            in (
                logger.UART0,
                logger.UART0_SWAP,
            )
        ):
            raise cv.Invalid(
                "The single-wire transport requires UART0. Disable or move serial logging"
            )
    return config


def _final_validate(config: ConfigType) -> None:
    transport = config[CONF_TYPE]
    if transport == I2C:
        _VALIDATE_I2C(config)
        return

    uart_id = config[CONF_UART_ID]
    if transport == SWI_RMT:
        _VALIDATE_RMT_UART(config)
        fv.id_declaration_match_schema(_validate_rmt_wiring)(uart_id)
        esp32_rmt.final_validate_rmt_channels(config)
    else:
        _VALIDATE_SWI_UART(config)
        fv.id_declaration_match_schema(_validate_swi_uart_wiring)(uart_id)
    _VALIDATE_UART_BUFFER(uart_id)
    uart.claim_exclusive(config, DOMAIN)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    transport_type = config[CONF_TYPE]
    if transport_type == SWI_RMT:
        cg.add_define("USE_MICROCHIP_ATSHA204A_SWI_RMT")
        esp32_rmt.register_rmt_channel(config)
        esp32.include_builtin_idf_component("esp_driver_rmt")
        uart_config = fv.get_declaring_config(config[CONF_UART_ID], CORE.config)
        pin = uart_config[CONF_RX_PIN][CONF_NUMBER]
        transport = cg.new_Pvariable(config[CONF_TRANSPORT_ID], pin)
        await uart.register_uart_device(transport, config)
    elif transport_type == SWI_UART:
        cg.add_define("USE_MICROCHIP_ATSHA204A_SWI_UART")
        wake_pin = await cg.gpio_pin_expression(config[CONF_WAKE_PIN])
        transport = cg.new_Pvariable(config[CONF_TRANSPORT_ID], wake_pin)
        await uart.register_uart_device(transport, config)
    else:
        cg.add_define("USE_MICROCHIP_ATSHA204A_I2C")
        transport = cg.new_Pvariable(config[CONF_TRANSPORT_ID])
        await i2c.register_i2c_device(transport, config)
    var = cg.new_Pvariable(config[CONF_ID], transport)
    await cg.register_component(var, config)


OPCODES = Opcode.options(
    "pause",
    "read",
    "mac",
    "hmac",
    "write",
    "gen_dig",
    "nonce",
    "lock",
    "random",
    "derive_key",
    "update_extra",
    "check_mac",
    "dev_rev",
    "sha",
    prefix="OPCODE_",
)
END_MODES = EndMode.options("sleep", "idle", "keep_awake", prefix="END_MODE_")


def _validate_command(config: ConfigType) -> ConfigType:
    if not config[CONF_EXPECT_RESPONSE] and (
        config[CONF_OPCODE] != "pause" or config[CONF_END] != "sleep"
    ):
        raise cv.Invalid(
            "Only a Pause command with end: sleep can omit the response",
            [CONF_EXPECT_RESPONSE],
        )
    return config


@automation.register_action(
    f"{DOMAIN}.command",
    CommandAction,
    cv.All(
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(ATSHA204A),
                cv.Required(CONF_OPCODE): cv.enum(OPCODES, lower=True),
                cv.Optional(CONF_PARAM1, default=0): cv.templatable(cv.hex_uint8_t),
                cv.Optional(CONF_PARAM2, default=0): cv.templatable(cv.hex_uint16_t),
                cv.Optional(CONF_DATA, default=[]): cv.templatable(
                    cv.All(
                        cv.ensure_list(cv.hex_uint8_t), cv.Length(max=MAX_COMMAND_DATA)
                    )
                ),
                cv.Optional(CONF_END, default="sleep"): cv.enum(END_MODES, lower=True),
                cv.Optional(CONF_SESSION, default=0): cv.templatable(cv.uint32_t),
                cv.Optional(CONF_EXPECT_RESPONSE, default=True): cv.boolean,
                cv.Optional(CONF_ON_RESULT): automation.validate_automation(
                    single=True
                ),
            }
        ),
        _validate_command,
    ),
    synchronous=True,
)
async def command_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(
        action_id,
        template_arg,
        parent,
        config[CONF_OPCODE],
        config[CONF_END],
        config[CONF_EXPECT_RESPONSE],
    )
    cg.add(var.set_param1(await cg.templatable(config[CONF_PARAM1], args, cg.uint8)))
    cg.add(var.set_param2(await cg.templatable(config[CONF_PARAM2], args, cg.uint16)))
    cg.add(var.set_session(await cg.templatable(config[CONF_SESSION], args, cg.uint32)))
    await automation.templatable_bytes(
        config[CONF_DATA],
        args,
        var.set_data_template,
        var.set_data_static,
        f"{action_id}_data",
    )
    if (result := config.get(CONF_ON_RESULT)) is not None:
        await automation.build_callback_automation(
            var,
            "set_result_callback",
            [(cg.int_, "status"), (CommandResponse, "data"), (cg.uint32, "session")],
            result,
        )
    return var


automation.register_apply_action(
    f"{DOMAIN}.end_session",
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ATSHA204A),
            cv.Required(CONF_SESSION): cv.templatable(cv.uint32_t),
        }
    ),
    automation.ApplyField(CONF_SESSION, "end_session", cg.uint32),
)
