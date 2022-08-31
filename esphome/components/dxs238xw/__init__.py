import logging
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import uart
from esphome.const import CONF_ID, CONF_RX_PIN, CONF_TX_PIN, CONF_BAUD_RATE, CONF_NUMBER

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@rodgon81"]

DEPENDENCIES = ["uart"]

CONF_DXS238XW_ID = "dxs238xw_id"
COMPONENT_NAME = "dxs238xw"

UART_CONFIG_RX_PIN = 3
UART_CONFIG_TX_PIN = 1
UART_CONFIG_BAUD_RATE = 9600
UART_CONFIG_PARITY = "NONE"
UART_CONFIG_RX_BUFFER_SIZE = 256
UART_CONFIG_STOP_BITS = 1
UART_CONFIG_DATA_BITS = 8

dxs238xw_ns = cg.esphome_ns.namespace(COMPONENT_NAME)
dxs238xwComponent = dxs238xw_ns.class_(
    "Dxs238xwComponent", cg.PollingComponent, uart.UARTDevice
)
SmIdEntity = dxs238xw_ns.enum("SmIdEntity", True)
SmLimitValue = dxs238xw_ns.enum("SmLimitValue")

DXS238XW_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DXS238XW_ID): cv.use_id(dxs238xwComponent),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema({cv.GenerateID(): cv.declare_id(dxs238xwComponent)})
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("3s"))
)


def valid_uart(conf):
    rx_pin = UART_CONFIG_RX_PIN
    if conf[CONF_RX_PIN][CONF_NUMBER] != rx_pin:
        _LOGGER.warning("Usually rx pin is GPIO%d", rx_pin)

    tx_pin = UART_CONFIG_TX_PIN
    if conf[CONF_TX_PIN][CONF_NUMBER] != tx_pin:
        _LOGGER.warning("Usually tx pin is GPIO%d", tx_pin)

    baud_rate = UART_CONFIG_BAUD_RATE
    if conf[CONF_BAUD_RATE] != baud_rate:
        raise cv.Invalid(
            f"Component {COMPONENT_NAME} required baud_rate {baud_rate} for the uart bus"
        )

    parity = UART_CONFIG_PARITY
    if conf[uart.CONF_PARITY] != parity:
        raise cv.Invalid(
            f"Component {COMPONENT_NAME} required parity {parity} for the uart bus"
        )

    rx_buffer_size = UART_CONFIG_RX_BUFFER_SIZE
    if conf[uart.CONF_RX_BUFFER_SIZE] < rx_buffer_size:
        raise cv.Invalid(
            f"Component {COMPONENT_NAME} required minimum rx_buffer_size {rx_buffer_size} for the uart bus"
        )

    stop_bits = UART_CONFIG_STOP_BITS
    if conf[uart.CONF_STOP_BITS] != stop_bits:
        raise cv.Invalid(
            f"Component {COMPONENT_NAME} required stop_bits {stop_bits} for the uart bus"
        )

    data_bits = UART_CONFIG_DATA_BITS
    if conf[uart.CONF_DATA_BITS] != data_bits:
        raise cv.Invalid(
            f"Component {COMPONENT_NAME} required data_bits {data_bits} for the uart bus"
        )

    return conf


FINAL_VALIDATE_SCHEMA = cv.All(
    cv.Schema(
        {cv.Required(uart.CONF_UART_ID): fv.id_declaration_match_schema(valid_uart)},
        extra=cv.ALLOW_EXTRA,
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)


# ////////////////////////////////////////////////////////////////////////////////////////////////////

setMeterStateToogleAction = dxs238xw_ns.class_(
    "setMeterStateToogleAction", automation.Action
)


@automation.register_action(
    "dxs238xw.meterStateToogle",
    setMeterStateToogleAction,
    maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(dxs238xwComponent),
        }
    ),
)
async def setMeterStateToogleAction_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


# ////////////////////////////////////////////////////////////////////////////////////////////////////

setMeterStateOnAction = dxs238xw_ns.class_("setMeterStateOnAction", automation.Action)


@automation.register_action(
    "dxs238xw.meterStateOn",
    setMeterStateOnAction,
    maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(dxs238xwComponent),
        }
    ),
)
async def setMeterStateOnAction_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


# ////////////////////////////////////////////////////////////////////////////////////////////////////

setMeterStateOffAction = dxs238xw_ns.class_("setMeterStateOffAction", automation.Action)


@automation.register_action(
    "dxs238xw.meterStateOff",
    setMeterStateOffAction,
    maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(dxs238xwComponent),
        }
    ),
)
async def setMeterStateOffAction_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


# ////////////////////////////////////////////////////////////////////////////////////////////////////

sendHexMessageAction = dxs238xw_ns.class_("sendHexMessageAction", automation.Action)

MESSAGE_VALUE = "message"
CHECK_CRC = "check_crc"


@automation.register_action(
    "dxs238xw.sendHexMessage",
    sendHexMessageAction,
    maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(dxs238xwComponent),
            cv.Required(MESSAGE_VALUE): cv.templatable(cv.string),
            cv.Optional(CHECK_CRC, default=True): cv.templatable(cv.boolean),
        }
    ),
)
async def sendHexMessageAction_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_message = await cg.templatable(config[MESSAGE_VALUE], args, str)
    cg.add(var.set_message(template_message))

    template_check_crc = await cg.templatable(config[CHECK_CRC], args, bool)
    cg.add(var.set_check_crc(template_check_crc))

    return var
