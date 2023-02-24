import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_STEP,
)

from .. import (
    lg_uart_ns,
    LG_UART_CLIENT_SCHEMA,
    CONF_LG_UART_CMD,
    CONF_DECODE_BASE,
    CONF_DECODE_BASE_TYPES,
    register_lg_uart_child,
    validate_uart_cmd_len,
    CODEOWNERS as co,
)

CODEOWNERS = co
DEPENDENCIES = ["lg_uart"]

LGUartNumber = lg_uart_ns.class_("LGUartNumber", number.Number, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(
    number.NUMBER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(LGUartNumber),
            # TODO: length? It seems like all LG commands are two chars?
            cv.Required(CONF_LG_UART_CMD): cv.string_strict,
            # TODO: can I do this as just a number rather than a string?
            cv.Optional(CONF_DECODE_BASE, default=10): cv.one_of(
                *CONF_DECODE_BASE_TYPES, int=True
            ),
            # LG is all over the place when it comes to encodings and min/max values.
            # Some settings take 00 through 99 base 10 and others take 0x00 through 0x64
            # In the documentation that I have, I can't find a setting that is best represented by a slider that
            #       takes values of more than 100
            cv.Optional(CONF_MAX_VALUE, default=100): cv.positive_int,
            cv.Optional(CONF_MIN_VALUE, default=0): cv.positive_int,
            cv.Optional(CONF_STEP, default=1): cv.positive_int,
        }
    )
    # User can adjust as needed; we poll the screen every min
    .extend(cv.polling_component_schema("60s")).extend(LG_UART_CLIENT_SCHEMA),
    validate_uart_cmd_len,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await number.register_number(
        var,
        config,
        min_value=config[CONF_MIN_VALUE],
        max_value=config[CONF_MAX_VALUE],
        step=config[CONF_STEP],
    )

    await register_lg_uart_child(var, config)
    # The command chars
    cg.add(var.set_cmd(config[CONF_LG_UART_CMD]))
    cg.add(var.set_encoding_base(config[CONF_DECODE_BASE]))
