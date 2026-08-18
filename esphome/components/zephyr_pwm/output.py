from collections.abc import Callable
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


def _pwm_pin_map() -> dict[int, tuple[int, int]]:
    """GPIO -> (block index, local channel) for variants whose PWM pins are wired
    to one fixed hardware position each. Empty for variants using the free
    allocator instead (nrf52/nordic)."""
    if CORE.is_nrf52:
        return {}
    return VARIANTS[zephyr_variant()].pwm_pin_map


def _only_on_pwm_hardware(value):
    if CORE.is_nrf52:
        return value
    if CORE.using_zephyr:
        family = zephyr_variant_family()
        if family in ("nordic", "rpi_pico") and _pwm_node_labels():
            return value
        if family == "renesas" and _pwm_pin_map():
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


def _pwm_channels_per_block() -> int:
    """Channels per PWM block on this target's hardware.

    platform: nrf52 predates the variant registry (see _pwm_node_labels' docstring),
    so its channel count is hardcoded here too: 4, Nordic's NRF_PWM peripheral IP
    block, consistent across nRF52/nRF54L.
    """
    if CORE.is_nrf52:
        return 4
    return VARIANTS[zephyr_variant()].pwm_channels_per_block


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


def _rpi_pico_pwm_slice_channel(pin: int) -> tuple[int, int]:
    """RP2040/RP2350's fixed GPIO -> (slice, local channel A=0/B=1) mapping.

    8 slices, 2 channels each, repeating every 16 pins -- confirmed against
    upstream Zephyr's rpi-pico-pinctrl-common.h, which enumerates this exact
    mapping for P0-P29 (the full range this variant's gpio_port_width=30 exposes).
    """
    return (pin // 2) % 8, pin % 2


def _allocate_blocks_free(
    zephyr_pwm_conf: list[ConfigType], block_count: int, channels_per_block: int
) -> list[PWMBlock]:
    """Nordic-style allocation: any pin can be wired to any channel on any
    instance, so pack pins sharing a frequency into the same block greedily."""
    pwm_blocks: list[PWMBlock] = []
    for cfg in zephyr_pwm_conf:
        pin_number = cfg[CONF_PIN][CONF_NUMBER]
        period_ns = int(1e9 / cfg[CONF_FREQUENCY])
        pwm_block = next(
            (
                block
                for block in pwm_blocks
                if block.period_ns == period_ns and len(block.pins) < channels_per_block
            ),
            None,
        )
        if pwm_block is None:
            if len(pwm_blocks) >= block_count:
                raise cv.Invalid(
                    f"Only {block_count} PWM blocks with a distinct frequency and "
                    f"{channels_per_block} channels each are supported on this hardware"
                )
            pwm_block = PWMBlock(id=len(pwm_blocks), period_ns=period_ns, pins=[])
            pwm_blocks.append(pwm_block)
        pwm_block.pins.append(pin_number)
    return pwm_blocks


def _allocate_blocks_fixed(
    zephyr_pwm_conf: list[ConfigType],
    resolve: Callable[[int], tuple[int, int] | None],
) -> list[PWMBlock]:
    """RP2040/RP2350/RA4M1-style allocation: each pin's block (hardware slice or
    GPT instance) is fixed by silicon wiring, not freely chosen -- group by that
    fixed block and reject pins sharing one block but requesting different
    frequencies (they'd share one physical counter/period register)."""
    blocks: dict[int, PWMBlock] = {}
    for cfg in zephyr_pwm_conf:
        pin_number = cfg[CONF_PIN][CONF_NUMBER]
        period_ns = int(1e9 / cfg[CONF_FREQUENCY])
        block_channel = resolve(pin_number)
        if block_channel is None:
            raise cv.Invalid(
                f"Pin {pin_number} does not have PWM hardware wired up on this variant"
            )
        block_id, _channel = block_channel
        block = blocks.get(block_id)
        if block is None:
            block = PWMBlock(id=block_id, period_ns=period_ns, pins=[])
            blocks[block_id] = block
        elif block.period_ns != period_ns:
            raise cv.Invalid(
                f"Pins {block.pins[0]} and {pin_number} share one PWM hardware "
                "counter and must use the same frequency"
            )
        block.pins.append(pin_number)
    return list(blocks.values())


def _allocate_blocks() -> None:
    full_config = fv.full_config.get()
    zephyr_pwm_conf = [
        cfg
        for cfg in full_config.get(CONF_OUTPUT, [])
        if cfg.get(CONF_PLATFORM) == DOMAIN
    ]

    if CORE.using_zephyr and zephyr_variant_family() == "rpi_pico":
        pwm_blocks = _allocate_blocks_fixed(
            zephyr_pwm_conf, _rpi_pico_pwm_slice_channel
        )
    elif CORE.using_zephyr and zephyr_variant_family() == "renesas":
        pwm_blocks = _allocate_blocks_fixed(zephyr_pwm_conf, _pwm_pin_map().get)
    else:
        pwm_blocks = _allocate_blocks_free(
            zephyr_pwm_conf, len(_pwm_node_labels()), _pwm_channels_per_block()
        )

    _get_data().pwm_blocks = pwm_blocks


def _final_validate(config: ConfigType) -> None:
    _allocate_blocks()


FINAL_VALIDATE_SCHEMA = _final_validate


def _rpi_pico_pwm_pinmux(pin: int) -> str:
    """Zephyr's PWM_<slice><A|B>_P<pin> pinctrl macro name for a given GPIO."""
    slice_id, channel = _rpi_pico_pwm_slice_channel(pin)
    return f"PWM_{slice_id}{'A' if channel == 0 else 'B'}_P{pin}"


def _overlay_pwm_rpi_pico(pwm_blocks: list[PWMBlock]) -> str:
    """RP2040/RP2350 overlay: a single "pwm" controller node, not one node per
    block -- each block is a hardware slice (2 channels sharing one period)
    addressed by global channel number, all pinmux'd together in one shared
    pinctrl group. No "sleep" pinctrl state: unlike Nordic's psels,
    low-power-enable isn't a supported property on this pinctrl binding."""
    pinmuxes = ", ".join(
        f"<{_rpi_pico_pwm_pinmux(pin)}>" for block in pwm_blocks for pin in block.pins
    )
    return f"""
        &pwm {{
            status = "okay";
            pinctrl-0 = <&pwm_default_custom>;
            pinctrl-names = "default";
        }};

        &pinctrl {{
            pwm_default_custom: pwm_default_custom {{
                group1 {{
                    pinmux = {pinmuxes};
                }};
            }};
        }};"""


def _overlay_pwm_renesas(pwm_blocks: list[PWMBlock], labels: list[str]) -> str:
    """RA4M1 overlay: one node per GPT instance (like Nordic), but only a single
    "default" pinctrl state -- low-power-enable isn't a supported property on
    this pinctrl binding, matching ek_ra4m1's own pwm1 example."""
    pin_map = _pwm_pin_map()
    overlay_parts = []

    overlay_parts.extend(
        f"""
        &{labels[block.id]} {{
            status = "okay";
            pinctrl-0 = <&{labels[block.id]}_default_custom>;
            pinctrl-names = "default";
        }};"""
        for block in pwm_blocks
    )

    pinctls = []
    for block in pwm_blocks:
        label = labels[block.id]
        gpt_num = label.removeprefix("pwm")
        psels = ", ".join(
            f"<RA_PSEL(RA_PSEL_GPT{gpt_num}, {pin // 16}, {pin % 16})>"
            for pin in sorted(block.pins, key=lambda pin: pin_map[pin][1])
        )
        pinctls.append(f"""
            {label}_default_custom: {label}_default_custom {{
                group1 {{
                    psels = {psels};
                }};
            }};""")

    overlay_parts.append(f"""
        &pinctrl {{
         {"\n".join(pinctls)}
        }};""")
    return "\n".join(overlay_parts)


def _overlay_pwm_nordic(pwm_blocks: list[PWMBlock], labels: list[str]) -> str:
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


def _overlay_pwm():
    pwm_blocks: list[PWMBlock] = _get_data().pwm_blocks
    if CORE.using_zephyr:
        family = zephyr_variant_family()
        if family == "rpi_pico":
            return _overlay_pwm_rpi_pico(pwm_blocks)
        if family == "renesas":
            return _overlay_pwm_renesas(pwm_blocks, _pwm_node_labels())
    return _overlay_pwm_nordic(pwm_blocks, _pwm_node_labels())


async def to_code(config):
    zephyr_add_prj_conf("PWM", True)
    pin = config[CONF_PIN]
    pwm_blocks: list[PWMBlock] = _get_data().pwm_blocks
    pwm_block = next(
        (block for block in pwm_blocks if pin[CONF_NUMBER] in block.pins), None
    )
    if CORE.using_zephyr and zephyr_variant_family() == "rpi_pico":
        # RP2040/RP2350 share one "pwm" device across all blocks (slices), so the
        # driver's channel argument must be the pin's own real global slice*2+A/B
        # index -- not its position within the block, which is allocation order
        # and unrelated to the pin's actual fixed hardware channel.
        slice_id, local_channel = _rpi_pico_pwm_slice_channel(pin[CONF_NUMBER])
        channel_id = slice_id * 2 + local_channel
    elif CORE.using_zephyr and zephyr_variant_family() == "renesas":
        _, channel_id = _pwm_pin_map()[pin[CONF_NUMBER]]
    else:
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
