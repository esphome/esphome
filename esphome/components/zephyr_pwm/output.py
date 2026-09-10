from dataclasses import dataclass, field
from typing import Any

from esphome import pins
import esphome.codegen as cg
from esphome.components import output
from esphome.components.zephyr import zephyr_add_overlay_builder, zephyr_add_prj_conf
import esphome.config_validation as cv
from esphome.const import (
    CONF_ALLOW_OTHER_USES,
    CONF_FREQUENCY,
    CONF_ID,
    CONF_INVERTED,
    CONF_NUMBER,
    CONF_OUTPUT,
    CONF_PIN,
    CONF_PLATFORM,
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


CONFIG_SCHEMA = cv.All(
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.declare_id(ZephyrPWMChannel),
            cv.Required(CONF_PIN): _pin_schema,
            cv.Optional(CONF_FREQUENCY, default="1kHz"): validate_frequency,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_nrf52,
)

PWM_BLOCK_COUNT = 4
PWM_CHANNELS_PER_BLOCK = 4


@dataclass
class PWMBlock:
    id: int
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
    full_config = fv.full_config.get()
    zephyr_pwm_conf = [
        cfg
        for cfg in full_config.get(CONF_OUTPUT, [])
        if cfg.get(CONF_PLATFORM) == DOMAIN
    ]

    pwm_blocks: list[PWMBlock] = []
    for cfg in zephyr_pwm_conf:
        pin_number = cfg[CONF_PIN][CONF_NUMBER]
        period_ns = int(1e9 / cfg[CONF_FREQUENCY])
        pwm_block = next(
            (
                block
                for block in pwm_blocks
                if block.period_ns == period_ns
                and len(block.pins) < PWM_CHANNELS_PER_BLOCK
            ),
            None,
        )
        if pwm_block is None:
            if len(pwm_blocks) >= PWM_BLOCK_COUNT:
                raise cv.Invalid(
                    f"Only {PWM_BLOCK_COUNT} PWM blocks with a distinct frequency and {PWM_CHANNELS_PER_BLOCK} channels each are supported by nrf52"
                )
            pwm_block = PWMBlock(id=len(pwm_blocks), period_ns=period_ns, pins=[])
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
        &pwm{block.id} {{
            status = "okay";
            pinctrl-0 = <&pwm{block.id}_default_custom>;
            pinctrl-1 = <&pwm{block.id}_sleep_custom>;
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
            pwm{block.id}_default_custom: pwm{block.id}_default_custom {{
                group1 {{
                    psels = {psels};
                }};
            }};
            pwm{block.id}_sleep_custom: pwm{block.id}_sleep_custom {{
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
        cg.RawExpression(f"DEVICE_DT_GET_OR_NULL(DT_NODELABEL(pwm{pwm_block.id}))"),
        channel_id,
        pin_inverted,
        pwm_block.period_ns,
    )
    await cg.register_component(var, config)
    await output.register_output(var, config)
