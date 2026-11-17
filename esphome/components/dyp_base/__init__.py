from collections.abc import Callable
from dataclasses import dataclass
from typing import Any

from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor, uart
from esphome.components.const import CONF_OUTPUT_MODE
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_RANGE,
    CONF_MODE,
    CONF_MODEL,
    CONF_TRIGGER_PIN,
    DEVICE_CLASS_DISTANCE,
    ICON_ARROW_EXPAND_VERTICAL,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
)
from esphome.types import ConfigType

AUTO_LOAD = ["sensor"]
CODEOWNERS = ["@estockda"]
DEPENDENCIES = ["uart"]
DOMAIN = "dyp_base"

CONF_BLIND_ZONE = "blind_zone"
CONF_OUTPUT_MODE_PIN = "output_mode_pin"
CONF_PUBLISH_MODE = "publish_mode"

dyp_base_ns = cg.esphome_ns.namespace("dyp_base")
DYPSensor = dyp_base_ns.class_(
    "DYPSensor", sensor.Sensor, cg.PollingComponent, uart.UARTDevice
)
DYPMode = dyp_base_ns.enum("DYPMode", is_class=True)
DYPOutputMode = dyp_base_ns.enum("DYPOutputMode", is_class=True)
DYPPublishMode = dyp_base_ns.enum("DYPPublishMode", is_class=True)

MODES = DYPMode.options("CONTROLLED", "AUTO", prefix="DYP_MODE_")
OUTPUT_MODES = DYPOutputMode.options("PROCESSED", "REALTIME", prefix="DYP_OUTPUT_MODE_")
PUBLISH_MODES = DYPPublishMode.options(
    "INTERVAL", "IMMEDIATE", prefix="DYP_PUBLISH_MODE_"
)


@dataclass(frozen=True)
class DYPModel:
    """Specification for one DYP ultrasonic sensor model."""

    blind_zone: str
    max_range: str
    baud_rate: int = 9600
    mode: str | None = None
    supports_rx_mode_select: bool = True


def _validate_model_config(model: DYPModel) -> Callable[[ConfigType], ConfigType]:
    def _validator(config: ConfigType) -> ConfigType:
        if model.mode is not None:
            config[CONF_MODE] = cv.enum(MODES, upper=True)(model.mode)

        is_auto = config[CONF_MODE] == "AUTO"
        if is_auto and CONF_TRIGGER_PIN in config:
            raise cv.Invalid("`trigger_pin` is not allowed when `mode` is 'AUTO'.")
        if not is_auto and (
            CONF_OUTPUT_MODE in config or CONF_OUTPUT_MODE_PIN in config
        ):
            raise cv.Invalid(
                "'output_mode' and 'output_mode_pin' are only allowed when 'mode' is 'AUTO'."
            )
        if CONF_OUTPUT_MODE in config and CONF_OUTPUT_MODE_PIN not in config:
            raise cv.Invalid(
                "'output_mode' requires 'output_mode_pin' to be configured."
            )
        if CONF_OUTPUT_MODE_PIN in config and CONF_OUTPUT_MODE not in config:
            config[CONF_OUTPUT_MODE] = cv.enum(OUTPUT_MODES, upper=True)("PROCESSED")
        return config

    return _validator


def _model_schema(model: DYPModel) -> cv.Schema:
    extra: dict[Any, Any] = {
        cv.Optional(CONF_BLIND_ZONE, default=model.blind_zone): cv.distance,
        cv.Optional(CONF_MAX_RANGE, default=model.max_range): cv.distance,
    }
    if model.mode is None:
        extra[cv.Optional(CONF_MODE, default="AUTO")] = cv.enum(MODES, upper=True)
        extra[cv.Optional(CONF_TRIGGER_PIN)] = pins.gpio_output_pin_schema
        extra[cv.Optional(CONF_PUBLISH_MODE, default="INTERVAL")] = cv.enum(
            PUBLISH_MODES, upper=True
        )
        if model.supports_rx_mode_select:
            extra[cv.Optional(CONF_OUTPUT_MODE)] = cv.enum(OUTPUT_MODES, upper=True)
            extra[cv.Optional(CONF_OUTPUT_MODE_PIN)] = pins.gpio_output_pin_schema
    elif model.mode == "CONTROLLED":
        extra[cv.Optional(CONF_TRIGGER_PIN)] = pins.gpio_output_pin_schema
    elif model.mode == "AUTO":
        extra[cv.Optional(CONF_PUBLISH_MODE, default="INTERVAL")] = cv.enum(
            PUBLISH_MODES, upper=True
        )
        if model.supports_rx_mode_select:
            extra[cv.Optional(CONF_OUTPUT_MODE)] = cv.enum(OUTPUT_MODES, upper=True)
            extra[cv.Optional(CONF_OUTPUT_MODE_PIN)] = pins.gpio_output_pin_schema

    base_schema = (
        sensor.sensor_schema(
            DYPSensor,
            unit_of_measurement=UNIT_METER,
            icon=ICON_ARROW_EXPAND_VERTICAL,
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
            device_class=DEVICE_CLASS_DISTANCE,
        )
        .extend(extra)
        .extend(cv.polling_component_schema("1s"))
        .extend(uart.UART_DEVICE_SCHEMA)
    )
    return cv.All(base_schema, _validate_model_config(model))


def dyp_sensor_schema(
    models: dict[str, DYPModel],
) -> Callable[[ConfigType], ConfigType]:
    """Return a typed schema for a DYP sensor series."""
    return cv.typed_schema(
        {name: _model_schema(model) for name, model in models.items()},
        key=CONF_MODEL,
        upper=True,
    )


def dyp_final_validate_schema(
    domain: str, models: dict[str, DYPModel]
) -> Callable[[ConfigType], ConfigType]:
    """Return the final validation schema for a DYP sensor series."""

    def _final_validate(config: ConfigType) -> ConfigType:
        model = models[config[CONF_MODEL]]
        is_controlled = config[CONF_MODE] == "CONTROLLED"
        require_tx = is_controlled and CONF_TRIGGER_PIN not in config
        return uart.final_validate_device_schema(
            domain,
            baud_rate=model.baud_rate,
            require_tx=require_tx,
            require_rx=True,
            data_bits=8,
            parity="NONE",
            stop_bits=1,
        )(config)

    return _final_validate


async def to_code_base(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_mode(config[CONF_MODE]))
    cg.add(var.set_blind_zone(config[CONF_BLIND_ZONE]))
    cg.add(var.set_max_range(config[CONF_MAX_RANGE]))
    cg.add_optional(var.set_publish_mode, config.get(CONF_PUBLISH_MODE))
    cg.add_optional(var.set_output_mode, config.get(CONF_OUTPUT_MODE))
    await cg.add_pin(var.set_trigger_pin, config.get(CONF_TRIGGER_PIN))
    await cg.add_pin(var.set_output_mode_pin, config.get(CONF_OUTPUT_MODE_PIN))
