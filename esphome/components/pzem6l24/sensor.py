from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import modbus, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
    CONF_ID,
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_REACTIVE_ENERGY,
    DEVICE_CLASS_REACTIVE_POWER,
    DEVICE_CLASS_VOLTAGE,
    ICON_CURRENT_AC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_HERTZ,
    UNIT_KILOVOLT_AMPS_HOURS,
    UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_VOLT_AMPS_REACTIVE,
    UNIT_WATT,
)
from esphome.types import ConfigType

AUTO_LOAD = ["modbus"]
CODEOWNERS = ["@nuttytree"]

pzem6l24_ns = cg.esphome_ns.namespace("pzem6l24")
PZEM6L24 = pzem6l24_ns.class_(
    "PZEM6L24", cg.PollingComponent, modbus.ModbusClientDevice
)
ResetEnergyAction = pzem6l24_ns.class_("ResetEnergyAction", automation.Action)

ResetPhase = pzem6l24_ns.enum("ResetPhase")
RESET_PHASE_OPTIONS = {
    "all": ResetPhase.RESET_PHASE_ALL,
    "a": ResetPhase.RESET_PHASE_A,
    "b": ResetPhase.RESET_PHASE_B,
    "c": ResetPhase.RESET_PHASE_C,
    "combined": ResetPhase.RESET_PHASE_COMBINED,
}

# Per-phase config keys
CONF_VOLTAGE_A = "voltage_a"
CONF_VOLTAGE_B = "voltage_b"
CONF_VOLTAGE_C = "voltage_c"
CONF_CURRENT_A = "current_a"
CONF_CURRENT_B = "current_b"
CONF_CURRENT_C = "current_c"
CONF_ACTIVE_POWER_A = "active_power_a"
CONF_ACTIVE_POWER_B = "active_power_b"
CONF_ACTIVE_POWER_C = "active_power_c"
CONF_REACTIVE_POWER_A = "reactive_power_a"
CONF_REACTIVE_POWER_B = "reactive_power_b"
CONF_REACTIVE_POWER_C = "reactive_power_c"
CONF_APPARENT_POWER_A = "apparent_power_a"
CONF_APPARENT_POWER_B = "apparent_power_b"
CONF_APPARENT_POWER_C = "apparent_power_c"
CONF_POWER_FACTOR_A = "power_factor_a"
CONF_POWER_FACTOR_B = "power_factor_b"
CONF_POWER_FACTOR_C = "power_factor_c"
CONF_ACTIVE_ENERGY_A = "active_energy_a"
CONF_ACTIVE_ENERGY_B = "active_energy_b"
CONF_ACTIVE_ENERGY_C = "active_energy_c"
CONF_REACTIVE_ENERGY_A = "reactive_energy_a"
CONF_REACTIVE_ENERGY_B = "reactive_energy_b"
CONF_REACTIVE_ENERGY_C = "reactive_energy_c"
CONF_APPARENT_ENERGY_A = "apparent_energy_a"
CONF_APPARENT_ENERGY_B = "apparent_energy_b"
CONF_APPARENT_ENERGY_C = "apparent_energy_c"
# Combined config keys
CONF_TOTAL_ACTIVE_POWER = "total_active_power"
CONF_TOTAL_REACTIVE_POWER = "total_reactive_power"
CONF_TOTAL_APPARENT_POWER = "total_apparent_power"
CONF_TOTAL_POWER_FACTOR = "total_power_factor"
CONF_TOTAL_ACTIVE_ENERGY = "total_active_energy"
CONF_TOTAL_REACTIVE_ENERGY = "total_reactive_energy"
CONF_TOTAL_APPARENT_ENERGY = "total_apparent_energy"
CONF_PHASE = "phase"


def _voltage_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _current_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _active_power_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _reactive_power_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT_AMPS_REACTIVE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_REACTIVE_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _apparent_power_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT_AMPS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_APPARENT_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _power_factor_schema():
    return sensor.sensor_schema(
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_POWER_FACTOR,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _active_energy_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    )


def _reactive_energy_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_REACTIVE_ENERGY,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    )


def _apparent_energy_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOVOLT_AMPS_HOURS,
        accuracy_decimals=1,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    )


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PZEM6L24),
            # Per-phase voltage
            cv.Optional(CONF_VOLTAGE_A): _voltage_schema(),
            cv.Optional(CONF_VOLTAGE_B): _voltage_schema(),
            cv.Optional(CONF_VOLTAGE_C): _voltage_schema(),
            # Per-phase current
            cv.Optional(CONF_CURRENT_A): _current_schema(),
            cv.Optional(CONF_CURRENT_B): _current_schema(),
            cv.Optional(CONF_CURRENT_C): _current_schema(),
            # Per-phase active power
            cv.Optional(CONF_ACTIVE_POWER_A): _active_power_schema(),
            cv.Optional(CONF_ACTIVE_POWER_B): _active_power_schema(),
            cv.Optional(CONF_ACTIVE_POWER_C): _active_power_schema(),
            # Per-phase reactive power
            cv.Optional(CONF_REACTIVE_POWER_A): _reactive_power_schema(),
            cv.Optional(CONF_REACTIVE_POWER_B): _reactive_power_schema(),
            cv.Optional(CONF_REACTIVE_POWER_C): _reactive_power_schema(),
            # Per-phase apparent power
            cv.Optional(CONF_APPARENT_POWER_A): _apparent_power_schema(),
            cv.Optional(CONF_APPARENT_POWER_B): _apparent_power_schema(),
            cv.Optional(CONF_APPARENT_POWER_C): _apparent_power_schema(),
            # Per-phase power factor
            cv.Optional(CONF_POWER_FACTOR_A): _power_factor_schema(),
            cv.Optional(CONF_POWER_FACTOR_B): _power_factor_schema(),
            cv.Optional(CONF_POWER_FACTOR_C): _power_factor_schema(),
            # Per-phase active energy
            cv.Optional(CONF_ACTIVE_ENERGY_A): _active_energy_schema(),
            cv.Optional(CONF_ACTIVE_ENERGY_B): _active_energy_schema(),
            cv.Optional(CONF_ACTIVE_ENERGY_C): _active_energy_schema(),
            # Per-phase reactive energy
            cv.Optional(CONF_REACTIVE_ENERGY_A): _reactive_energy_schema(),
            cv.Optional(CONF_REACTIVE_ENERGY_B): _reactive_energy_schema(),
            cv.Optional(CONF_REACTIVE_ENERGY_C): _reactive_energy_schema(),
            # Per-phase apparent energy
            cv.Optional(CONF_APPARENT_ENERGY_A): _apparent_energy_schema(),
            cv.Optional(CONF_APPARENT_ENERGY_B): _apparent_energy_schema(),
            cv.Optional(CONF_APPARENT_ENERGY_C): _apparent_energy_schema(),
            # Combined sensors
            cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
                unit_of_measurement=UNIT_HERTZ,
                icon=ICON_CURRENT_AC,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_FREQUENCY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TOTAL_ACTIVE_POWER): _active_power_schema(),
            cv.Optional(CONF_TOTAL_REACTIVE_POWER): _reactive_power_schema(),
            cv.Optional(CONF_TOTAL_APPARENT_POWER): _apparent_power_schema(),
            cv.Optional(CONF_TOTAL_POWER_FACTOR): _power_factor_schema(),
            cv.Optional(CONF_TOTAL_ACTIVE_ENERGY): _active_energy_schema(),
            cv.Optional(CONF_TOTAL_REACTIVE_ENERGY): _reactive_energy_schema(),
            cv.Optional(CONF_TOTAL_APPARENT_ENERGY): _apparent_energy_schema(),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(modbus.modbus_device_schema(0x01))
)


@automation.register_action(
    "pzem6l24.reset_energy",
    ResetEnergyAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(PZEM6L24),
            cv.Optional(CONF_PHASE, default="all"): cv.enum(
                RESET_PHASE_OPTIONS, lower=True
            ),
        }
    ),
    synchronous=True,
)
async def reset_energy_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    cg.add(var.set_phase(config[CONF_PHASE]))
    return var


async def _register_sensor(var, config, conf_key, setter_name):
    if (conf := config.get(conf_key)) is not None:
        sens = await sensor.new_sensor(conf)
        cg.add(getattr(var, setter_name)(sens))


def _final_validate(config: ConfigType) -> ConfigType:
    return modbus.final_validate_modbus_device("pzem6l24", role="client")(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_client_device(var, config)

    # Per-phase voltage
    await _register_sensor(var, config, CONF_VOLTAGE_A, "set_voltage_sensor_a")
    await _register_sensor(var, config, CONF_VOLTAGE_B, "set_voltage_sensor_b")
    await _register_sensor(var, config, CONF_VOLTAGE_C, "set_voltage_sensor_c")
    # Per-phase current
    await _register_sensor(var, config, CONF_CURRENT_A, "set_current_sensor_a")
    await _register_sensor(var, config, CONF_CURRENT_B, "set_current_sensor_b")
    await _register_sensor(var, config, CONF_CURRENT_C, "set_current_sensor_c")
    # Per-phase active power
    await _register_sensor(
        var, config, CONF_ACTIVE_POWER_A, "set_active_power_sensor_a"
    )
    await _register_sensor(
        var, config, CONF_ACTIVE_POWER_B, "set_active_power_sensor_b"
    )
    await _register_sensor(
        var, config, CONF_ACTIVE_POWER_C, "set_active_power_sensor_c"
    )
    # Per-phase reactive power
    await _register_sensor(
        var, config, CONF_REACTIVE_POWER_A, "set_reactive_power_sensor_a"
    )
    await _register_sensor(
        var, config, CONF_REACTIVE_POWER_B, "set_reactive_power_sensor_b"
    )
    await _register_sensor(
        var, config, CONF_REACTIVE_POWER_C, "set_reactive_power_sensor_c"
    )
    # Per-phase apparent power
    await _register_sensor(
        var, config, CONF_APPARENT_POWER_A, "set_apparent_power_sensor_a"
    )
    await _register_sensor(
        var, config, CONF_APPARENT_POWER_B, "set_apparent_power_sensor_b"
    )
    await _register_sensor(
        var, config, CONF_APPARENT_POWER_C, "set_apparent_power_sensor_c"
    )
    # Per-phase power factor
    await _register_sensor(
        var, config, CONF_POWER_FACTOR_A, "set_power_factor_sensor_a"
    )
    await _register_sensor(
        var, config, CONF_POWER_FACTOR_B, "set_power_factor_sensor_b"
    )
    await _register_sensor(
        var, config, CONF_POWER_FACTOR_C, "set_power_factor_sensor_c"
    )
    # Per-phase active energy
    await _register_sensor(
        var, config, CONF_ACTIVE_ENERGY_A, "set_active_energy_sensor_a"
    )
    await _register_sensor(
        var, config, CONF_ACTIVE_ENERGY_B, "set_active_energy_sensor_b"
    )
    await _register_sensor(
        var, config, CONF_ACTIVE_ENERGY_C, "set_active_energy_sensor_c"
    )
    # Per-phase reactive energy
    await _register_sensor(
        var, config, CONF_REACTIVE_ENERGY_A, "set_reactive_energy_sensor_a"
    )
    await _register_sensor(
        var, config, CONF_REACTIVE_ENERGY_B, "set_reactive_energy_sensor_b"
    )
    await _register_sensor(
        var, config, CONF_REACTIVE_ENERGY_C, "set_reactive_energy_sensor_c"
    )
    # Per-phase apparent energy
    await _register_sensor(
        var, config, CONF_APPARENT_ENERGY_A, "set_apparent_energy_sensor_a"
    )
    await _register_sensor(
        var, config, CONF_APPARENT_ENERGY_B, "set_apparent_energy_sensor_b"
    )
    await _register_sensor(
        var, config, CONF_APPARENT_ENERGY_C, "set_apparent_energy_sensor_c"
    )
    # Combined sensors
    await _register_sensor(var, config, CONF_FREQUENCY, "set_frequency_sensor")
    await _register_sensor(
        var, config, CONF_TOTAL_ACTIVE_POWER, "set_total_active_power_sensor"
    )
    await _register_sensor(
        var, config, CONF_TOTAL_REACTIVE_POWER, "set_total_reactive_power_sensor"
    )
    await _register_sensor(
        var, config, CONF_TOTAL_APPARENT_POWER, "set_total_apparent_power_sensor"
    )
    await _register_sensor(
        var, config, CONF_TOTAL_POWER_FACTOR, "set_total_power_factor_sensor"
    )
    await _register_sensor(
        var, config, CONF_TOTAL_ACTIVE_ENERGY, "set_total_active_energy_sensor"
    )
    await _register_sensor(
        var, config, CONF_TOTAL_REACTIVE_ENERGY, "set_total_reactive_energy_sensor"
    )
    await _register_sensor(
        var, config, CONF_TOTAL_APPARENT_ENERGY, "set_total_apparent_energy_sensor"
    )
