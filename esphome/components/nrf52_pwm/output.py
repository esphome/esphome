from __future__ import annotations

from dataclasses import dataclass

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import output
from esphome.components.zephyr import zephyr_add_overlay, zephyr_add_prj_conf
import esphome.config_validation as cv
from esphome.const import CONF_FREQUENCY, CONF_ID, CONF_NUMBER, CONF_PIN
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.cpp_generator import RawExpression
import esphome.final_validate as fv

DEPENDENCIES = ["nrf52"]

_MAX_PWM_PERIPHERALS = 3
_MAX_CHANNELS_PER_PWM = 4
_DATA_KEY = "nrf52_pwm"

# Zephyr's nordic,nrf-pwm binding requires pinctrl groups for enabled PWM devices.
# https://docs.zephyrproject.org/latest/build/dts/api/bindings/pwm/nordic,nrf-pwm.html


@dataclass
class Nrf52PWMData:
    configs: list[dict]
    final_job_scheduled: bool = False


def _get_data() -> Nrf52PWMData:
    if _DATA_KEY not in CORE.data:
        CORE.data[_DATA_KEY] = Nrf52PWMData(configs=[])
    return CORE.data[_DATA_KEY]


def _frequency_key(config: dict) -> int:
    return int(round(config[CONF_FREQUENCY]))


def _allocate_pwm(configs: list[dict]) -> list[tuple[dict, int, int, int]]:
    frequency_groups: dict[int, list[dict]] = {}
    for config in configs:
        frequency_groups.setdefault(_frequency_key(config), []).append(config)

    allocations: list[tuple[dict, int, int, int]] = []
    pwm_num = 0
    for group in frequency_groups.values():
        for offset in range(0, len(group), _MAX_CHANNELS_PER_PWM):
            if pwm_num >= _MAX_PWM_PERIPHERALS:
                raise cv.Invalid(
                    "nrf52_pwm supports up to 3 PWM peripherals with 4 channels each. "
                    "Use fewer distinct frequencies or fewer outputs."
                )
            chunk = group[offset : offset + _MAX_CHANNELS_PER_PWM]
            for channel, config in enumerate(chunk):
                allocations.append((config, pwm_num, channel, len(chunk)))
            pwm_num += 1
    return allocations


def _final_validate(config):
    all_outputs = fv.full_config.get().get("output", [])
    configs = [conf for conf in all_outputs if conf.get("platform") == "nrf52_pwm"]
    _allocate_pwm(configs)


FINAL_VALIDATE_SCHEMA = _final_validate


nrf52_pwm_ns = cg.esphome_ns.namespace("nrf52_pwm")
Nrf52PWMOutput = nrf52_pwm_ns.class_("Nrf52PWMOutput", output.FloatOutput, cg.Component)
SetFrequencyAction = nrf52_pwm_ns.class_("SetFrequencyAction", automation.Action)
validate_frequency = cv.All(cv.frequency, cv.float_range(min=1.0e-6))

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(Nrf52PWMOutput),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_FREQUENCY, default="1kHz"): validate_frequency,
    }
).extend(cv.COMPONENT_SCHEMA)


def _add_pwm_overlay(allocations: list[tuple[dict, int, int, int]]) -> None:
    pwm_groups: dict[int, list[tuple[dict, int]]] = {}
    for config, pwm_num, channel, _ in allocations:
        pwm_groups.setdefault(pwm_num, []).append((config, channel))

    pinctrl_entries = []
    peripheral_entries = []
    for pwm_num, items in sorted(pwm_groups.items()):
        psel_entries = []
        for config, channel in items:
            pin_num = config[CONF_PIN][CONF_NUMBER]
            psel_entries.append(
                f"<NRF_PSEL(PWM_OUT{channel}, {pin_num // 32}, {pin_num % 32})>"
            )
        psels = ",\n                    ".join(psel_entries)
        pinctrl_entries.append(
            f"""
            pwm{pwm_num}_default: pwm{pwm_num}_default {{
                group1 {{
                    psels = {psels};
                }};
            }};
            pwm{pwm_num}_sleep: pwm{pwm_num}_sleep {{
                group1 {{
                    psels = {psels};
                    low-power-enable;
                }};
            }};
            """
        )
        peripheral_entries.append(
            f"""
            &pwm{pwm_num} {{
                status = "okay";
                pinctrl-0 = <&pwm{pwm_num}_default>;
                pinctrl-1 = <&pwm{pwm_num}_sleep>;
                pinctrl-names = "default", "sleep";
            }};
            """
        )

    zephyr_add_overlay(
        f"""
        &pinctrl {{
            {"".join(pinctrl_entries)}
        }};
        {"".join(peripheral_entries)}
        """
    )


@coroutine_with_priority(CoroPriority.FINAL)
async def _finalize_pwm_outputs() -> None:
    data = _get_data()
    allocations = _allocate_pwm(data.configs)
    _add_pwm_overlay(allocations)

    for config, pwm_num, channel, group_size in allocations:
        var = await cg.get_variable(config[CONF_ID])
        device = RawExpression(f"DEVICE_DT_GET(DT_NODELABEL(pwm{pwm_num}))")
        cg.add(var.set_device(device))
        cg.add(var.set_channel(channel))
        cg.add(var.set_runtime_frequency_mutable(group_size == 1))


async def to_code(config) -> None:
    zephyr_add_prj_conf("PWM", True)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))

    data = _get_data()
    if not data.final_job_scheduled:
        CORE.add_job(_finalize_pwm_outputs)
        data.final_job_scheduled = True
    data.configs.append(config)


@automation.register_action(
    "output.nrf52_pwm.set_frequency",
    SetFrequencyAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(Nrf52PWMOutput),
            cv.Required(CONF_FREQUENCY): cv.templatable(validate_frequency),
        }
    ),
    synchronous=True,
)
async def nrf52_pwm_set_frequency_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    template_ = await cg.templatable(config[CONF_FREQUENCY], args, cg.float_)
    cg.add(var.set_frequency(template_))
    return var
