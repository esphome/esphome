from esphome import pins
import esphome.codegen as cg
from esphome.components import remote_base
from esphome.components.libretiny import get_libretiny_family
from esphome.components.libretiny.const import (
    FAMILY_BK7231Q,
    FAMILY_BK7238,
    KEY_BOARD,
    KEY_LIBRETINY,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_DUMP,
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_PIN,
)
from esphome.core import CORE
from esphome.types import ConfigType

AUTO_LOAD = ["remote_base"]
CODEOWNERS = ["@bbbert"]
DEPENDENCIES = ["bk72xx"]
DOMAIN = "beken_irda"

IRDA_PIN = 26

beken_irda_ns = cg.esphome_ns.namespace("beken_irda")

BekenIrdaComponent = beken_irda_ns.class_(
    "BekenIrdaComponent", cg.Component, remote_base.RemoteReceiverBase
)


def validate_pin(value: ConfigType) -> ConfigType:
    """The IRDA block has no pin matrix and sets up its pin itself."""
    if value[CONF_NUMBER] != IRDA_PIN:
        raise cv.Invalid(
            f"The IRDA block only works on P{IRDA_PIN}, not pin {value[CONF_NUMBER]}; "
            "use remote_receiver for other pins"
        )
    if value[CONF_INVERTED] or any(
        on for flag, on in value.get(CONF_MODE, {}).items() if flag != CONF_INPUT
    ):
        raise cv.Invalid(
            "The IRDA block does not apply pin modes or inversion; use remote_receiver for those"
        )
    return value


def validate_board(config: ConfigType) -> ConfigType:
    """Only the legacy IRDA block, with its NEC decoder and interrupt, is supported."""
    family = get_libretiny_family()
    board = CORE.data[KEY_LIBRETINY][KEY_BOARD]
    # The BK7238 has no IRDA block and the BK7231Q SDK registers no IRDA interrupt. BK7252N/BK7253 have
    # a different block but report the BK7251 family, so only the board id tells them apart.
    if family in (FAMILY_BK7238, FAMILY_BK7231Q) or any(
        model in board.lower() for model in ("bk7252n", "bk7253")
    ):
        raise cv.Invalid(
            f"beken_irda is not supported on '{board}'; use remote_receiver instead"
        )
    return config


CONFIG_SCHEMA = cv.All(
    remote_base.validate_triggers(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(BekenIrdaComponent),
                cv.Optional(CONF_PIN, default=f"P{IRDA_PIN}"): cv.All(
                    pins.internal_gpio_input_pin_schema, validate_pin
                ),
                cv.Optional(CONF_DUMP, default=[]): remote_base.validate_dumpers,
            }
        ).extend(cv.COMPONENT_SCHEMA)
    ),
    validate_board,
)


async def to_code(config: ConfigType) -> None:
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    var = cg.new_Pvariable(config[CONF_ID], pin)

    for dumper in await remote_base.build_dumpers(config[CONF_DUMP]):
        remote_base.add_dumper(var, dumper)

    for trigger in await remote_base.build_triggers(config):
        remote_base.add_listener(var, trigger)

    await cg.register_component(var, config)
