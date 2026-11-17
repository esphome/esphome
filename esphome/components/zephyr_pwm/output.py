from dataclasses import dataclass, field
from typing import Any

from esphome import pins
import esphome.codegen as cg
from esphome.components import output
from esphome.components.nrf52.const import DEFAULT_PWM, VARIANT_PWM, VariantPWM
from esphome.components.nrf52.framework import get_nrf52_variant
from esphome.components.output import DOMAIN as OUTPUT_DOMAIN
from esphome.components.zephyr import zephyr_add_overlay_builder, zephyr_add_prj_conf
import esphome.config_validation as cv
from esphome.const import (
    CONF_ALLOW_OTHER_USES,
    CONF_FREQUENCY,
    CONF_ID,
    CONF_INVERTED,
    CONF_NUMBER,
    CONF_PIN,
)
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

DEPENDENCIES = ["zephyr"]
DOMAIN = "zephyr_pwm"

zephyr_pwm_ns = cg.esphome_ns.namespace("zephyr_pwm")
ZephyrPWMChannel = zephyr_pwm_ns.class_(
    "ZephyrPWMChannel", output.FloatOutput, cg.Component
)
validate_frequency = cv.All(cv.frequency, cv.float_range(min=3.815, max=1e7))


def _pin_schema(value: Any) -> ConfigType:
    value = pins.internal_gpio_output_pin_schema(value)
    if value.get(CONF_ALLOW_OTHER_USES, False):
        raise cv.Invalid("allow_other_uses is not supported for zephyr_pwm pins")
    return value


def _get_variant_pwm() -> VariantPWM:
    return VARIANT_PWM.get(get_nrf52_variant(), DEFAULT_PWM)


def _validate_variant(config: ConfigType) -> ConfigType:
    """Reject chips without PWM, and nRF54L pins outside the power domain of the PWM instances."""
    variant_pwm = _get_variant_pwm()
    if not variant_pwm.labels:
        raise cv.Invalid(f"{get_nrf52_variant()} has no PWM peripheral")
    if (ports := variant_pwm.ports) is None:
        return config
    port, pin = divmod(config[CONF_PIN][CONF_NUMBER], 32)
    if port not in ports:
        allowed = " or ".join(f"P{p}" for p in ports)
        raise cv.Invalid(
            f"P{port}.{pin:02d} is not in the power domain of the PWM peripherals; "
            f"use a pin on {allowed}",
            [CONF_PIN],
        )
    return config


CONFIG_SCHEMA = cv.All(
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.declare_id(ZephyrPWMChannel),
            cv.Required(CONF_PIN): _pin_schema,
            cv.Optional(CONF_FREQUENCY, default="1kHz"): validate_frequency,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_nrf52,
    _validate_variant,
)

PWM_CHANNELS_PER_BLOCK = 4


@dataclass
class PWMBlock:
    label: str
    period_ns: int
    pins: list[int]


@dataclass
class ZephyrPWMData:
    pwm_blocks: list[PWMBlock] = field(default_factory=list)


def _get_data() -> ZephyrPWMData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = ZephyrPWMData()
    return CORE.data[DOMAIN]


def _allocate_blocks() -> None:
    variant_pwm = _get_variant_pwm()
    labels = variant_pwm.labels
    # The nRF54L GPIO chapter says a peripheral cannot mix pins from different ports
    one_port_per_block = variant_pwm.ports is not None
    pwm_blocks: list[PWMBlock] = []
    for cfg in fv.platform_entries(domain=OUTPUT_DOMAIN, platform=DOMAIN):
        pin_number = cfg[CONF_PIN][CONF_NUMBER]
        period_ns = int(1e9 / cfg[CONF_FREQUENCY])
        pwm_block = next(
            (
                block
                for block in pwm_blocks
                if block.period_ns == period_ns
                and len(block.pins) < PWM_CHANNELS_PER_BLOCK
                and (not one_port_per_block or block.pins[0] // 32 == pin_number // 32)
            ),
            None,
        )
        if pwm_block is None:
            if len(pwm_blocks) >= len(labels):
                same_port = " and GPIO port" if one_port_per_block else ""
                raise cv.Invalid(
                    f"Only {len(labels)} PWM blocks with a distinct frequency{same_port} and "
                    f"{PWM_CHANNELS_PER_BLOCK} channels each are supported by {get_nrf52_variant()}"
                )
            pwm_block = PWMBlock(
                label=labels[len(pwm_blocks)], period_ns=period_ns, pins=[]
            )
            pwm_blocks.append(pwm_block)
        pwm_block.pins.append(pin_number)

    _get_data().pwm_blocks = pwm_blocks


def _final_validate(config: ConfigType) -> None:
    _allocate_blocks()


FINAL_VALIDATE_SCHEMA = _final_validate


def _overlay_pwm() -> str:
    pwm_blocks: list[PWMBlock] = _get_data().pwm_blocks

    assert CORE.is_nrf52

    overlay_parts = []

    overlay_parts.extend(
        f"""
        &{block.label} {{
            status = "okay";
            pinctrl-0 = <&{block.label}_default_custom>;
            pinctrl-1 = <&{block.label}_sleep_custom>;
            pinctrl-names = "default", "sleep";
        }};"""
        for block in pwm_blocks
    )

    pinctls = []
    for block in pwm_blocks:
        psels = ", ".join(
            f"<NRF_PSEL(PWM_OUT{channel_id}, {pin // 32}, {pin % 32})>"
            for channel_id, pin in enumerate(block.pins)
        )
        pinctls.append(f"""
            {block.label}_default_custom: {block.label}_default_custom {{
                group1 {{
                    psels = {psels};
                }};
            }};
            {block.label}_sleep_custom: {block.label}_sleep_custom {{
                group1 {{
                    psels = {psels};
                    low-power-enable;
                }};
            }};""")

    overlay_parts.append(f"""
        &pinctrl {{
         {"\n".join(pinctls)}
        }};""")
    return "\n".join(overlay_parts)


async def to_code(config: ConfigType) -> None:
    zephyr_add_prj_conf("PWM", True)
    pin = config[CONF_PIN]
    pwm_blocks: list[PWMBlock] = _get_data().pwm_blocks
    pwm_block = next(
        (block for block in pwm_blocks if pin[CONF_NUMBER] in block.pins), None
    )
    channel_id = pwm_block.pins.index(pin[CONF_NUMBER])

    zephyr_add_overlay_builder(_overlay_pwm)

    pin_inverted = pin.get(CONF_INVERTED, False)
    var = cg.new_Pvariable(
        config[CONF_ID],
        cg.RawExpression(f"DEVICE_DT_GET_OR_NULL(DT_NODELABEL({pwm_block.label}))"),
        channel_id,
        pin_inverted,
        pwm_block.period_ns,
    )
    await cg.register_component(var, config)
    await output.register_output(var, config)
