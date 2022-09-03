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

dxs238xw_ns = cg.esphome_ns.namespace(COMPONENT_NAME)
Dxs238xwComponent = dxs238xw_ns.class_(
    "Dxs238xwComponent", cg.PollingComponent, uart.UARTDevice
)
SmIdEntity = dxs238xw_ns.enum("SmIdEntity", True)
SmLimitValue = dxs238xw_ns.enum("SmLimitValue")

DXS238XW_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DXS238XW_ID): cv.use_id(Dxs238xwComponent),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema({cv.GenerateID(): cv.declare_id(Dxs238xwComponent)})
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("3s"))
)


def validate_uart(
    component: str,
    tx_pin: int,
    rx_pin: int,
    baud_rate: int,
    parity: str,
    rx_buffer_size: int,
    stop_bits: int,
    data_bits: int,
):
    def validate_data(opt, required_value):
        def validator(value):
            if value != required_value:
                raise cv.Invalid(
                    f"Component {component} required {opt} {required_value} for the uart bus"
                )
            return value

        return validator

    def validate_pin(opt, device, pin, value_other_pin):
        def validator(value):
            if value[CONF_NUMBER] != pin:
                _LOGGER.warning(
                    "Component %s usually uart %s is GPIO%d", component, opt, pin
                )
            if value[CONF_NUMBER] == value_other_pin[CONF_NUMBER]:
                raise cv.Invalid(
                    f"Component {component} required {CONF_TX_PIN} and {CONF_RX_PIN} not be the same"
                )
            if opt in device:
                raise cv.Invalid(
                    f"The uart {opt} is used both by {component} and {device[opt]}, "
                    f"but can only be used by one. Please create a new uart bus for {component}."
                )
            device[opt] = component
            return value

        return validator

    def validate_hub(conf):
        if CONF_TX_PIN not in conf:
            raise cv.Invalid(
                f"Component {component} requires this uart bus to declare a {CONF_TX_PIN}"
            )

        if CONF_RX_PIN not in conf:
            raise cv.Invalid(
                f"Component {component} requires this uart bus to declare a {CONF_RX_PIN}"
            )

        hub_schema = {}
        uart_id = conf[CONF_ID]
        devices = fv.full_config.get().data.setdefault(uart.KEY_UART_DEVICES, {})
        device = devices.setdefault(uart_id, {})

        hub_schema[cv.Required(CONF_TX_PIN)] = validate_pin(
            CONF_TX_PIN, device, tx_pin, conf[CONF_RX_PIN]
        )
        hub_schema[cv.Required(CONF_RX_PIN)] = validate_pin(
            CONF_RX_PIN, device, rx_pin, conf[CONF_TX_PIN]
        )
        hub_schema[cv.Required(CONF_BAUD_RATE)] = validate_data(
            CONF_BAUD_RATE, baud_rate
        )
        hub_schema[cv.Optional(uart.CONF_PARITY)] = validate_data(
            uart.CONF_PARITY, parity
        )
        hub_schema[cv.Optional(uart.CONF_RX_BUFFER_SIZE)] = validate_data(
            uart.CONF_RX_BUFFER_SIZE, rx_buffer_size
        )
        hub_schema[cv.Optional(uart.CONF_STOP_BITS)] = validate_data(
            uart.CONF_STOP_BITS, stop_bits
        )
        hub_schema[cv.Optional(uart.CONF_DATA_BITS)] = validate_data(
            uart.CONF_DATA_BITS, data_bits
        )

        return cv.Schema(hub_schema, extra=cv.ALLOW_EXTRA)(conf)

    return cv.Schema(
        {cv.Required(uart.CONF_UART_ID): fv.id_declaration_match_schema(validate_hub)},
        extra=cv.ALLOW_EXTRA,
    )


FINAL_VALIDATE_SCHEMA = validate_uart(
    component=COMPONENT_NAME,
    tx_pin=1,
    rx_pin=3,
    baud_rate=9600,
    parity="NONE",
    rx_buffer_size=256,
    stop_bits=1,
    data_bits=8,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)


# ////////////////////////////////////////////////////////////////////////////////////////////////////

MeterStateToggleAction = dxs238xw_ns.class_("MeterStateToggleAction", automation.Action)


@automation.register_action(
    "dxs238xw.meter_state_toggle",
    MeterStateToggleAction,
    maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(Dxs238xwComponent),
        }
    ),
)
async def meter_state_toggle_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


# ////////////////////////////////////////////////////////////////////////////////////////////////////

MeterStateOnAction = dxs238xw_ns.class_("MeterStateOnAction", automation.Action)


@automation.register_action(
    "dxs238xw.meter_state_on",
    MeterStateOnAction,
    maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(Dxs238xwComponent),
        }
    ),
)
async def meter_state_on_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


# ////////////////////////////////////////////////////////////////////////////////////////////////////

MeterStateOffAction = dxs238xw_ns.class_("MeterStateOffAction", automation.Action)


@automation.register_action(
    "dxs238xw.meter_state_off",
    MeterStateOffAction,
    maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(Dxs238xwComponent),
        }
    ),
)
async def meter_state_off_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


# ////////////////////////////////////////////////////////////////////////////////////////////////////

HexMessageAction = dxs238xw_ns.class_("HexMessageAction", automation.Action)

MESSAGE_VALUE = "message"
CHECK_CRC = "check_crc"


@automation.register_action(
    "dxs238xw.hex_message",
    HexMessageAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(Dxs238xwComponent),
            cv.Required(MESSAGE_VALUE): cv.templatable(cv.string),
            cv.Optional(CHECK_CRC, default=True): cv.templatable(cv.boolean),
        }
    ),
)
async def hex_message_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_message = await cg.templatable(config[MESSAGE_VALUE], args, str)
    cg.add(var.set_message(template_message))

    template_check_crc = await cg.templatable(config[CHECK_CRC], args, bool)
    cg.add(var.set_check_crc(template_check_crc))

    return var
