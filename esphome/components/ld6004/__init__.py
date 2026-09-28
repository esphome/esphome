"""Hi-Link LD6004 TinyFrame radar."""

import math

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_REFRESH
from esphome.core import Lambda
from esphome.types import ConfigType

CONF_STALE_TIMEOUT: str = "stale_timeout"
CONF_OUT_PIN: str = "out_pin"
CONF_X_MIN: str = "x_min"
CONF_X_MAX: str = "x_max"
CONF_Y_MIN: str = "y_min"
CONF_Y_MAX: str = "y_max"
CONF_Z_MIN: str = "z_min"
CONF_Z_MAX: str = "z_max"
CONF_ZONE_ID: str = "zone_id"
CONF_MINIMUM: str = "minimum"
CONF_MAXIMUM: str = "maximum"

CODEOWNERS: list[str] = ["@wolph"]
DEPENDENCIES: list[str] = ["uart"]
MULTI_CONF: bool = True
ld6004_ns: cg.MockObj = cg.esphome_ns.namespace("ld6004")
LD6004Component: cg.MockObjClass = ld6004_ns.class_(
    "LD6004Component", cg.Component, uart.UARTDevice
)
CONF_LD6004_ID: str = "ld6004_id"
AXES: tuple[str, ...] = (
    CONF_X_MIN,
    CONF_X_MAX,
    CONF_Y_MIN,
    CONF_Y_MAX,
    CONF_Z_MIN,
    CONF_Z_MAX,
)


def finite_coordinate(value: object) -> float:
    result: float = cv.float_(value)
    if not math.isfinite(result) or abs(result) > 3.4028234e38:
        raise cv.Invalid("Coordinate must be a finite float32")
    return result


def validate_bounds(config: ConfigType) -> ConfigType:
    for minimum, maximum in (
        (CONF_X_MIN, CONF_X_MAX),
        (CONF_Y_MIN, CONF_Y_MAX),
        (CONF_Z_MIN, CONF_Z_MAX),
        (CONF_MINIMUM, CONF_MAXIMUM),
    ):
        if minimum not in config:
            continue
        low: object = config[minimum]
        high: object = config[maximum]
        if not isinstance(low, Lambda) and not isinstance(high, Lambda) and low >= high:
            raise cv.Invalid(f"{minimum} must be less than {maximum}")
    return config


CONFIG_SCHEMA: cv.Schema = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD6004Component),
            cv.Optional(
                CONF_STALE_TIMEOUT, default="5s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_OUT_PIN): pins.gpio_input_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)
FINAL_VALIDATE_SCHEMA: cv.Schema = uart.final_validate_device_schema(
    "ld6004",
    baud_rate=115200,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var: cg.MockObj = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_stale_timeout(config[CONF_STALE_TIMEOUT].total_milliseconds))
    pin_config: ConfigType | None
    if pin_config := config.get(CONF_OUT_PIN):
        pin: cg.MockObj = await cg.gpio_pin_expression(pin_config)
        cg.add(var.set_out_pin(pin))


ACTION_SCHEMA: cv.Schema = cv.Schema({cv.GenerateID(): cv.use_id(LD6004Component)})
for action in (CONF_REFRESH, "recover_commands"):
    automation.register_apply_action(
        f"ld6004.{action}", ACTION_SCHEMA, automation.ApplyCall(f"{action}()")
    )

SET_ZONE_SCHEMA: cv.All = cv.All(
    ACTION_SCHEMA.extend(
        {
            cv.Required(CONF_ZONE_ID): cv.templatable(cv.int_range(min=0, max=11)),
            **{cv.Required(axis): cv.templatable(finite_coordinate) for axis in AXES},
        }
    ),
    validate_bounds,
)
automation.register_apply_action(
    "ld6004.set_zone",
    SET_ZONE_SCHEMA,
    automation.ApplyCall(
        "set_zone({}, {}, {}, {}, {}, {}, {})",
        (
            (CONF_ZONE_ID, cg.uint32, None),
            *((axis, cg.float_, None) for axis in AXES),
        ),
    ),
)
automation.register_apply_action(
    "ld6004.set_z_range",
    cv.All(
        ACTION_SCHEMA.extend(
            {
                cv.Required(CONF_MINIMUM): cv.templatable(finite_coordinate),
                cv.Required(CONF_MAXIMUM): cv.templatable(finite_coordinate),
            }
        ),
        validate_bounds,
    ),
    automation.ApplyCall(
        "set_z_range({}, {})",
        (
            (CONF_MINIMUM, cg.float_, None),
            (CONF_MAXIMUM, cg.float_, None),
        ),
    ),
)
