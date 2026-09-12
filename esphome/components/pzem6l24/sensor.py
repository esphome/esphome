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


def _frequency_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_HERTZ,
        icon=ICON_CURRENT_AC,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_FREQUENCY,
        state_class=STATE_CLASS_MEASUREMENT,
    )


_PHASES = ("a", "b", "c")

# Quantities measured once per phase: (base name, schema factory). The config key is
# "<base name>_<phase>" and the setter is "set_<base name>_sensor_<phase>".
_PHASE_QUANTITIES = (
    ("voltage", _voltage_schema),
    ("current", _current_schema),
    ("active_power", _active_power_schema),
    ("reactive_power", _reactive_power_schema),
    ("apparent_power", _apparent_power_schema),
    ("power_factor", _power_factor_schema),
    ("active_energy", _active_energy_schema),
    ("reactive_energy", _reactive_energy_schema),
    ("apparent_energy", _apparent_energy_schema),
)

# Quantities the device reports once for the whole meter: (config key, schema factory).
# The setter is "set_<config key>_sensor".
_COMBINED_QUANTITIES = (
    (CONF_FREQUENCY, _frequency_schema),
    (CONF_TOTAL_ACTIVE_POWER, _active_power_schema),
    (CONF_TOTAL_REACTIVE_POWER, _reactive_power_schema),
    (CONF_TOTAL_APPARENT_POWER, _apparent_power_schema),
    (CONF_TOTAL_POWER_FACTOR, _power_factor_schema),
    (CONF_TOTAL_ACTIVE_ENERGY, _active_energy_schema),
    (CONF_TOTAL_REACTIVE_ENERGY, _reactive_energy_schema),
    (CONF_TOTAL_APPARENT_ENERGY, _apparent_energy_schema),
)

# The single source of truth for every sensor: (config key, schema, setter name).
_SENSORS = [
    *(
        (f"{name}_{phase}", schema_fn(), f"set_{name}_sensor_{phase}")
        for name, schema_fn in _PHASE_QUANTITIES
        for phase in _PHASES
    ),
    *(
        (key, schema_fn(), f"set_{key}_sensor")
        for key, schema_fn in _COMBINED_QUANTITIES
    ),
]

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PZEM6L24),
            **{cv.Optional(key): schema for key, schema, _ in _SENSORS},
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


def _final_validate(config: ConfigType) -> ConfigType:
    return modbus.final_validate_modbus_device("pzem6l24", role="client")(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_client_device(var, config)

    for key, _, setter in _SENSORS:
        if (conf := config.get(key)) is not None:
            sens = await sensor.new_sensor(conf)
            cg.add(getattr(var, setter)(sens))
