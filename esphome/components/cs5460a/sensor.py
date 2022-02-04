import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi, sensor
from esphome.const import (
    CONF_CURRENT,
    CONF_ID,
    CONF_POWER,
    CONF_VOLTAGE,
    UNIT_VOLT,
    UNIT_AMPERE,
    UNIT_WATT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_VOLTAGE,
)
from esphome import automation
from esphome.automation import maybe_simple_id

CODEOWNERS = ["@balrog-kun"]
DEPENDENCIES = ["spi"]

cs5460a_ns = cg.esphome_ns.namespace("cs5460a")
CS5460APGAGain = cs5460a_ns.enum("CS5460APGAGain")
PGA_GAIN_OPTIONS = {
    "10X": CS5460APGAGain.CS5460A_PGA_GAIN_10X,
    "50X": CS5460APGAGain.CS5460A_PGA_GAIN_50X,
}

CS5460AComponent = cs5460a_ns.class_("CS5460AComponent", spi.SPIDevice, cg.Component)
CS5460ARestartAction = cs5460a_ns.class_("CS5460ARestartAction", automation.Action)

CONF_SAMPLES = "samples"
CONF_PHASE_OFFSET = "phase_offset"
CONF_PGA_GAIN = "pga_gain"
CONF_CURRENT_GAIN = "current_gain"
CONF_VOLTAGE_GAIN = "voltage_gain"
CONF_CURRENT_HPF = "current_hpf"
CONF_VOLTAGE_HPF = "voltage_hpf"
CONF_PULSE_ENERGY = "pulse_energy"


def validate_config(config):
    current_gain = abs(config[CONF_CURRENT_GAIN]) * (
        1.0 if config[CONF_PGA_GAIN] == "10X" else 5.0
    )
    voltage_gain = config[CONF_VOLTAGE_GAIN]
    pulse_energy = config[CONF_PULSE_ENERGY]

    if current_gain == 0.0 or voltage_gain == 0.0:
        raise cv.Invalid("The gains can't be zero")

    max_energy = (0.25 * 0.25 / 3600 / (2**-4)) / (voltage_gain * current_gain)
    min_energy = (0.25 * 0.25 / 3600 / (2**18)) / (voltage_gain * current_gain)
    mech_min_energy = (0.25 * 0.25 / 3600 / 7.8) / (voltage_gain * current_gain)
    if pulse_energy < min_energy or pulse_energy > max_energy:
        raise cv.Invalid(
            "For given current&voltage gains, the pulse energy must be between "
            f"{min_energy} Wh and {max_energy} Wh and in mechanical counter mode "
            f"between {mech_min_energy} Wh and {max_energy} Wh"
        )

    return config


validate_energy = cv.float_with_unit("energy", "(Wh|WH|wh)?", optional_unit=True)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CS5460AComponent),
            cv.Optional(CONF_SAMPLES, default=4000): cv.int_range(min=1, max=0xFFFFFF),
            cv.Optional(CONF_PHASE_OFFSET, default=0): cv.int_range(min=-64, max=63),
            cv.Optional(CONF_PGA_GAIN, default="10X"): cv.enum(
                PGA_GAIN_OPTIONS, upper=True
            ),
            cv.Optional(CONF_CURRENT_GAIN, default=0.001): cv.negative_one_to_one_float,
            cv.Optional(CONF_VOLTAGE_GAIN, default=0.001): cv.zero_to_one_float,
            cv.Optional(CONF_CURRENT_HPF, default=True): cv.boolean,
            cv.Optional(CONF_VOLTAGE_HPF, default=True): cv.boolean,
            cv.Optional(CONF_PULSE_ENERGY, default=10.0): validate_energy,
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_VOLTAGE,
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_CURRENT,
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=False)),
    validate_config,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_samples(config[CONF_SAMPLES]))
    cg.add(var.set_phase_offset(config[CONF_PHASE_OFFSET]))
    cg.add(var.set_pga_gain(config[CONF_PGA_GAIN]))
    cg.add(var.set_gains(config[CONF_CURRENT_GAIN], config[CONF_VOLTAGE_GAIN]))
    cg.add(var.set_hpf_enable(config[CONF_CURRENT_HPF], config[CONF_VOLTAGE_HPF]))
    cg.add(var.set_pulse_energy_wh(config[CONF_PULSE_ENERGY]))

    if CONF_VOLTAGE in config:
        conf = config[CONF_VOLTAGE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_voltage_sensor(sens))
    if CONF_CURRENT in config:
        conf = config[CONF_CURRENT]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_current_sensor(sens))
    if CONF_POWER in config:
        conf = config[CONF_POWER]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_power_sensor(sens))


@automation.register_action(
    "cs5460a.restart",
    CS5460ARestartAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(CS5460AComponent),
        }
    ),
)
async def restart_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
