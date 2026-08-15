from dataclasses import dataclass, field

from esphome import pins
import esphome.codegen as cg
from esphome.components import output
from esphome.components.zephyr import (
    zephyr_add_overlay_builder,
    zephyr_add_prj_conf,
    zephyr_variant,
    zephyr_variant_family,
)
from esphome.components.zephyr.variants import VARIANTS
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


def _pin_schema(value):
    value = pins.internal_gpio_output_pin_schema(value)
    if value.get(CONF_ALLOW_OTHER_USES, False):
        raise cv.Invalid("allow_other_uses is not supported for zephyr_pwm pins")
    return value


def _pwm_node_labels() -> list[str]:
    """Devicetree node labels of this target's PWM peripheral instances.

    platform: nrf52 predates the variant registry and always targets nRF52840, so its
    labels are hardcoded here rather than looked up in VARIANTS (same precedent as
    this component's own gain/pin handling elsewhere in the codebase).
    """
    if CORE.is_nrf52:
        return ["pwm0", "pwm1", "pwm2", "pwm3"]
    return VARIANTS[zephyr_variant()].pwm_node_labels


def _only_on_pwm_hardware(value):
    if CORE.is_nrf52:
        return value
    if (
        CORE.using_zephyr
        and zephyr_variant_family() in ("nordic", "rpi_pico")
        and _pwm_node_labels()
    ):
        return value
    raise cv.Invalid(
        "zephyr_pwm is only available on nrf52 (platform: nrf52, or platform: zephyr "
        "with a variant that has PWM hardware wired up)"
    )


CONFIG_SCHEMA = cv.All(
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.declare_id(ZephyrPWMChannel),
            cv.Required(CONF_PIN): _pin_schema,
            cv.Optional(CONF_FREQUENCY, default="1kHz"): validate_frequency,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _only_on_pwm_hardware,
)

# Fixed hardware constant of Nordic's NRF_PWM peripheral IP block (4 output channels
# per instance), consistent across nRF52/nRF54L -- verified against both families'
# devicetree bindings, not per-variant.
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

    block_count = len(_pwm_node_labels())
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
            if len(pwm_blocks) >= block_count:
                raise cv.Invalid(
                    f"Only {block_count} PWM blocks with a distinct frequency and "
                    f"{PWM_CHANNELS_PER_BLOCK} channels each are supported on this hardware"
                )
            pwm_block = PWMBlock(id=len(pwm_blocks), period_ns=period_ns, pins=[])
            pwm_blocks.append(pwm_block)
        pwm_block.pins.append(pin_number)

    _get_data().pwm_blocks = pwm_blocks


def _final_validate(config: ConfigType) -> ConfigType:
    _allocate_blocks()
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


def _overlay_pwm():
    pwm_blocks: list[PWMBlock] = _get_data().pwm_blocks
    labels = _pwm_node_labels()

    overlay_parts = []

    overlay_parts.extend(
        f"""
        &{labels[block.id]} {{
            status = "okay";
            pinctrl-0 = <&{labels[block.id]}_default_custom>;
            pinctrl-1 = <&{labels[block.id]}_sleep_custom>;
            pinctrl-names = "default", "sleep";
        }};"""
        for block in pwm_blocks
    )

    pinctls = []
    for block in pwm_blocks:
        label = labels[block.id]
        psels = ", ".join(
            f"<NRF_PSEL(PWM_OUT{channel_id}, {pin // 32}, {pin % 32})>"
            for channel_id, pin in enumerate(block.pins)
        )
        pinctls.append(f"""
            {label}_default_custom: {label}_default_custom {{
                group1 {{
                    psels = {psels};
                }};
            }};
            {label}_sleep_custom: {label}_sleep_custom {{
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


async def to_code(config):
    zephyr_add_prj_conf("PWM", True)
    pin = config[CONF_PIN]
    pwm_blocks: list[PWMBlock] = _get_data().pwm_blocks
    pwm_block = next(
        (block for block in pwm_blocks if pin[CONF_NUMBER] in block.pins), None
    )
    channel_id = pwm_block.pins.index(pin[CONF_NUMBER])

    zephyr_add_overlay_builder(_overlay_pwm)

    pin_inverted = pin.get(CONF_INVERTED, False)
    label = _pwm_node_labels()[pwm_block.id]
    var = cg.new_Pvariable(
        config[CONF_ID],
        cg.RawExpression(f"DEVICE_DT_GET_OR_NULL(DT_NODELABEL({label}))"),
        channel_id,
        pin_inverted,
        pwm_block.period_ns,
    )
    await cg.register_component(var, config)
    await output.register_output(var, config)
