from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import output
from esphome.components.zephyr import zephyr_add_overlay_builder, zephyr_add_prj_conf
import esphome.config_validation as cv
from esphome.const import (
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

zephyr_pwm_ns = cg.esphome_ns.namespace("zephyr_pwm")
ZephyrPWMChannel = zephyr_pwm_ns.class_(
    "ZephyrPWMChannel", output.FloatOutput, cg.Component
)
SetFrequencyAction = zephyr_pwm_ns.class_("SetFrequencyAction", automation.Action)
validate_frequency = cv.All(cv.frequency, cv.float_range(min=1.0, max=1e7))


CONFIG_SCHEMA = cv.All(
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.declare_id(ZephyrPWMChannel),
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_FREQUENCY, default="1kHz"): validate_frequency,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_nrf52,
)

PWM_BLOCK_COUNT = 4
PWM_CHANNELS_PER_BLOCK = 4


def _final_validate(config: ConfigType) -> ConfigType:
    full_config = fv.full_config.get()
    zephyr_pwm_conf = [
        cfg
        for cfg in full_config.get(CONF_OUTPUT, [])
        if cfg.get(CONF_PLATFORM) == "zephyr_pwm"
    ]
    if zephyr_pwm_conf and len(zephyr_pwm_conf) > (
        PWM_BLOCK_COUNT * PWM_CHANNELS_PER_BLOCK
    ):
        raise cv.Invalid(
            f"Only {PWM_BLOCK_COUNT * PWM_CHANNELS_PER_BLOCK} PWM outputs are supported by nrf52"
        )

    return config


FINAL_VALIDATE_SCHEMA = _final_validate

CONF_ZEPHYR_PWM_PINS = "zephyr_pwm_pins"


def _overlay_pwm():
    pwm_pins = CORE.data[CONF_ZEPHYR_PWM_PINS]

    assert CORE.is_nrf52
    pwm_pins_by_block = [
        pwm_pins[i : i + PWM_CHANNELS_PER_BLOCK]
        for i in range(0, len(pwm_pins), PWM_CHANNELS_PER_BLOCK)
    ]

    overlay_parts = []

    overlay_parts.extend(
        f"""
        &pwm{block_id} {{
            status = "okay";
            pinctrl-0 = <&pwm{block_id}_default_custom>;
            pinctrl-1 = <&pwm{block_id}_sleep_custom>;
            pinctrl-names = "default", "sleep";
        }};"""
        for block_id in range(len(pwm_pins_by_block))
    )

    psels_by_block = [
        ", ".join(
            f"<NRF_PSEL(PWM_OUT{channel_id}, {pin // 32}, {pin % 32})>"
            for channel_id, pin in enumerate(block_pwm_pins)
        )
        for block_pwm_pins in pwm_pins_by_block
    ]
    pinctls = "\n".join(
        f"""
            pwm{block_id}_default_custom: pwm{block_id}_default_custom {{
                group1 {{
                    psels = {block_psels};
                }};
            }};
            pwm{block_id}_sleep_custom: pwm{block_id}_sleep_custom {{
                group1 {{
                    psels = {block_psels};
                    low-power-enable;
                }};
            }};"""
        for block_id, block_psels in enumerate(psels_by_block)
    )

    overlay_parts.append(f"""
        &pinctrl {{
         {pinctls}
        }};""")
    return "\n".join(overlay_parts)


async def to_code(config):
    assert CORE.is_nrf52
    zephyr_add_prj_conf("PWM", True)
    pin = config[CONF_PIN]

    CORE.data.setdefault(CONF_ZEPHYR_PWM_PINS, [])
    pwm_pins = CORE.data[CONF_ZEPHYR_PWM_PINS]
    pwm_id = len(pwm_pins)
    assert pwm_id < (PWM_BLOCK_COUNT * PWM_CHANNELS_PER_BLOCK), (
        f"Only {PWM_BLOCK_COUNT * PWM_CHANNELS_PER_BLOCK} PWM outputs are supported by nrf52"
    )

    block_id = pwm_id // PWM_CHANNELS_PER_BLOCK
    channel_id = pwm_id % PWM_CHANNELS_PER_BLOCK
    pwm_pins.append(pin[CONF_NUMBER])
    zephyr_add_overlay_builder(_overlay_pwm)

    inverted = pin.get(CONF_INVERTED, False)
    var = cg.new_Pvariable(
        config[CONF_ID],
        cg.RawExpression(f"DEVICE_DT_GET_OR_NULL(DT_NODELABEL(pwm{block_id}))"),
        channel_id,
        inverted,
    )
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))
    await cg.register_component(var, config)
    await output.register_output(var, config)


@automation.register_action(
    "output.zephyr_pwm.set_frequency",
    SetFrequencyAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ZephyrPWMChannel),
            cv.Required(CONF_FREQUENCY): cv.templatable(validate_frequency),
        }
    ),
    synchronous=True,
)
async def zephyr_pwm_set_frequency_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_FREQUENCY], args, cg.float_)
    cg.add(var.set_frequency(template_))
    return var
