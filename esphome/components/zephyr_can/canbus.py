import logging
import re

from esphome import pins
import esphome.codegen as cg
from esphome.components.canbus import (
    CANBUS_SCHEMA,
    CONF_BIT_RATE,
    CanbusComponent,
    get_rate,
    register_canbus,
)
from esphome.components.zephyr import (
    zephyr_add_overlay,
    zephyr_add_prj_conf,
    zephyr_data,
    zephyr_dts_board_id,
    zephyr_variant,
    zephyr_variant_family,
)
from esphome.components.zephyr.const import KEY_BOARD
from esphome.components.zephyr.dts_lookup import (
    get_can_controller_labels,
    normalize_dts_label,
    validate_dts_label_exists,
)
from esphome.components.zephyr.variants import VARIANTS
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MODE,
    CONF_RX_BUFFER_SIZE,
    CONF_RX_PIN,
    CONF_TX_PIN,
)
from esphome.core import CORE
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["zephyr"]

CONF_INSTANCE = "instance"

# Only STM32 has its CAN pinctrl and overlay shape wired up here; other Zephyr targets
# spell their controller and pin routing differently (e.g. Espressif's TWAI).
SUPPORTED_FAMILIES = ("stm32",)

# Devicetree node labels of CAN controllers: bxCAN parts (STM32F0/F1/F3/F4/L4) label
# theirs can1/can2, FDCAN parts (STM32U5/C0) fdcan1.
_INSTANCE_RE = re.compile(r"(fd)?can\d+")

zephyr_can_ns = cg.esphome_ns.namespace("zephyr_can")
ZephyrCan = zephyr_can_ns.class_("ZephyrCan", CanbusComponent)

CAN_MODES = {
    "NORMAL": cg.RawExpression("CAN_MODE_NORMAL"),
    "LISTENONLY": cg.RawExpression("CAN_MODE_LISTENONLY"),
    "LOOPBACK": cg.RawExpression("CAN_MODE_LOOPBACK"),
}


def _validate_instance(value: str) -> str:
    value = normalize_dts_label(cv.string_strict(value))
    if not _INSTANCE_RE.fullmatch(value):
        raise cv.Invalid(
            f"'{value}' is not a CAN controller devicetree node label, "
            "e.g. 'fdcan1' or 'can1'"
        )
    return value


def _validate(config: ConfigType) -> ConfigType:
    if not CORE.using_zephyr or zephyr_variant_family() not in SUPPORTED_FAMILIES:
        raise cv.Invalid(
            "zephyr_can is only available on Zephyr variants of the "
            f"{', '.join(SUPPORTED_FAMILIES)} family"
        )
    if (CONF_TX_PIN in config) != (CONF_RX_PIN in config):
        raise cv.Invalid(
            f"'{CONF_TX_PIN}' and '{CONF_RX_PIN}' must be given together; leave both "
            "out to use the pins the board's devicetree already assigns"
        )
    return config


CONFIG_SCHEMA = cv.All(
    CANBUS_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ZephyrCan),
            cv.Optional(CONF_INSTANCE): _validate_instance,
            cv.Optional(CONF_TX_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_RX_PIN): pins.internal_gpio_input_pin_number,
            cv.Optional(CONF_RX_BUFFER_SIZE, default=8): cv.int_range(min=1, max=64),
            cv.Optional(CONF_MODE, default="NORMAL"): cv.enum(CAN_MODES, upper=True),
        }
    ),
    _validate,
)


def _resolve_instance(board: str) -> str:
    """Return the board's CAN controller node label, read from its devicetree."""
    labels = get_can_controller_labels(board)
    if labels:
        if len(labels) > 1:
            _LOGGER.info(
                "[zephyr] Multiple CAN controllers on '%s': %s; using '%s'",
                board,
                labels,
                labels[0],
            )
        return labels[0]
    if labels is None:
        detail = "Install gcc/cpp (C preprocessor) for automatic devicetree detection."
    else:
        detail = f"Board '{board}' declares no CAN controller in its devicetree."
    raise cv.Invalid(
        f"Cannot determine the CAN controller node label for board '{board}'. "
        f"{detail}\n"
        "To set it explicitly:\n"
        "  canbus:\n"
        "    - platform: zephyr_can\n"
        "      instance: fdcan1  # replace with your board's node label"
    )


def _pin_name(pin: int) -> str:
    """Format a pin number the way STM32 pinctrl nodes spell it, e.g. 11 -> 'pa11'."""
    variant = VARIANTS[zephyr_variant()]
    port_labels = variant.gpio_port_labels
    port = pin // variant.gpio_port_width
    if port >= len(port_labels):
        raise cv.Invalid(
            f"Pin {pin} is out of range: this variant has ports "
            f"{port_labels[0].upper()}-{port_labels[-1].upper()} "
            f"with {variant.gpio_port_width} pins each"
        )
    return f"p{port_labels[port]}{pin % variant.gpio_port_width}"


async def to_code(config: ConfigType) -> None:
    zephyr_add_prj_conf("CAN", True)
    # Without this Zephyr rejects remote transmission request frames in the driver, so
    # they would never reach an on_frame trigger.
    zephyr_add_prj_conf("CAN_ACCEPT_RTR", True)

    board = zephyr_dts_board_id(zephyr_data()[KEY_BOARD])
    if explicit_instance := config.get(CONF_INSTANCE):
        validate_dts_label_exists("can", board, explicit_instance)
        instance = explicit_instance
    else:
        instance = _resolve_instance(board)

    rx_queue = f"{config[CONF_ID]}_rx_queue"
    cg.add_global(
        cg.RawExpression(f"CAN_MSGQ_DEFINE({rx_queue}, {config[CONF_RX_BUFFER_SIZE]})")
    )

    var = cg.new_Pvariable(
        config[CONF_ID],
        cg.RawExpression(f"DEVICE_DT_GET(DT_NODELABEL({instance}))"),
        cg.RawExpression(f"&{rx_queue}"),
        get_rate(config[CONF_BIT_RATE]),
        config[CONF_MODE],
    )
    await register_canbus(var, config)

    # STM32 SoC devicetrees ship their CAN nodes disabled, so the node always has to be
    # turned on here. A controller that also needs a kernel clock source picked (e.g.
    # STM32U5's FDCAN, which only gets its bus clock from the SoC dtsi) needs a `clocks`
    # property too -- add that from `zephyr: overlays: app:`, which is applied after
    # component overlays and therefore wins.
    overlay = [f"&{instance} {{", '  status = "okay";']
    if (tx_pin := config.get(CONF_TX_PIN)) is not None:
        rx_pin = config[CONF_RX_PIN]
        pinctrl = (
            f"<&{instance}_rx_{_pin_name(rx_pin)} &{instance}_tx_{_pin_name(tx_pin)}>"
        )
        overlay += [
            f"  pinctrl-0 = {pinctrl};",
            '  pinctrl-names = "default";',
        ]
    overlay.append("};")
    zephyr_add_overlay("\n".join(overlay))
