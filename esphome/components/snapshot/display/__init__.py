import esphome.codegen as cg
from esphome.components import display
from esphome.components.const import BYTE_ORDER_LITTLE, CONF_COLOR_DEPTH
import esphome.config_validation as cv
from esphome.const import (
    CONF_DIMENSIONS,
    CONF_HEIGHT,
    CONF_ID,
    CONF_LAMBDA,
    CONF_WIDTH,
    PLATFORM_HOST,
)
from esphome.types import ConfigType

from .. import Snapshot, register_snapshot, snapshot_ns

# The base class and the file writing live in the parent component, which nothing else in a
# configuration using only this platform would pull in.
AUTO_LOAD = ["snapshot"]

SnapshotDisplay = snapshot_ns.class_(
    "SnapshotDisplay", display.DisplayBuffer, cg.Component, Snapshot
)


def add_display_metadata(config: ConfigType) -> ConfigType:
    dimensions = config[CONF_DIMENSIONS]
    if isinstance(dimensions, dict):
        width, height = dimensions[CONF_WIDTH], dimensions[CONF_HEIGHT]
    else:
        width, height = dimensions
    if (
        width > 65535
        or height > 65535
        or width * height * (config[CONF_COLOR_DEPTH] // 8) > 0xFFFFFFFF
    ):
        raise cv.Invalid(
            "Snapshot dimensions exceed the supported buffer size", [CONF_DIMENSIONS]
        )
    display.add_metadata(
        config[CONF_ID],
        width,
        height,
        byte_order=BYTE_ORDER_LITTLE,
        native_color_depth=config[CONF_COLOR_DEPTH],
        rotation=config.get("rotation", 0),
        has_writer=config.get("auto_clear_enabled") is True
        or any(key in config for key in ("lambda", "pages"))
        or config.get("show_test_card") is True,
    )
    return config


CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(SnapshotDisplay),
                cv.Optional(CONF_COLOR_DEPTH, default=16): cv.one_of(16, 24),
                cv.Required(CONF_DIMENSIONS): cv.Any(
                    cv.dimensions,
                    cv.Schema(
                        {
                            cv.Required(CONF_WIDTH): cv.positive_not_null_int,
                            cv.Required(CONF_HEIGHT): cv.positive_not_null_int,
                        }
                    ),
                ),
            }
        )
    ),
    cv.only_on(PLATFORM_HOST),
    add_display_metadata,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_color_depth(config[CONF_COLOR_DEPTH]))
    await display.register_display(var, config)
    await register_snapshot(var, config)

    dimensions = config[CONF_DIMENSIONS]
    if isinstance(dimensions, dict):
        cg.add(var.set_dimensions(dimensions[CONF_WIDTH], dimensions[CONF_HEIGHT]))
    else:
        (width, height) = dimensions
        cg.add(var.set_dimensions(width, height))

    if lamb := config.get(CONF_LAMBDA):
        lambda_ = await cg.process_lambda(
            lamb, [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
