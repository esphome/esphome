from collections.abc import Callable

from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PASSWORD, CONF_THROTTLE, CONF_TIMEOUT
import esphome.final_validate as fv
from esphome.types import ConfigType

AUTO_LOAD = ["ld24xx"]
DEPENDENCIES = ["uart"]
CODEOWNERS = ["@sebcaps", "@regevbr"]
DOMAIN = "ld2410"
MULTI_CONF = True

ld2410_ns = cg.esphome_ns.namespace("ld2410")
LD2410Component = ld2410_ns.class_("LD2410Component", cg.Component, uart.UARTDevice)

CONF_LD2410_ID = "ld2410_id"
CONF_MAX_MOVE_DISTANCE = "max_move_distance"
CONF_MAX_STILL_DISTANCE = "max_still_distance"
CONF_BACKGROUND_CORRECTION = "background_correction"
CONF_BACKGROUND_CORRECTION_DURATION = "background_correction_duration"
CONF_MOVE_THRESHOLDS = [f"g{x}_move_threshold" for x in range(9)]
CONF_STILL_THRESHOLDS = [f"g{x}_still_threshold" for x in range(9)]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LD2410Component),
        cv.Optional(CONF_THROTTLE): cv.invalid(
            f"{CONF_THROTTLE} has been removed; use per-sensor filters, instead"
        ),
        cv.Optional(CONF_MAX_MOVE_DISTANCE): cv.invalid(
            f"The '{CONF_MAX_MOVE_DISTANCE}' option has been moved to the '{CONF_MAX_MOVE_DISTANCE}'"
            f" number component"
        ),
        cv.Optional(CONF_MAX_STILL_DISTANCE): cv.invalid(
            f"The '{CONF_MAX_STILL_DISTANCE}' option has been moved to the '{CONF_MAX_STILL_DISTANCE}'"
            f" number component"
        ),
        cv.Optional(CONF_TIMEOUT): cv.invalid(
            f"The '{CONF_TIMEOUT}' option has been moved to the '{CONF_TIMEOUT}'"
            f" number component"
        ),
        cv.Optional(CONF_BACKGROUND_CORRECTION_DURATION): cv.All(
            cv.positive_time_period_seconds,
            cv.Range(
                min=cv.TimePeriod(seconds=10),
                max=cv.TimePeriod(seconds=120),
            ),
        ),
    }
)

for i in range(9):
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
        cv.Schema(
            {
                cv.Optional(CONF_MOVE_THRESHOLDS[i]): cv.invalid(
                    f"The '{CONF_MOVE_THRESHOLDS[i]}' option has been moved to the '{CONF_MOVE_THRESHOLDS[i]}'"
                    f" number component"
                ),
                cv.Optional(CONF_STILL_THRESHOLDS[i]): cv.invalid(
                    f"The '{CONF_STILL_THRESHOLDS[i]}' option has been moved to the '{CONF_STILL_THRESHOLDS[i]}'"
                    f" number component"
                ),
            }
        )
    )

CONFIG_SCHEMA = cv.All(
    CONFIG_SCHEMA.extend(uart.UART_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)
)


def require_background_correction_button(
    key: str, hub_key: str = CONF_LD2410_ID
) -> Callable[[ConfigType], ConfigType]:
    """A final-validation function that rejects key unless the hub has a background_correction button.

    Background correction is only compiled in with the button, so without it key would never take effect.
    """

    def validator(config: ConfigType) -> ConfigType:
        if key not in config:
            return config
        buttons = fv.platform_entries(
            domain="button", platform=DOMAIN, match={CONF_LD2410_ID: config[hub_key]}
        )
        if not any(CONF_BACKGROUND_CORRECTION in entry for entry in buttons):
            raise cv.Invalid(
                f"'{key}' requires the ld2410 '{CONF_BACKGROUND_CORRECTION}' button",
                path=[key],
            )
        return config

    return validator


FINAL_VALIDATE_SCHEMA = cv.All(
    uart.final_validate_device_schema(
        "ld2410",
        require_tx=True,
        require_rx=True,
        parity="NONE",
        stop_bits=1,
    ),
    require_background_correction_button(
        CONF_BACKGROUND_CORRECTION_DURATION, hub_key=CONF_ID
    ),
)


async def to_code(config: ConfigType) -> None:
    var = await uart.new_uart_component(config)
    if (duration := config.get(CONF_BACKGROUND_CORRECTION_DURATION)) is not None:
        cg.add(var.set_background_correction_duration(duration.total_seconds))


CALIBRATION_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(LD2410Component),
    }
)


# Actions
BLUETOOTH_PASSWORD_SET_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(LD2410Component),
        cv.Required(CONF_PASSWORD): cv.sensitive(cv.templatable(cv.string_strict)),
    }
)

automation.register_apply_action(
    "bluetooth_password.set",
    BLUETOOTH_PASSWORD_SET_SCHEMA,
    automation.ApplyField(CONF_PASSWORD, "set_bluetooth_password", cg.std_string),
)
